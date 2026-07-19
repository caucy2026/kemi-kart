# STK 双屏异显 — 版本记录

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
