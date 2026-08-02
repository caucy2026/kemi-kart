# 单屏游戏改双屏异显 — 实战移植手册

> 基于 SuperTuxKart (STK) 开源卡丁车游戏的完整双屏移植实战。
> 设备：RK356x / Mali-G52 / Android 12 / 双屏 1920×1280 (Display 0 + Display 2)。
> 覆盖从零到可玩双人对战的全部阶段，包含可迁移到其他游戏引擎的通用方法论。

---

## 目录

1. [概述与适用场景](#1-概述与适用场景)
2. [总架构决策](#2-总架构决策)
3. [阶段 0：帧镜像（最小改动验证渲染链路）](#3-阶段-0帧镜像最小改动验证渲染链路)
4. [阶段 1：独立相机视角](#4-阶段-1独立相机视角)
5. [阶段 2：触控分离](#5-阶段-2触控分离)
6. [阶段 3：双屏选单流程](#6-阶段-3双屏选单流程)
7. [阶段 4：副屏物理方向固定](#7-阶段-4副屏物理方向固定)
8. [阶段 5：性能优化到稳定帧率](#8-阶段-5性能优化到稳定帧率)
9. [阶段 6：体验完善（自动驾驶、HUD、加载动画）](#9-阶段-6体验完善自动驾驶hud加载动画)
10. [通用踩坑清单](#10-通用踩坑清单)
11. [可迁移到其他引擎的最小模板](#11-可迁移到其他引擎的最小模板)

---

## 1. 概述与适用场景

### 1.1 目标

将一款单屏游戏改造为**双屏异显双人对战**：两个玩家各看一个屏幕、独立操作、同场竞技。

### 1.2 适用条件

| 条件 | 说明 |
|------|------|
| 游戏引擎 | 有分屏多人模式基础代码（多 Camera + 多 Player） |
| 图形 API | OpenGL ES / Vulkan，支持多 EGL Surface |
| Android 版本 | API 29+ (SurfaceControl) 或 API 26+ (基本双屏) |
| 硬件 | 芯片有双显输出（Display 0 内置 + Display 2 外接） |
| 触控 | 多指触控，最好系统能区分不同 Display 的触摸事件 |

### 1.3 本项目数据

| 指标 | 数值 |
|------|------|
| 总改动文件数 | ~30+ |
| 新增文件 | ~6 (DualScreenActivity, LoadingCircleView, android_native_dual_screen 等) |
| 纯代码改动量 | ~1500 行 Java + ~800 行 C++ |
| 从零到可玩 | 约 7 天（含大量调试和方向固定难题） |

---

## 2. 总架构决策

### 2.1 同进程 vs 多进程

| 方案 | 优点 | 缺点 | 本项目 |
|------|------|------|:------:|
| **同进程** | 共享游戏状态、EGL Context；无需 IPC | Display 2 Activity 必须同进程启动 | ✅ |
| 多进程 | 独立崩溃域 | 状态同步复杂；无法共享 EGL | ❌ |

**结论**：游戏类双屏必须同进程。两个 Activity 在同一进程内运行，D2 Activity 不启动第二套游戏主循环，只提供 Android 窗口宿主。

### 2.2 双 Activity vs Presentation API

| 方案 | 适用场景 | 本项目 |
|------|---------|:------:|
| **双 Activity** | 独立 UI、独立触控、需要方向控制 | ✅ 最终方案 |
| Presentation API | 第二屏显示辅助信息 | ❌ 已废弃 |

**为什么放弃 Presentation**：
1. Presentation 不是独立 Activity，不能成为非默认显示器的方向策略所有者
2. 退出应用时 Presentation 不触发标准生命周期，后台 App 音频可能异常
3. 触摸事件路由受限

### 2.3 Android 层最终架构

```
┌─────────────────────────────────────────────────────┐
│                   同一进程 (PID x)                    │
│                                                     │
│  ┌───────────────────┐   ┌───────────────────────┐  │
│  │ SuperTuxKart      │   │ DualScreenActivity    │  │
│  │ Activity (Display 0)│   │ (Display 2)           │  │
│  │                   │   │                       │  │
│  │ SDLSurface (D0)   │   │ SDLSurface (D2)       │  │
│  │   └─ Surface      │   │   └─ Parent SC        │  │
│  │       └─ EGL      │   │       └─ Child SC     │  │
│  │           └─ Cam0 │   │           └─ EGL      │  │
│  │                   │   │               └─ Cam1 │  │
│  └───────────────────┘   └───────────────────────┘  │
│                                                     │
│  共享：EGLContext, World, RaceManager, DeviceManager │
└─────────────────────────────────────────────────────┘
```

### 2.4 实施路线总览

```
帧镜像 → 独立视角 → 触控分离 → 菜单流程 → 方向固定 → 性能优化 → 体验完善
 1天       1天        1天        2天        2天        1天        1天
```

---

## 3. 阶段 0：帧镜像（最小改动验证渲染链路）

### 3.1 目标

在 Display 2 上看到 Display 0 的画面副本。验证 Android → EGL → 副屏的渲染链路通畅。

### 3.2 Android 层

```java
// 1. 创建 D2 专用 Activity
public class DualScreenActivity extends Activity {
    @Override protected void onCreate(Bundle b) {
        super.onCreate(b);
        // 等待 native ready 后再创建 Surface
    }
}

// 2. 主 Activity 启动副屏
DisplayManager dm = getSystemService(DISPLAY_SERVICE);
for (Display d : dm.getDisplays()) {
    if (d.getDisplayId() != Display.DEFAULT_DISPLAY) {
        Intent intent = new Intent(this, DualScreenActivity.class);
        ActivityOptions options = ActivityOptions.makeBasic();
        options.setLaunchDisplayId(d.getDisplayId());
        startActivity(intent, options.toBundle());
        break;
    }
}
```

### 3.3 C++ EGL 层

```cpp
// 1. 共享 EGL Context，创建第二个 EGL Surface
EGLSurface g_secondSurface = EGL_NO_SURFACE;
ANativeWindow* g_secondWindow = nullptr;

// 2. 每帧渲染后拷贝到 D2
void dualScreenMirror() {
    if (!g_secondReady) return;

    // 绑定 D2 EGL Surface
    eglMakeCurrent(display, g_secondSurface, g_secondSurface, context);

    // 全屏 quad 绘制 D0 的渲染结果纹理
    drawFullscreenQuad(g_colorTexture);

    // 交换
    eglSwapBuffers(display, g_secondSurface);

    // 恢复 D0
    eglMakeCurrent(display, mainSurface, mainSurface, context);
}
```

### 3.4 关键验证

- [ ] D2 能看到画面
- [ ] 帧率没有崩溃性下降
- [ ] 进程 PID 不变（确认不是靠重启）

---

## 4. 阶段 1：独立相机视角

### 4.1 目标

D0 显示 Camera 0（Player 1 视角），D2 显示 Camera 1（Player 2 视角）。

### 4.2 原理

利用游戏引擎现有的分屏多人模式。大多数游戏的多人分屏已经管理了多 Camera、多 Player、多 Viewport。双屏只是把两个 viewport 分别渲染到两个 EGL Surface，而不是拼在一个屏幕。

### 4.3 关键改动

```cpp
// 原来：两个 camera 渲染到同一个 FBO 的左右两半
renderCamera(0, leftViewport);   // P1 左半
renderCamera(1, rightViewport);  // P2 右半

// 改为：
// D0：Camera 0 → 全屏
eglMakeCurrent(display, mainSurface, mainSurface, context);
renderCamera(0, fullViewport);
eglSwapBuffers(display, mainSurface);

// D2：Camera 1 → 全屏
eglMakeCurrent(display, g_secondSurface, g_secondSurface, context);
renderCamera(1, fullViewport);
eglSwapBuffers(display, g_secondSurface);

// 恢复
eglMakeCurrent(display, mainSurface, mainSurface, context);
```

### 4.4 通用适配要点

1. **Camera 数量检查**：确保引擎支持至少 2 个 Camera
2. **Viewport 全屏**：每个 camera 渲染到完整的 1920×1280，而非分屏
3. **HUD 分离**：小地图、计时器、道具栏等 UI 元素需要按 Camera 分别绘制
4. **不要重复整个渲染管线**：只需切 Surface → 切 Camera → render → swap → 恢复

---

## 5. 阶段 2：触控分离

### 5.1 目标

Display 0 触摸 → Player 1 操作；Display 2 触摸 → Player 2 操作。两屏互不干扰。

### 5.2 通用方案

**不要**在业务层手工解析触摸坐标并映射玩家。正确做法是：

1. **利用引擎的触控设备抽象层**
2. **给每个 Display 的触摸事件打上不同的 DeviceID**
3. **在输入管理器中按 DeviceID 路由到对应玩家**

### 5.3 SDL 路径（STK 的具体实现）

```
Android TouchEvent
  → SDL (按 touchDeviceId 区分)
    → Irrlicht CIrrDeviceSDL (写入 TouchInput.DeviceID)
      → STK InputManager (按 DeviceID 分流)
        → MultitouchDevice[0] → Player 0
        → MultitouchDevice[1] → Player 1
```

**核心改动量极小**：

```cpp
// Irrlicht — 4 行
// STouchInput 结构增加 DeviceID 字段
// CIrrDeviceSDL 传递设备 ID

// STK InputManager — 5 行
// 按 TouchInput.DeviceID 选择 MultitouchDevice
```

### 5.4 通用引擎适配

| 引擎 | 触控区分方案 |
|------|-------------|
| SDL | `SDL_Event.tfinger.touchId` — SDL 原生支持多设备 |
| Unity | `Input.GetTouch(i).fingerId` + Display 判断 |
| Unreal | `APlayerController` 按 ControllerId 分流 |
| 自定义引擎 | 在 Android `onTouchEvent` 中附加 Display ID，传给 C++ 层 |

### 5.5 虚拟方向盘

两屏各自渲染虚拟方向盘，按各自的触控状态独立绘制。关键是方向盘 GUI 必须知道自己属于哪个 Player/Display。

---

## 6. 阶段 3：双屏选单流程

### 6.1 核心挑战

游戏原有选单是**单光标、单焦点**。改为双屏后，两屏各自需要独立的光标和焦点。

### 6.2 双光标方案

**原理**：维护两个鼠标状态数组 `mouse_pos[2]`，按 `DeviceID` 选择对应光标。

```cpp
// CIrrDeviceStub — 核心改动
std::array<core::position2di, 2> mouse_pos;  // 原来是单值

// 触摸转鼠标时
int devId = event.TouchInput.DeviceID;
mouse_pos[devId] = touchPos;
irrevent.MouseInput.DeviceID = devId;

// STK EventHandler
std::array<core::vector2di, 2> m_mouse_pos;
// 按 event.MouseInput.DeviceID 更新对应项
```

### 6.3 选车流程状态机

```
阶段 1：P0 和 P1 各自选车（两屏独立）
  ├─ P0 确认 → D0 显示"等待副屏确认"
  ├─ P1 确认 → D2 显示"等待主屏确认"
  └─ 双方确认 → 进入阶段 2

阶段 2：P0 选择赛道（P1 等待）
  ├─ D0：赛道选择界面
  └─ D2："等待主屏选择赛道..."

阶段 3：创建 World，两屏同步进入
  └─ 3-2-1 倒计时 → 比赛开始
```

### 6.4 关键实现点

1. **Widget 显示隔离**：每屏的选车 widget 按 `m_display_id` 过滤
2. **Ribbon 高亮独立**：渲染时用 `curDisp` 映射的 `playerID`
3. **`setSinglePlayer(NULL)`**：双屏模式必须清除单玩家模式，否则所有触控被劫持给 P0
4. **`m_init_done` 标志位**：防止 init 阶段的 `setSelection` 触发自动确认

### 6.5 通用经验

- 菜单阶段的触控隔离比比赛阶段更难（引擎的菜单系统通常没考虑多光标）
- 如果引擎支持在线多人，可复用其"多客户端独立 UI"逻辑
- 首选方案：每屏一个独立的 GUI 上下文，互不干扰

---

## 7. 阶段 4：副屏物理方向固定

### 7.1 问题

Android 外屏（Display 2）会跟随设备 G-sensor 或厂商 DisplayRotation 策略改变方向。
游戏画面会跟着旋转 180°，导致上下颠倒。

### 7.2 失败方案列表

| 尝试 | 结果 | 原因 |
|------|------|------|
| `View.setRotation(180)` | 触摸旋转了，SurfaceView 画面不变 | SurfaceView 是独立 Surface 合成 |
| `ANativeWindow_setBuffersTransform(ROTATE_180)` | API 返回成功，画面不变 | 未成为厂商合成链路的最终策略 |
| Activity `requestedOrientation=reverseLandscape` | WMS 状态正确，物理画面仍可能错 | WMS rotation 不等于像素合成结果 |
| 只反转触摸坐标 | 输入对了，画面仍倒置 | 不是综合方案 |

### 7.3 最终方案：应用自有子 SurfaceControl + 动态逆旋转

**核心思想**：不阻止系统旋转，而是读取当前 rotation 后在应用像素层做逆变换。

```
系统 D2 rotation = 0  → 子层 transform = IDENTITY (0)
系统 D2 rotation = 2  → 子层 transform = ROT_180 (3)
```

### 7.4 实现步骤

```java
// 1. 创建应用自有子 SurfaceControl
SurfaceControl child = new SurfaceControl.Builder()
    .setName("game render surface")
    .setParent(surfaceView.getSurfaceControl())  // 挂到 SurfaceView 下
    .setBufferSize(width, height)
    .setFormat(PixelFormat.RGBX_8888)
    .build();

Surface renderSurface = new Surface(child);

// 2. EGL 用 renderSurface 而非 surfaceView.getHolder().getSurface()
nativeSetSecondSurface(renderSurface);

// 3. 计算逆旋转
int childRotation = inverseDisplayRotation(display.getRotation());
int surfaceTransform = toSurfaceControlTransform(childRotation);

// 4. 应用变换
new SurfaceControl.Transaction()
    .setGeometry(child, srcRect, dstRect, surfaceTransform)
    .setVisibility(child, true)
    .apply();

// 5. 注册 DisplayListener，rotation 变化时动态更新
displayManager.registerDisplayListener(listener, null);
```

### 7.5 ⚠️ 关键陷阱：Rotation 枚举混淆

```java
// Surface.ROTATION_180 == 2  →  这是逻辑方向枚举
// SurfaceControl 中 2 表示 FLIP_V（镜像垂直）！
// SurfaceControl.BUFFER_TRANSFORM_ROTATE_180 == 3  →  这才对应真正的 180° 旋转

// ❌ 错误：直接传 2
transaction.setGeometry(child, src, dst, 2);  // SurfaceFlinger: FLIP_V

// ✅ 正确
transaction.setGeometry(child, src, dst,
    SurfaceControl.BUFFER_TRANSFORM_ROTATE_180);  // SurfaceFlinger: ROT_180
```

### 7.6 触摸同步

触摸坐标必须用同一个 `childRotation` 做逆映射：

| childRotation | 变换后坐标 |
|:------------:|------------|
| 0（不转） | $(x, y)$ |
| 1（90°） | $(y, 1-x)$ |
| 2（180°） | $(1-x, 1-y)$ |
| 3（270°） | $(1-y, x)$ |

### 7.7 验收标准

```bash
# 强制测试 rotation 切换
adb shell wm fixed-to-user-rotation -d 2 enabled
adb shell wm set-ignore-orientation-request -d 2 true
adb shell wm user-rotation -d 2 lock 0   # → 画面不变
adb shell wm user-rotation -d 2 lock 2   # → 画面不变（逆变换抵消）

# SurfaceFlinger 验证
adb shell dumpsys SurfaceFlinger | grep "game render surface"
# rotation 0 → geomLayerTransform (IDENTITY)
# rotation 2 → geomLayerTransform (ROT_180) (ROTATE TRANSLATE)
# 绝对不能是 FLIP_V！

# 恢复
adb shell wm user-rotation -d 2 free
adb shell wm fixed-to-user-rotation -d 2 default
adb shell wm set-ignore-orientation-request -d 2 false
```

---

## 8. 阶段 5：性能优化到稳定帧率

### 8.1 核心问题

双屏渲染两个 Camera，GPU 工作量翻倍。初始帧率可能只有 17-20 FPS。

### 8.2 关键优化 1：Camera Fence 隔离

**问题**：两个 Camera 共享一个全局 `GLsync`，Camera 1 每帧等待 Camera 0 的 GPU 工作完成 → 强行串行化。

**修复**：
```cpp
// ❌ 原来
GLsync g_sync;  // 全局唯一

// ✅ 改为每 Camera 独立 fence
GLsync m_sync[MAX_PLAYER_COUNT];  // Camera 0 等 Camera 0 的，Camera 1 等 Camera 1 的
```

**配套**：动态资源（粒子 VAO/VBO、骨骼矩阵纹理、文字 billboard buffer）改为每玩家独立分配。

**收益**：renderer 从 ~40ms 降到 ~20-23ms。

### 8.3 关键优化 2：Orientation 抖动过滤

**问题**：Android/SDL 的 orientation 枚举在同一方向轴正反间反复变化（landscape ↔ landscape-flipped），每次触发 `resizeWindow()` 全量重建 RTT 和图形资源。尖峰 14-77ms。

**修复**：
```cpp
// 仅当实际尺寸未变 且 方向未跨轴（landscape ↔ landscape-flipped）时跳过 resize
if (g_dual_screen_mode &&
    m_actual_screen_size == currentSize &&
    newSize == currentSize &&
    !isAxisCrossing(oldOrientation, newOrientation)) {
    updateOrientationOnly();
    return;  // 不调用 resizeWindow()
}
```

**收益**：消除周期性卡顿尖峰。

### 8.4 关键优化 3：编译优化统一

**问题**：只有 `main` 模块用 `-O3 -mcpu=cortex-a73`，Bullet、Irrlicht、SDL 等模块以 `-O0` 编译。

**修复**：在 `build.gradle` 中为所有 native 模块统一设置：
```gradle
ndkBuild {
    arguments "APP_CFLAGS+=-O3",
              "APP_CFLAGS+=-mcpu=cortex-a73",
              "APP_CFLAGS+=-fomit-frame-pointer"
}
```

**收益**：simulation 从 13-18ms 降到 2-4ms。

### 8.5 关键优化 4：稳定帧率控制

```cpp
// 双屏模式：固定 30 FPS，开启 vsync 时也执行软件 pacing
if (g_dual_screen_mode) {
    float targetFrameTime = 1000.0f / 30.0f;  // 33.33ms
    if (elapsed < targetFrameTime) {
        sleepRemaining(targetFrameTime - elapsed);
    }
}
```

### 8.6 最终性能数据

| 指标 | 优化前 | 优化后 |
|------|:------:|:------:|
| 帧率 | 17-20 FPS | 稳定 30 FPS |
| 每帧工作量 | ~50ms | ~25ms |
| simulation | 13-18ms | 2-4ms |
| renderer | ~40ms | ~21ms |
| Camera 1 fence | ~18ms | ~0.02ms |

---

## 9. 阶段 6：体验完善（自动驾驶、HUD、加载动画）

### 9.1 双屏自动驾驶

**关键设计**：每个 `LocalPlayerController` 独立持有 `m_auto_drive_wanted` 状态。按钮 ID 必须全局唯一（100/101），不能与道具按钮 ID 冲突。

**最隐蔽的 bug**：`Controller::m_controls` 是指向 `kart->getControls()` 的**指针**，不是副本。`PlayerController::steer()` 每帧把 steer 往 0 拉，会与 `SkiddingAI` 抢同一份控制量，导致 AI 只走直线。修复：auto-drive 激活时跳过 `PlayerController::update()`。

### 9.2 双屏 HUD

每屏独立绘制小地图、计时器、玩家列表。HUD 元素按当前 Display 对应的 `ActivePlayer` 选择数据源。

### 9.3 加载动画

D2 在 EGL surface 就绪前有 ~6 秒黑屏。当前实现方案把“品牌加载界面”从单纯的立即隐藏，改成了“先展示、再等待 native ready、最后再切到正式渲染”的稳态流程。

#### 9.3.1 本次改动逻辑（2026-08-02）

1. **主屏先发起副屏加载入口**
   - `SuperTuxKartActivity` 在主屏创建阶段先调用 `launchDualScreenLoading()`，确保副屏加载流程尽早起跑。
   - 这样做的目的不是直接显示游戏画面，而是先让 D2 的品牌动画稳定出现。

2. **D2 统一使用全屏加载层**
   - `DualScreenActivity` 里用 `Dialog + LoadingCircleView` 作为覆盖层，保证副屏在一开始就有稳定的视觉占位。
   - `SDLSurface` 被挂到背景层，LoadingView 在前层，避免画面一开始就空白或闪一下。

3. **最小展示时长兜底**
   - 增加 `MIN_LOADING_DURATION_MS = 3000`。
   - 即使 native 已经 ready，加载界面也至少保持 3 秒，避免用户感觉“Logo 瞬间没了”。
   - 这个时长是为了保证视觉连续性，而不是为了拖延真实进入游戏。

4. **native ready 与 UI 收起解耦**
   - `DualScreenPresentation` / `DualScreenActivity` 都会轮询 `nativeIsD2Ready()`。
   - 只有当“native ready”且“已满足最短展示时长”两个条件同时满足时，才收起加载界面。
   - 这样可避免出现“native 已经准备好，但 UI 还没完成视觉过渡”的不连续感。

5. **资源与生命周期整理**
   - 加载层、Surface、生命周期清理都放在同一套流程里，避免外部 Activity 退出时留下残留窗口或空白层。
   - 对应的清理逻辑包含 `onDestroy()` 和异常退出场景。

#### 9.3.2 该方案的价值

- 避免副屏启动初期出现“黑屏 / 瞬间空白 / 过快切换”的视觉抖动。
- 把显示逻辑从“立即隐藏”改成“稳定展示”，更适合品牌启动和双屏体验。
- 这种思路对后续继续接入更复杂的副屏提示、等待界面或角色选择动画也有复用价值。

---

## 10. 通用踩坑清单

### 10.1 Android 层

| # | 坑 | 修复 |
|---|-----|------|
| 1 | Presentation 不能固定副屏方向 | 改用 D2 专用 Activity |
| 2 | D2 Activity 被启动到 D0 | 在 `onCreate` 加防呆检测 + `setLaunchDisplayId` |
| 3 | 防呆检测在 `super.onCreate()` 之前导致崩溃 | 必须先 `super.onCreate()` |
| 4 | `Surface.ROTATION_*` 和 `SurfaceControl.BUFFER_TRANSFORM_*` 数值不同 | 必须显式映射，不能直接传值 |
| 5 | `screencap -d 2` 不等于物理输出 | 最终验证必须看实车画面 |
| 6 | Surface 生命周期顺序错误导致 EGL 崩溃 | 先通知 native 停止 → release Surface → release SurfaceControl |

### 10.2 C++ EGL 层

| # | 坑 | 修复 |
|---|-----|------|
| 7 | 全局 fence 串行化两个 Camera | 每 Camera 独立 fence + 每 Player 独立动态资源 |
| 8 | Orientation 抖动触发 resize | 同轴同尺寸跳过 resize |
| 9 | GLES2 无 `glBlitFramebuffer` | 用 `glCopyTexSubImage2D` + 全屏 quad |
| 10 | EGL Surface 创建时机错误 | 等待 ANativeWindow 就绪后再创建 |

### 10.3 游戏逻辑层

| # | 坑 | 修复 |
|---|-----|------|
| 11 | `setSinglePlayer()` 劫持所有触控给 P0 | 双屏模式设置为 NULL |
| 12 | 内部 GUI 事件（DeviceID=0）污染 `m_last_touch_device` | 加 DeviceID-curDisp 匹配过滤 |
| 13 | `m_controls` 共享指针 → AI 与 PlayerController 抢控制权 | auto-drive 时跳过 `PlayerController::update()` |
| 14 | `ActivePlayer ID` ≠ `World::getKart()` 下标 | 用 `ActivePlayer::getKart()` |
| 15 | `m_init_done` 未初始化（C++ 不会自动赋 false） | 构造函数显式初始化 |
| 16 | XML `text` 属性会经过翻译词典 | 中文文案用 `raw_text` |

### 10.4 构建/部署

| # | 坑 | 修复 |
|---|-----|------|
| 17 | `git clone` 后编不过——`.gitignore` 排除了关键文件 | 解除 deps/、sdl2/、shaderc/、*.a 的忽略规则 |
| 18 | NDK r26 macOS `fcntl()` 大量警告 | 不影响构建，升级到 NDK 27+ 可消除 |

### 10.5 调试技巧

- **触摸验证用 swipe 不要用 tap**：瞬时事件可能落在错误帧
- **PID 是低成本判据**：旋转切换后 PID 不变 = 没重启
- **SurfaceFlinger dump 才是真相**：Java 日志和 WMS 状态都不等于实际合成
- **测试策略成组恢复**：`user-rotation free` + `fixed-to-user-rotation default` + `set-ignore-orientation-request false` 三项必须全部恢复

---

## 11. 可迁移到其他引擎的最小模板

### 11.1 前提检查清单

- [ ] 引擎支持多 Camera 渲染
- [ ] 引擎支持分屏多人模式（或多 Viewport）
- [ ] 设备有双显输出（`dumpsys display | grep mDisplayId`）
- [ ] API ≥ 26（双 Activity）或 ≥ 29（SurfaceControl 方向固定）

### 11.2 最小核心改动（按实施顺序）

```
第一优先级（渲染链路）：
  1. AndroidManifest 注册 D2 Activity
  2. 主 Activity 枚举副屏 + setLaunchDisplayId
  3. C++ 创建第二个 EGL Surface
  4. Camera 1 渲染到第二个 Surface（镜像）

第二优先级（触控）：
  5. 在触控事件中附加 Display ID
  6. 输入管理器按 DeviceID 路由到不同 Player

第三优先级（稳定性）：
  7. D2 Activity 方向固定（子 SurfaceControl + 动态逆旋转）
  8. 每 Camera 独立 fence（性能）
  9. Orientation 抖动过滤（性能）

第四优先级（体验）：
  10. 菜单双光标 + 独立选车
  11. 双屏 HUD
  12. 加载动画
```

### 11.3 通用代码模板

#### AndroidManifest.xml — D2 Activity 注册

```xml
<activity
    android:name=".DualScreenActivity"
    android:launchMode="singleInstance"
    android:taskAffinity=".secondary"
    android:excludeFromRecents="true"
    android:screenOrientation="reverseLandscape"
    android:configChanges="orientation|screenSize|screenLayout|keyboardHidden|smallestScreenSize"
    android:resizeableActivity="true"
    android:exported="false" />
```

#### 主 Activity 启动副屏

```java
void launchDualScreen() {
    DisplayManager dm = getSystemService(DISPLAY_SERVICE);
    for (Display d : dm.getDisplays()) {
        if (d.getDisplayId() != Display.DEFAULT_DISPLAY && d.isValid()) {
            Intent intent = new Intent(this, DualScreenActivity.class)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            ActivityOptions opts = ActivityOptions.makeBasic();
            opts.setLaunchDisplayId(d.getDisplayId());
            startActivity(intent, opts.toBundle());
            break;
        }
    }
}
```

#### 旋转枚举映射（Java）

```java
static int inverseDisplayRotation(int displayRotation) {
    switch (displayRotation) {
        case Surface.ROTATION_90:  return Surface.ROTATION_270;
        case Surface.ROTATION_270: return Surface.ROTATION_90;
        default: return displayRotation;
    }
}

static int toSurfaceTransform(int childRotation) {
    switch (childRotation) {
        case Surface.ROTATION_90:
            return SurfaceControl.BUFFER_TRANSFORM_ROTATE_90;   // 4
        case Surface.ROTATION_180:
            return SurfaceControl.BUFFER_TRANSFORM_ROTATE_180;  // 3 ⚠️ 不是 2
        case Surface.ROTATION_270:
            return SurfaceControl.BUFFER_TRANSFORM_ROTATE_270;  // 7
        default:
            return SurfaceControl.BUFFER_TRANSFORM_IDENTITY;    // 0
    }
}
```

#### 触摸坐标逆变换（C++）

```cpp
void transformTouchCoord(float& x, float& y, int childRotation) {
    float nx = x, ny = y;
    switch (childRotation) {
        case 0:  /* unchanged */          break;
        case 1:  nx = y; ny = 1.0f - x;   break;  // 90°
        case 2:  nx = 1.0f - x; ny = 1.0f - y; break;  // 180°
        case 3:  nx = 1.0f - y; ny = x;   break;  // 270°
    }
    x = nx; y = ny;
}
```

### 11.4 对不同引擎类型的建议

| 引擎类型 | 双屏方案建议 |
|---------|-------------|
| **自研 C++ 引擎 + SDL** | 按 STK 方案完整移植（SDL 原生支持多触摸设备） |
| **Unity** | 使用 Unity 的 Multi-Display 功能 + `Display.displays[1].Activate()` |
| **Unreal Engine** | 用 `UGameViewportClient` 的 `LayoutPlayers()` + 多 `FViewport` |
| **Cocos2d-x / Godot / 其他** | 评估引擎是否支持多 Viewport/RenderTarget；不支持则需要底层扩展 |

---

## 文档版本

| 版本 | 日期 | 说明 |
|------|------|------|
| v1.0 | 2026-07-25 | 基于 STK v1.6.10 双屏移植全流程整理 |

## 相关文档

| 文档 | 内容 |
|------|------|
| [chip.md § 2.7](chip.md#27-rk356x-副屏物理方向固定d2-activity--动态子-surfacecontrol-反补偿2026-07-24-真机验证) | 方向固定详细方案 |
| [dual-screen-performance.md](dual-screen-performance.md) | 双屏性能优化详情 |
| [dual_screen_kart_selection.md](dual_screen_kart_selection.md) | 选车流程架构 |
| [dual_cursor_plan.md](dual_cursor_plan.md) | 双光标实现计划 |
| [requirements.md](requirements.md) | 双屏对战需求文档 |
| [stk-android-build.md](stk-android-build.md) | 编译与 gitignore 经验 |

## 参考提交

| Commit | 内容 |
|--------|------|
| `17b332641` | 帧镜像 + 双人自动开赛 |
| `3ab9639b4` | 独立相机视角 |
| `bcce48589` | 触控分离 |
| `88ecc1c09` | 统一触控架构（SDL 标准路径） |
| `1f0216535` | 双屏选车独立性 |
| `cad88381f` | 双屏大厅 + RTT 修复 |
| `bc4c1568d` | 副屏方向固定 (SurfaceControl) |
| `53f9ed867` | 性能优化：FPS 稳定 30Hz |
| `a7cef997c` | 自动驾驶 AI 根因修复 |
