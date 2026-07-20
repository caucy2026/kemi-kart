# STK 双屏异显 — 版本记录

## v1.6 (2026-07-20) — 双屏大厅完整流程与 Deferred RTT 黑块根治

> Android package: `version_name 1.5 -> 1.6`, `version_code 1 -> 2`.
> Target: RK356x / Mali-G52 / Android arm64-v8a, Display 0 + Display 2.

### 用户可见改动

- **双屏选车改为明确确认流程**：点击 kart 只更新 3D 预览与属性；只有点击 Continue 才锁定该玩家，消除了浏览 kart 时被自动确认的问题。
- **默认选车信息可见**：D0 进入选车时立即布局模型、属性和 kart 列表，不再出现角色模型及车辆信息空白的首帧。
- **双方独立等待状态**：P0 确认后 D0 显示等待 P1；P1 确认后 D2 显示等待主屏。等待画面会隐藏另一个屏幕的模型和 UI，避免残留叠加。
- **P0 选图、P1 等待、同时开赛**：两名玩家确认 kart 后保持 `MENU` 状态，D0 可正常浏览和确认地图，D2 固定显示等待页。P0 确认赛道后才创建共享 `World`，两个屏幕一同进入标准 3-2-1 双人比赛。
- **D2 全屏显示**：外接屏 Presentation 使用 immersive fullscreen，隐藏系统状态栏和导航栏，避免内容被系统 UI 挤出屏幕。
- **3D kart 预览与比赛小地图黑色方块修复**：消除选中 kart 时背景的黑色闪烁方块；同一 deferred RTT 路径的小地图同步修复。

### 关键实现

| 范围 | 文件 | 改动 |
|---|---|---|
| 双屏大厅状态机 | `src/states_screens/kart_selection.cpp/.hpp` | 新增 P0 正在选图状态；移除 kart hover 自动确认；在双方确认后初始化两人比赛参数并转入地图页；按显示器同步 kart、地图和等待 UI。 |
| 地图确认 | `src/states_screens/tracks_and_gp_screen.cpp` | 仅允许 D0 确认并调用标准 `startSingleRace`；D2 等待画面不接受地图控件。 |
| UI 布局 | `data/gui/screens/karts.stkgui`, `data/gui/screens/tracks_and_gp.stkgui` | 添加 P0/P1 等待标签与可整体隐藏的 kart/map 容器。 |
| 逐屏 GUI | `src/guiengine/engine.*`, `screen.*`, `event_handler.cpp`, `shader_based_renderer.cpp` | 每个 D0/D2 render pass 使用正确 display ID 与 widget 可见性；双屏布局改用立即生效的 `updateSizeNow`，避免两 surface 交替绘制时的动画布局错位。 |
| Android 壳 | `android/src/main/java/SuperTuxKartActivity.java`, `android/src/main/java/org/libsdl/app/DualScreenPresentation.java` | 补齐原生 edit-box JNI 回调 stub；D2 Presentation 开启沉浸式全屏。 |
| Deferred renderer | `src/graphics/shader_based_renderer.cpp` | combine pass 读取 depth-stencil 前临时解除 `FBO_COLORS` 的 `GL_DEPTH_STENCIL_ATTACHMENT`，完成后立刻重新挂回。 |

### Deferred RTT 黑块：根因分析与解决过程

#### 现象与控制组

- 问题在单屏版本已经存在，双屏仅让预览更容易观察到；因此首先排除双屏布局、ribbon 背景和 kart 资源本身。
- Wilber、Amanda 的不透明材质预览会出现随帧闪烁的黑色方块；Hexley、Puffy 含 `alphablend` 透明 pass 时背景较干净。这个差异用于定位 renderer pass，不作为资源损坏结论。
- 同类方块同时出现在比赛小地图，说明问题位于 `ModelViewWidget` 和 `Graph::makeMiniMap` 共用的 `RenderTarget::renderToTexture` / deferred RTT 流程。

#### 已验证并排除的方向

| 假设 | 实验结果 | 结论 |
|---|---|---|
| 选车 UI、阴影或双屏合成 | 单屏也复现；更换 UI/alpha 合成状态没有改变截图 | 排除。 |
| GL1 RTT、RGBA/BGRA/float 纹理格式 | 设备运行 `ShaderBasedRenderer` + `GL3RenderTarget`，Mali 使用 `GL_RGBA8` | 排除。 |
| UI quad blend、scissor、depth mask 或 tone mapping | 组合输出前后及 UI 最终合成都检查过；强制不走 post-processing 仍有方块 | 排除。 |
| forward 渲染 | 强制 forward RTT 后方块消失 | 问题确定在 deferred 渲染阶段，但 forward 仅作诊断，不作为最终降级方案。 |
| 深度清除 API 或残留 write mask | `glClearBufferfi` 与传统 depth/stencil clear 均不能改变条纹 | 不是单纯 clear API 或上一帧 mask 泄漏。 |

#### 确认根因

1. 为 `combine_diffuse_color.frag` 临时输出 depth 灰度图。注意 Android 首次运行会把 APK assets 解压到外部存储，修改 `data/shaders` 或仅重新安装 APK 不会覆盖已解压文件；诊断 shader 必须同步到：
	 `/storage/emulated/0/Android/data/org.supertuxkart.stk/files/SuperTuxKart/data/shaders/`。
2. 实机 depth 图中黑白条纹与最终黑色方块一一对应，证明错误数据来自 deferred depth-stencil 采样，而不是颜色合成。
3. `RTT` 将同一张 `GL_DEPTH24_STENCIL8` 纹理同时挂给 `FBO_SP` 和 `FBO_COLORS`。combine 阶段在 `FBO_COLORS` 仍附着该纹理时，把它作为 `depth_stencil` sampler 读取。
4. 该 read/write feedback loop 在 GLES 中是未定义行为；Mali-G52 返回按 tile 分布的错误深度值，combine 将其视为几何深度，产生黑色方块。
5. 诊断性地在 combine 前解除 depth-stencil attachment 后，depth 灰度图的条纹完全消失，直接证实 feedback loop。恢复正常 fragment shader 后，Wilber 预览背景连续干净，小地图同路径不再出现方块。

#### 最终修复

```cpp
m_rtts->getFBO(FBO_COLORS).bind();
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
		GL_TEXTURE_2D, 0, 0);
CombineDiffuseColor::getInstance()->render(...,
		m_rtts->getDepthStencilTexture(), ...);
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
		GL_TEXTURE_2D, m_rtts->getDepthStencilTexture(), 0);
```

只在全屏 combine shader 采样 depth 的窗口解除 attachment；后续 skybox、透明材质、粒子和后处理恢复使用原 depth-stencil，不改变图形质量或强制 forward renderer。

### 实机验证

- `arm64-v8a` NDK 编译和 `./gradlew assembleDebug` 成功。
- APK 已在 `192.168.3.54:5555` 安装、启动，运行 `org.supertuxkart.stk/.SuperTuxKartActivity`。
- Wilber 不透明预览：修复后背景稳定，无黑色 tile。
- Hexley/Puffy 透明材质控制组保持正常。
- 比赛小地图：同一 shared deferred RTT 路径完成回归检查，无黑色 tile。
- P0/P1 选车、等待、P0 选图、两屏共同进入比赛流程已完成实机验证。

### 后续注意事项

1. **禁止在附着到当前 draw framebuffer 的纹理上采样。** 新增 fullscreen pass 时，要检查 color、depth 和 stencil attachment 是否同时被作为 sampler 使用；必要时先解绑或输出到独立 FBO。
2. **Android shader 资产有两份生命周期。** 维护源文件时保持 `data/shaders` 与 `android/assets/data/shaders` 一致；测试已运行设备时，重新安装不保证覆盖外部存储的解压资产。应删除 `.extracted` 触发完整解压，或明确推送改动资源后再测试。
3. **不要用 forward RTT 作为修复。** 它只能证明 deferred 分支有问题，会降低预览和小地图的真实渲染路径覆盖。
4. **双屏菜单期间不要创建 `World`。** P0 选图阶段必须保留 `MENU`；提前进入 `GAME` 会让 D2 渲染赛道并与菜单控件叠加，历史上还导致 SDLThread 崩溃。
5. **交替 D0/D2 render pass 内的布局应立即生效。** 选车模型移动继续使用 `updateSizeNow`；异步动画 `move` 容易让一个 display 看到过渡布局。
6. **回归测试必须覆盖单屏和双屏。** 每次调整 RTT/FBO，至少检查 Wilber/Amanda、Hexley/Puffy、小地图，以及 D0/D2 两个 display 的截图。

### 构建与部署命令

```bash
cd android
/Users/newlink/android-sdk/ndk/26.1.10909125/ndk-build \
	NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=Android.mk APP_ABI=arm64-v8a \
	APP_PLATFORM=android-24 APP_STL=c++_static PROJECT_VERSION=1.6 \
	PACKAGE_NAME=org.supertuxkart.stk APP_DIR_NAME=SuperTuxKart \
	PACKAGE_CLASS_NAME=org/supertuxkart/stk/SuperTuxKartActivity -j4
./gradlew assembleDebug
adb -s 192.168.3.54:5555 install -r build/outputs/apk/debug/android-debug.apk
adb -s 192.168.3.54:5555 shell am start -n \
	org.supertuxkart.stk/.SuperTuxKartActivity
```

---

## v1.1.0 (2026-07-19) — 统一触控架构 & 两屏独立操控

### 架构变更（重大重构）
- **统一触控链路**：副屏触摸改走 SDL 标准路径（删除 ~250 行手工 JNI 触控代码）
- **SDL 双设备检测**：`SDLSurface.java` 用 `mDisplayId` 作为固定 `routedDevId`（D0=0, D2=2）
- **Irrlicht 事件扩展**：`STouchInput` 新增 `DeviceID` 字段，`CIrrDeviceSDL` 传递 `SDL_event.tfinger.touchId`
- **STK 设备分流**：`InputManager` 按 `DeviceID` 路由到独立 `MultitouchDevice`
- **双 MultitouchDevice**：`DeviceManager` 新增 `m_multitouch_device_2` → Player 1
- **双 GUI 状态**：`RaceGUIMultitouch::draw()` 按 `kart->getWorldKartId()` 选择对应 device，按钮自动克隆

### 功能
- **两屏完全独立操控**：Display 0 → P0, Display 2 → P1，方向盘/道具/后视镜各自独立
- **视觉完全独立**：每个 MultitouchDevice 有独立 `button->pressed`/`axis_x`/`axis_y`
- **双屏完整 HUD**：minimap、timer、player list、player icons 全部渲染到两屏
- **方向盘按角色独立**：改用 `kart->getControls().getSteer()` 渲染旋转

### 修改
| 类型 | 文件 | 说明 |
|------|------|------|
| 新增 | `IEventReceiver.h:DeviceID` | `STouchInput` 加设备 ID 字段 |
| 修改 | `CIrrDeviceSDL.cpp` | 传递 `touchId` 到 Irrlicht 事件 |
| 修改 | `input_manager.cpp` | 按 DeviceID 路由到不同 MultitouchDevice |
| 修改 | `device_manager.cpp/.hpp` | 新增 `m_multitouch_device_2` + 生命周期 |
| 修改 | `race_gui_multitouch.cpp/.hpp` | draw() 按 kart 选 device；按钮克隆 |
| 修改 | `race_gui.cpp` | drawGlobalMiniMap/drawGlobalTimer 改 public |
| 修改 | `shader_based_renderer.cpp` | 副屏渲染 minimap+timer+icons |
| 修改 | `SDLSurface.java` | routedDevId=mDisplayId，删除 mDisplayId==2 分支 |
| 删除 | `android_native_dual_screen.cpp` | nativeTouchDisplay2/g_touch2_*/dualScreenApplyTouch (~150行) |
| 删除 | `main.cpp` | dualScreenControlPlayer2 (~60行) |
| 删除 | `SDLActivity.java` | nativeTouchDisplay2 声明 |
| 删除 | `race_gui_multitouch.cpp` | P1 按钮高亮隔离（不再需要） |

### 踩坑记录
| 问题 | 根因 | 解决 |
|------|------|------|
| 副屏方向盘不显示 | `getNumLocalPlayers()==1` 限制 | `race_gui.cpp` 增加 `\|\| g_dual_screen_mode` |
| 方向盘视觉同步 | 共享 `button->axis_x` | 改用 `kart->getSteer()` |
| 副屏道具按钮无效 | JNI 触控坐标硬编码 1920x1280，实为 1205 | 动态读取 `g_secondWidth/Height` |
| 松手不释放(LOOK_BACK卡住) | 只发 MAX_VALUE 不发 0 | `s_prev_item_action` 追踪，自动释放 |
| 变量遮蔽导致操控无效 | 内层重声明 `float steer/accel` | 删除内层声明 |
| 角色反转(副屏控主屏) | device ID "先到先得"随机映射 | `routedDevId=mDisplayId` 固定映射 |
| **根本架构缺陷** | 两套触控路径(SDL vs JNI)永远不一致 | 统一到 SDL→Irrlicht→STK 标准路径 |

### 经验教训
1. **不要手工复刻框架逻辑**：MultitouchDevice 的 press/release/zone 检测极其复杂，手工 JNI 版本 100% 会有 bug
2. **硬编码坐标必然出错**：不同分辨率的显示屏需要动态计算
3. **变量遮蔽是 C++ 经典陷阱**：外层变量被内层同名声明覆盖
4. **统一代码路径 > 重复实现**：删 250 行手工代码，换 4 行核心改动，收益巨大

---

## v1.1.1 (2026-07-19) — Kart 选择界面双屏独立触控

### 功能
- **独立的 kart 选择**：D0 触摸只改 P0 的 kart，D2 触摸只改 P1 的 kart
- **独立的 3D 模型更新**：每个屏显示自己玩家的 kart 模型
- **独立的 ribbon 高亮**：kart 图标选中高亮只在对应屏幕上显示

### 修复 (3 处核心改动)

| # | 文件 | 问题 | 修复 |
|---|------|------|------|
| 1 | `event_handler.cpp` | `setFocusForPlayer` 调用太晚，`getSelectedRibbon` 找不到 focus → P1 回调永不触发 | `setFocusForPlayer` 移到 `m_event_handler->mouseHovered` 之前 |
| 2 | `event_handler.cpp` | 内部 GUI 事件(DeviceID=0)每帧涌入污染 `m_last_touch_device` → 交叉污染 | 增加 `m_last_touch_device` 与 `curDisp` 匹配校验 |
| 3 | `skin.cpp` | ribbon 渲染 3 处硬编码 `PLAYER_ID_GAME_MASTER`(0) → 两屏高亮同步 | 改用 `curDisp` 映射 D0→P0, D2→P1 |

### 踩坑记录
| 问题 | 根因 | 解决 |
|------|------|------|
| P1 模型不更新 | `setFocusForPlayer` 在 `getSelectedRibbon` 之后调用 → 返回NULL | 调换顺序（修复1） |
| D2 触摸导致 P0 变化 | 内部 GUI 事件 DeviceID=0 触发 ribbon hover | DeviceID-curDisp 匹配过滤（修复2） |
| 两屏 kart 图标高亮相同 | skin.cpp 固定查 P0 的选中状态 | 按 curDisp 动态选 playerID（修复3） |
| `input -d 2 tap` 不稳定 | tap 是瞬间事件，50% 概率落在错误交替帧 | 改用 swipe 持续事件 |
| `extern bool g_dual_screen_mode;` linker error | namespace 内声明变成 `GUIEngine::xxx` | 文件头部全局作用域声明 |
| 过滤用 `break` 实际跳出 switch | C++ case 中的 break 作用于 switch 而非 if | 改用 `skip` 标志变量 |

### 闭环调试方法
```bash
# 双屏截图
adb shell screencap -d 0 -p /sdcard/d0.png  # 主屏
adb shell screencap -d 2 -p /sdcard/d2.png  # 副屏
# 模拟触摸
adb shell input -d 2 swipe 100 900 1600 900 2000  # D2副屏
adb shell input swipe 100 900 1600 900 2000       # D0主屏
# 日志验证
adb logcat -d | grep "KartHover:"    # player_id 是否正确
adb logcat -d | grep "ribbonHover:"  # lastTouchDev + curDisp
adb logcat -d | grep "Touch dev="    # DeviceID 来源
```

---

## v1.0.0 (2026-07-19) — 双屏对战首版

### 功能
- **双屏帧镜像**：Display 0 画面实时拷贝到 Display 2，菜单和比赛均生效
- **双人自动开赛**：启动即进入 2 人对战，跳过选单，默认赛车+随机赛道
- **Android Presentation API**：Display 2 使用独立 SurfaceView，共享 EGL Context
- **GLES2 兼容镜像**：`glCopyTexSubImage2D` + 全屏 quad（无 glBlitFramebuffer 依赖）
- **完整 APK 构建**：Gradle assembleDebug，145MB，含完整 karts/tracks/assets

### 修改
| 类型 | 文件 | 说明 |
|------|------|------|
| 新增 | `DualScreenPresentation.java` | Display 2 Presentation 容器 |
| 新增 | `android_native_dual_screen.cpp/.h` | EGL 管理 + JNI + 帧镜像 |
| 新增 | `SuperTuxKartActivity.java` | 最小化 STK Activity |
| 修改 | `SDLActivity.java` | +initDualScreen + onSDLRenderingReady |
| 修改 | `SDLSurface.java` | +mDisplayId 路由 |
| 修改 | `Android.mk` | +dual_screen.cpp +EGL/GLESv2/android |
| 修改 | `build.gradle` | 恢复 externalNativeBuild + 完整 ndk args |
| 修改 | `irr_driver.cpp` | 镜像调用 + JNI 回调 |
| 修改 | `shader_based_renderer.cpp` | 比赛路径镜像 |
| 修改 | `main.cpp` | +startDualScreenRace() |
| 修改 | `main_android.cpp` | ExceptionClear |
| 修改 | `assets_android.cpp` | ExceptionClear |
| 修改 | `CIrrDeviceAndroid.cpp` | ExceptionClear |

### 设备
- 目标：192.168.1.142:5555 (huanglong 平板)
- Display 0: 1920×1280 内置, Display 2: 1920×1280 外接
- GPU: Mali-G52, OpenGL ES 3.2
- 系统：Android, API 24+, arm64-v8a

### 已知限制
- 两屏显示相同画面（镜像），非独立视角
- 触控未分离，两边共享同一输入
- 无选单流程，自动使用默认赛车

---

## 格式说明
- **新增**：新文件/新功能
- **修改**：已有文件改动
- **修复**：Bug 修复
- **设备**：目标硬件信息
