# STK Android 双屏性能分析与优化记录

> 设备：Android 12，Mali-G52，Display 0 + Display 2（各 1920×1280）  
> 图形架构：单进程、单 EGLContext、两个 EGLSurface；Camera 0 和 Camera 1 依次向同一 GL 命令流提交。  
> 日期：2026-07-23

## 1. 目标与约束

本轮目标是解释并降低双屏比赛中的持续卡顿，同时满足以下约束：

- 不降低纹理、模型、特效、阴影、后处理或渲染分辨率等画质设置。
- 结论必须来自固定场景的真机日志和双屏截图，不能由 CPU 总占用率推断。
- 临时诊断探针在完成定位后移除，最终代码只保留低开销的一秒聚合统计。
- 两个屏幕必须继续显示独立相机，车辆骨骼、粒子、HUD、赛道和道具均不能损坏。

本轮没有修改已有设备配置。设备原有配置中的 RTT 比例、纹理上限、阴影和后处理状态保持不变，因此结果只能与同一设备、同一配置、同一测试场景比较。

## 2. 可重复测试入口

Android Activity 支持通过 Intent extra 启动固定性能场景：

```bash
adb -s 192.168.3.54:5555 shell am force-stop org.supertuxkart.stk
adb -s 192.168.3.54:5555 logcat -c
adb -s 192.168.3.54:5555 shell am start \
  --ez perf_test true \
  -n org.supertuxkart.stk/.SuperTuxKartActivity
```

`perf_test=true` 在 Java 层转换为原生参数 `--dual-screen-perf-test`。原生启动固定场景：

| 参数 | 固定值 |
|---|---|
| 赛道 | `abyss` |
| 本地玩家 | 2 |
| AI | 4 |
| 圈数 | 100 |

性能测试模式会跳过通用 quickstart，避免同一进程重复创建 World 或重复启动比赛。最初尝试的 `black_forest` 在当前资源集上加载崩溃，因此固定场景改为可稳定加载的 `abyss`。

## 3. 统计口径

`MainLoop` 使用 `std::chrono::steady_clock` 记录 wall time，使用 `CLOCK_THREAD_CPUTIME_ID` 记录当前线程 CPU time。所有数据按约一秒窗口聚合后输出，避免逐帧日志本身改变调度和帧率。

### 3.1 日志字段

`DualPerf`：

- `frames`：统计窗口内完成的帧数。
- `frame`：主循环平均帧时间。
- `sim`：模拟更新平均时间。
- `render`：完整渲染平均时间。
- `cam0` / `cam1`：两台相机平均渲染时间。
- `swap0` / `swap2`：两个 EGLSurface 的平均 swap 时间。

`DualPerfRender`：

- `pre` / `precpu`：`IrrDriver::update()` 进入 renderer 前的 wall/CPU 时间。
- `renderer`：`ShaderBasedRenderer::render()` 时间。
- `post`：renderer 返回后的时间。
- `make2`：切换到 Display 2 EGLSurface 的时间。
- `fence0` / `fence1`：每台相机等待自身上一帧动态资源的时间。
- `hud2` / `hud0`：两个屏幕 HUD 绘制时间。
- `restore`：从 Display 2 恢复主 EGLSurface 的时间。

HUD 在两个屏幕实时显示：FPS、进程 CPU、GPU busy、RSS 内存、frame/sim/render、cam0/cam1 和 swap0/swap2。GPU 百分比来自设备暴露的 Mali devfreq busy 信息；设备节点不可读时显示不可用值，而不是伪造数据。

## 4. 根因一：全局 fence 把两台相机串行化

### 4.1 基线证据

优化前的代表性数据：

| 指标 | 基线 |
|---|---:|
| renderer | 约 40.35 ms |
| Camera 1 fence | 约 17.63 ms |
| 实际帧率 | 约 17–20 FPS |

Display 2 的 `eglMakeCurrent` 和两个 surface 的 swap 时间都远小于 Camera 1 的 fence 等待，因此持续低帧率的主因不是副屏切换或 swap。

### 4.2 原因

原实现只有一个全局 `GLsync`。Camera 0 提交本相机使用的动态资源后创建 fence；Camera 1 随后进入同一 GL 命令流时等待这个 fence。结果是 Camera 1 每帧都等待 Camera 0 的 GPU 工作完成，两个本应连续提交的相机被 CPU 端强制拆成串行阶段。

不能简单删除 fence。以下动态资源原来会在两台相机之间复用：

- CPU 粒子的 VAO/VBO。
- Android 非 TBO 骨骼矩阵纹理。
- 动态文字 billboard 的实例 buffer 数据 store。

若只删除同步而不隔离资源，Camera 1 可能覆盖 Camera 0 尚未消费的数据，造成粒子错乱、骨骼模型损坏或文字实例读取竞态。

### 4.3 修复

1. `DrawCalls::m_sync` 改为 `m_sync[MAX_PLAYER_COUNT]`。
2. 每台相机只等待并替换自己的 fence，即 Camera 0 等 Camera 0 上一帧，Camera 1 等 Camera 1 上一帧。
3. CPU 粒子 GPU 资源改为每玩家一组 VAO/VBO map；上传和 draw 都按 `SP::sp_cur_player` 选择。
4. Android 骨骼矩阵纹理改为每玩家一张；分配、上传、shader 预填充句柄和销毁均使用相同玩家索引。
5. 动态文字 billboard 更新时使用 `glBufferData(..., GL_DYNAMIC_DRAW)` 建立新的 data store，避免覆盖旧 store 仍在使用的 36 字节实例数据。
6. `DrawCalls` 析构时删除仍存在的每玩家 fence，完整覆盖资源生命周期。

CPU 粒子的 queue 和 generated vector 仍是相机内 scratch data：`DrawCalls::prepareDrawCalls()` 在每台相机开始时调用 `CPUParticleManager::reset()` 清空并重新生成。需要跨相机隔离的是可能仍被 GPU 消费的 VAO/VBO，而不是这些已经按相机重建的 CPU 容器。

骨骼纹理绑定顺序也经过复核：`SP::uploadAll()` 先上传当前玩家矩阵并设置 `sp_prefilled_tex[0]`，之后 draw 才由 `SPShader` 读取该句柄并调用 `glBindTexture`，不存在先 draw 后切换纹理的问题。

## 5. 根因二：尺寸未变的 orientation 抖动触发全量重建

真机日志显示 Android/SDL 的 orientation 枚举会在同一方向轴的正反状态之间反复变化，而两个显示器的实际渲染尺寸始终不变。旧路径把每次枚举变化都当成真实 resize，调用 `resizeWindow()`，导致窗口、RTT 和相关图形资源周期性重建。

优化前观测到的单次 driver pre 尖峰为 14–77 ms，表现为肉眼可见的周期性停顿。

修复条件严格限定为：

- 已启用双屏模式；
- `m_actual_screen_size`、当前 screen size 和新的 render-target size 完全相等；
- orientation 仅在同一轴的正反枚举间变化：landscape 与 landscape-flipped，或 portrait 与 portrait-flipped。

满足时只更新缓存 orientation，不调用 `resizeWindow()`。横竖轴切换或任何实际尺寸变化仍走原 resize 路径。该判断不依赖厂商把固定横屏报告为哪个枚举值。

修复后连续 30 个统计窗口中，driver pre 最大值约 0.03 ms，未再出现 14–77 ms 的重建尖峰。

## 6. 最终真机结果

最终 APK 重新编译、覆盖安装并冷启动固定测试场景后，稳态代表性窗口为：

```text
DualPerf: frames=30 frame=33.89ms sim=9.59ms render=23.27ms
          cam0=7.33ms cam1=8.34ms swap0=1.37ms swap2=1.54ms
DualPerfRender: pre=0.02ms renderer=23.24ms make2=0.62ms
                fence0=0.02ms fence1=0.02ms

DualPerf: frames=33 frame=30.83ms sim=7.89ms render=22.10ms
          cam0=6.77ms cam1=7.70ms swap0=1.38ms swap2=1.64ms
DualPerfRender: pre=0.03ms renderer=22.07ms make2=0.47ms
                fence0=0.01ms fence1=0.02ms
```

长窗口内 renderer 常见约 20–23 ms，Camera 1 fence 常见约 0.01–0.03 ms。相比基线，主要跨相机 GPU 等待已消除，renderer 时间约减半。

当前帧率常见约 26–33 FPS，没有达到 60 FPS。剩余波动主要出现在 simulation（部分窗口约 10–18 ms）和两个相机的实际 GPU 工作；单 EGLContext 下两台相机仍按顺序提交命令，这一架构限制没有被本轮修改掩盖。

## 6.1 稳定 30 Hz 补充优化

继续核对真实编译命令后发现，原 Debug APK 中只有 `main` 模块在自己的
`LOCAL_CFLAGS/LOCAL_CPPFLAGS` 末尾追加了 `-O3 -mcpu=cortex-a73`；Bullet、
Irrlicht、graphics_engine 和 SDL 等 ndk-build 模块仍由 Gradle 以 `-O0` 编译。
真机 `/proc/cpuinfo` 确认 8 个核心全部为 ARM implementer `0x41`、part
`0xd09`，即 Cortex-A73，因此为实际部署的 arm64 Debug 构建统一加入：

```text
-O3 -mcpu=cortex-a73 -fomit-frame-pointer
```

`-mcpu` 只在 `compile_arch=arm64-v8a` 时加入，避免未来 x86 或 all-ABI 构建
误用 ARM 参数；`-O3` 和 `-fomit-frame-pointer` 仍适用于所有 Debug ABI。

真实命令验证结果：

- 修改前 Bullet/Irrlicht 命令以 `-O0` 结束，没有 A73 参数。
- 修改后 AGP `build_model.json` 和 ndk-build invocation 均记录全局
  `APP_CFLAGS/APP_CPPFLAGS=-O3 -mcpu=cortex-a73 -fomit-frame-pointer`。
- `main` 模块继续在命令末尾使用 `-O3 -mcpu=cortex-a73 -flto=thin`，ThinLTO
  仍仅覆盖 `main` 自己编译的目标，不应宣称覆盖所有预编译依赖库。

全模块优化后，固定场景的 simulation 从常见 13–18 ms 降到约 2–4 ms，
节流前单帧工作量降到约 24–29 ms，具备满足 33.3 ms 截止时间的余量。

同时确认原有 Android `m_max_fps=30` 不能保证生效：设备配置文件保存的是
`max_fps=120`，且 `swap_interval=1` 时主循环会跳过软件 limiter。最终修复为：

- 双屏模式有效上限固定为 30 FPS，不受持久化的 `max_fps=120` 覆盖。
- 双屏模式即使开启 vsync，也执行软件 limiter 补足到 33.3 ms。
- 单屏、录像和非 Android 行为保持原路径。

最近 30 个连续一秒窗口统计：

```text
frames average = 30.10, min = 30, max = 31
work average   = 25.46 ms, min = 23.79 ms, max = 28.94 ms
simulation avg = 3.79 ms
renderer avg   = 21.00 ms
```

这里日志中的 `frame/work` 在 limiter sleep 之前结束，因此约 25.46 ms 表示真实
工作量，剩余约 7.9 ms 平均余量；30/31 个 frame 的一秒窗口边界抖动不代表运行
速率超过 30 Hz。

## 6.2 Mali-G52 特性与实际使用状态

真机驱动为 `Mali-G52, OpenGL ES 3.2 v1.r35p0`。GPU devfreq：200–900 MHz，
governor 为 `bifrost`，比赛期间观测到 600–700 MHz。编译参数不会设置 GPU
频率；频率由内核 governor 动态管理。

| 特性 | 硬件/驱动 | 项目实际状态 | 当前收益 |
|---|---|---|---|
| GLES 3.2 | 支持并已使用 | 运行日志确认 ARM Mali-G52 GLES 3.2 | 渲染基础能力 |
| `GL_ARM_mali_shader_binary` / program binary | 驱动声明支持 | 扩展枚举器能识别并加载入口，但项目没有 `glProgramBinary`/`glGetProgramBinary` 缓存调用 | 未启用，不能计入当前帧率收益 |
| ASTC LDR/HDR | 驱动声明支持 | 扩展可识别，但 APK 中没有 `.astc/.ktx/.ktx2` 纹理；链接的 astcenc 只被 Vulkan 压缩路径引用，当前是 GLES 双屏路径 | 未用于当前赛道纹理采样 |
| `EGL_IMG_context_priority` | SurfaceFlinger 报告支持 | 项目 EGL context 创建路径未请求 priority attribute | 未启用 |
| `EGL_KHR_partial_update` / swap with damage | SurfaceFlinger 报告支持 | 项目没有 partial-update/swap-damage 调用；比赛每帧重绘完整 3D 画面 | 未启用，且本场景预期收益有限 |
| NEON/ASIMD | 8 个 Cortex-A73 核心均支持 | arm64 编译目标和 A73 codegen 已实际进入命令；OpenAL 运行日志也报告 NEON | 已启用 |
| ThinLTO | 编译器支持 | `main` 模块编译和链接命令含 `-flto=thin` | 仅 main 模块有效 |

不建议为了“参数更多”盲目启用 Mali shader binary：binary 与驱动版本耦合，主要
改善 shader 冷启动编译，不会降低稳态 draw cost。EGL partial update 更适合局部
变化的 2D UI；当前双屏比赛是全屏 3D 重绘，不是稳定 30 Hz 的首要手段。

## 7. 画面与稳定性验证

最终构建的验证项：

- D0 和 D2 截图显示两个不同玩家、两个独立相机。
- 车辆和骨骼动画正常。
- CPU 粒子和碰撞特效正常。
- 赛道、道具、HUD、小地图和触控控件正常。
- 进程持续存活。
- 聚焦日志中无 `SIGSEGV`、`Fatal signal`、`EGL_BAD*` 或 `GL_INVALID*`。
- `./gradlew assembleDebug` 成功。
- `git diff --check` 通过。

## 8. 代码审查结论

本轮对高风险点做了额外复核：

- `glDeleteSync`：Khronos 规范允许删除尚未完成的 sync；对象会标记为待删除，直到 fence 不再关联或等待结束后才真正释放。删除 sync 不会取消已提交的 GPU 命令。
- `glBufferData`：Khronos GLES 规范定义它为当前 buffer 创建新的 data store，并替换旧 store；此处用于避免动态文字覆盖仍在使用的数据。
- 玩家索引：`SP::sp_cur_player` 由相机索引设置，数组容量为 `MAX_PLAYER_COUNT=8`，随后恢复为 0；当前测试只有两台相机。
- 资源销毁：粒子 VAO/VBO、骨骼纹理和每玩家 fence 都有对应销毁路径。
- resize：只有实际尺寸未变且 orientation 未跨轴时才抑制，真实横竖切换和分辨率变化不受影响。

临时 GPU timer query 探针已从最终代码移除。第一次 query 方案在该设备的 core query 入口上崩溃，且 timer query 会扰动 tile-based mobile GPU，因此只保留低开销 CPU 侧聚合统计。

## 9. 改动文件

| 文件 | 最终用途 |
|---|---|
| `android/src/main/java/SuperTuxKartActivity.java` | 把 `perf_test` Intent extra 转为原生 CLI 参数。 |
| `src/main.cpp` | 解析性能测试参数并启动固定 2P + 4AI + 100 圈 `abyss`；避免重复 quickstart。 |
| `src/main_loop.hpp/.cpp` | 一秒聚合主循环、CPU、renderer、相机、fence、HUD、surface 和 swap 时序。 |
| `android/build.gradle` | Debug 全 native 模块使用 O3；arm64-v8a 使用已验证的 Cortex-A73 codegen。 |
| `src/main_loop.cpp` | 双屏模式固定 30 FPS，并在 vsync 开启时仍执行软件 pacing。 |
| `src/graphics/draw_calls.hpp/.cpp` | 每玩家 fence、fence 等待统计和完整销毁。 |
| `src/graphics/cpu_particle_manager.hpp/.cpp` | 每玩家粒子 VAO/VBO map。 |
| `src/graphics/sp/sp_base.cpp` | Android 每玩家骨骼矩阵纹理。 |
| `src/graphics/stk_text_billboard.hpp` | 动态文字实例 data store 替换。 |
| `src/graphics/irr_driver.cpp` | driver pre/renderer/post 统计；过滤同尺寸、同方向轴的 orientation 抖动。 |
| `src/graphics/shader_based_renderer.cpp` | 两相机、surface 切换、HUD、restore 和 swap 分段统计。 |
| `src/states_screens/race_gui_base.cpp` | 两屏实时 FPS/CPU/GPU/RSS 和时序 HUD。 |

## 10. 后续优化边界

下一步若继续追求更高帧率，应先在同一固定测试入口下细分 simulation 的 10–18 ms 波动，并统计每相机真实 GPU pass 成本。不要通过 CPU 八核总占用率判断 GPU 或主线程瓶颈，也不要恢复全局 fence 或在未隔离资源时直接删除同步。
