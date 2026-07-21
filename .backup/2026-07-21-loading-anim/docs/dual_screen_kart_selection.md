# 双屏独立选车 — 架构文档

> 最后更新：2026-07-21
> 相关：`docs/requirements.md`（需求）、`docs/cl.md`（版本记录）、`docs/dual_cursor_plan.md`（双光标计划）

## 概述

RK356x 平板双屏（Display 0 + Display 2，各 1920×1280）独立选车流程。
两屏各自触控选车，互不干扰，双方确认后进入比赛。

## 关键文件

| 文件 | 作用 |
|------|------|
| `src/main.cpp` | `g_dual_screen_mode` 全局标志（line 588），`startDualScreenRace()`（line 590），双屏入口（line 2705） |
| `src/states_screens/kart_selection.cpp` | 选车屏幕核心逻辑：`init()`、`playerConfirm()`、`allPlayersDone()`、`syncDisplayWidgets()` |
| `src/states_screens/kart_selection.hpp` | `PlayerKartWidget` 增加 `m_display_id` 字段 |
| `src/guiengine/event_handler.cpp` | 触控→鼠标转换，Ribbon hover 按 DeviceID 过滤 |
| `src/guiengine/skin.cpp` | `drawRibbonChild()` 用 curDisp 映射 PlayerID，避免高亮错乱 |
| `src/input/device_manager.cpp` | `setSinglePlayer()` 逻辑——本次 P2 触控劫持的根因 |
| `data/gui/screens/karts.stkgui` | P0/P1 选车等待提示 label（`p0_waiting`、`p1_waiting`） |
| `data/gui/screens/tracks_and_gp.stkgui` | P1 等待 P0 选赛道提示 label（`waiting_p1`） |
| `android/.../LoadingCircleView.java` | 副屏加载动画：Canvas 白圆圈 + 蓝"KEMI"（2026-07-21） |
| `android/.../DualScreenPresentation.java` | D2 Presentation：加载画面 z-order、JNI 轮询隐藏 |
| `android/android_native_dual_screen.cpp` | JNI `nativeIsD2Ready()`：EGL surface 就绪检测 |

## 整体流程

```
main.cpp:2705 → g_dual_screen_mode = true
  → OfflineKartSelectionScreen::getInstance()->push()
    → KartSelectionScreen::init()
      → 创建 P0 widget（getLatestUsedDevice）
      → 创建 P1 widget（getKeyboard(1)）
      → 设 m_display_id[0]=0, m_display_id[1]=2
      → 两 widget 全屏铺满（不分割）
      → m_init_done = true
    → 用户触控选车 → onSelectionChanged
      → 双屏自动确认：playerConfirm(player_id)
    → 双方确认完毕 → allPlayersDone()
      → setSinglePlayer(NULL)  ★ 关键！
      → 设 multitouch 参数
      → startSingleRace("abyss", 3, false)
```

## 目标大厅流程（待实现）

> 2026-07-20 设计结论：等待阶段必须保持在菜单状态；只有地图确定后才创建
> `World` 并让两个屏幕一起进入 `GAME`。

```text
阶段 1：D0/P0 与 D2/P1 独立选择角色和 kart
  ├─ 任一玩家未确认：两屏都停留在各自的选车界面
  └─ P0、P1 都确认：进入阶段 2

阶段 2：仅 P0 选择地图
  ├─ D0：TracksAndGPScreen，可浏览和确认赛道
  └─ D2：等待主屏选择地图，不创建 World、不渲染赛道 Camera

阶段 3：P0 确认地图
  ├─ 写入已经锁定的 P0/P1 kart 与赛道
  ├─ RaceManager::startNew(false)
  ├─ StateManager::enterGameState()
  └─ D0/D2 同时进入共享 World

阶段 4：正常 3-2-1 与双人比赛
```

确认规则：

- P0 先确认时，D0 显示“等待副屏玩家确认”，P1 仍可选择或更换 kart。
- P1 先确认时，D2 显示“等待主屏玩家确认”，P0 仍可选择或更换 kart。
- 双方确认后，D0 才能进入地图选择；D2 切为静态等待页。
- 在 P0 选地图期间，任一玩家取消准备都应回到双屏选车，并撤销进入地图选择的资格。

### 不采用的混合状态

不再采用“P0 已创建 `World` 并进入地图，P1 仍运行完整选车菜单”的流程。

原因来自当前 STK 的实际架构：`World`、`GameState` 与 Irrlicht GUI 环境都是进程级全局对象。
`GAME` 状态下 renderer 仍会为 Camera 1 渲染 D2 的赛道画面；之后绘制的选车控件只会叠在赛道上，
并非独立菜单。保留菜单控件并在 World 初始化后继续同步其可见性，实机上还触发过 SDLThread
`SIGSEGV`。因此不能通过隐藏 HUD 或叠加 widget 实现真正的“P0 地图 / P1 选车”。

新流程避免同时运行 `World` 与 P1 选车菜单，复用 STK 正常的：

```cpp
RaceManager::get()->startNew(false);
StateManager::get()->enterGameState();
```

生命周期，两个屏幕在同一个时刻进入比赛并开始倒计时。

## 关键设计决策

### 1. 触控隔离（event_handler.cpp）

- Touch→Mouse 转换时，`m_last_touch_device` 在 touch handler 层追踪
- Ribbon hover 时加 `DeviceID == curDisp` 过滤，防止内部 GUI 事件（DeviceID=0）污染
- `setFocusForPlayer` 必须在 `mouseHovered` 之前调用，否则 Ribbon 选不中

### 2. Ribbon 高亮独立渲染（skin.cpp）

- `drawRibbonChild` 用 `selPlayerID`（curDisp 映射）替代硬编码 `PLAYER_ID_GAME_MASTER`
- `process3DPane` 中 `focused` 基于 `renderPlayerID`
- 不修改则两屏 Ribbon 高亮完全一样

### 3. setSinglePlayer(NULL) ★ 本次修复（2026-07-20）

`device_manager.cpp` 关键逻辑：

```cpp
// device_manager.cpp:377-381
if (m_single_player != NULL) {
    // 强制 multitouch_device 只发给 m_single_player
    m_multitouch_device->setPlayer(m_single_player);
}
```

- `m_single_player != NULL` 时，**所有触控事件**被路由到该单玩家
- 双屏模式必须 `setSinglePlayer(NULL)`，让 ASSIGN 模式按设备各自路由
- 修复位置：`kart_selection.cpp` → `allPlayersDone()` 中，双屏分支加 `ap = NULL`

### 4. init_done 防护

- `setSelection()` 在 init 阶段会触发 `onSelectionChanged` 回调
- 用 `m_init_done` 标志位防止 init 阶段的自动确认
- `m_init_done = true` 在 `init()` 末尾设置

### 5. Widget 显示隔离（syncDisplayWidgets）

- 按 `m_display_id` 控制每屏可见的 widget
- 隐藏的 widget 移到屏幕外（x=5000），因为 `setVisible(false)` 不完全隐藏子元素
- P1 等待 P0 时，D2 显示 `p1_waiting` label，隐藏选车 widget

### 6. 比赛启动必须参数

进入比赛前必须设置（否则 D2 方向盘不显示、触控不响应）：

```cpp
UserConfigParams::m_multitouch_active = 2;   // 强制启用虚拟方向盘
UserConfigParams::m_multitouch_draw_gui = true;
UserConfigParams::m_multitouch_controls = MULTITOUCH_CONTROLS_STEERING_WHEEL;
RaceManager::get()->setNumKarts(2);          // 只要2玩家，不加AI
input_manager->getDeviceManager()->setAssignMode(ASSIGN);
```

### 7. UI 文案管理标准：XML 定义，不硬编码在 C++

STK 的标准做法是：**所有 UI 文案都定义在 XML screen 文件中，不应在 C++ 源码中硬编码文本。**

原因：
- **维护性**：修改文案只需编辑 XML，无需重新编译 C++ 代码
- **本地化**：文案集中管理便于多语言支持和翻译维护
- **一致性**：所有 UI 元素的生命周期（加载、显示、销毁）都由 `screen_loader.cpp` 统一管理
- **灵活性**：XML 加载时可动态应用翻译系统、动态值替换等后处理

**反面教训**：之前在 `engine.cpp` 中硬编码了等待提示文案的英文版本作为 MENU 阶段的覆盖层，
导致 XML label 无法正确显示，用户看到的始终是 C++ 中的英文。2026-07-21 修正为：移除 C++ 硬编码，
完全依赖 XML label，由 `kart_selection.cpp` 的 `syncDisplayWidgets()` 控制其可见性。

### 8. 等待界面中文文案

等待文案定义在 XML screen 文件中：

| 显示时机 | Label ID | 源文件 | 中文文案 |
|---|---|---|---|
| P0 已确认 kart，等待 P1 | `p0_waiting` | `data/gui/screens/karts.stkgui` | `等待副屏玩家确认赛车…` |
| P1 已确认 kart，等待 P0 | `p1_waiting` | `data/gui/screens/karts.stkgui` | `等待主屏玩家选择赛车…` |
| P0 正在选择赛道，P1 等待 | `waiting_p1` | `data/gui/screens/tracks_and_gp.stkgui` | `等待主屏玩家选择赛道…` |

**必须使用 `raw_text`，不能使用 `text`。** `src/guiengine/screen_loader.cpp`
会对每个 `text` 属性调用 `_(text)`，交给 tinygettext 翻译词典处理。等待文案是
直接写入 XML 的中文字符串，使用 `text` 时可能被词典回退为英文。`raw_text` 在同一
loader 中直接赋给 widget，不经过翻译查询，因此可保证中文原样显示。

Android 有两份运行时资源需要同步：

1. 编辑 `data/gui/screens/*.stkgui` 作为源码。
2. 复制到 `android/assets/data/gui/screens/`，确保新 APK 包含相同资源。
3. 真机已解压资产时，直接推送到：
  `/storage/emulated/0/Android/data/org.supertuxkart.stk/files/SuperTuxKart/data/gui/screens/`。
4. 重启 `org.supertuxkart.stk/.SuperTuxKartActivity` 后验证 P0 等 P1、P1 等 P0、P1 等选图三种状态。

### 9. 副屏加载动画（2026-07-21 最终方案）

**需求**：副屏（Display 2）从 Android Presentation 创建到原生渲染就绪之间有 ~6 秒黑屏。需要在此期间显示加载动画，让用户感知系统在响应。

**最终三层架构**：

```
Java Presentation 创建
  → LoadingCircleView (白圆圈 + 蓝"KEMI" 脉动动画)  ← 覆盖初始 ~6s 黑屏
  → JNI 轮询 nativeIsD2Ready() 每 500ms
  → Native EGL surface 就绪 → 立即隐藏 Java 加载画面
  → 选车界面直接出现（无中间过渡）
```

**关键文件**：

| 文件 | 作用 |
|------|------|
| `android/.../LoadingCircleView.java` | 自定义 View：Canvas 绘制白色圆圈 + 蓝色"KEMI"，`postInvalidateOnAnimation()` 驱动脉动 |
| `android/.../DualScreenPresentation.java` | `onCreate()` 中创建 LoadingCircleView（在 SDLSurface **之上**，`bringToFront()`）；`startPollingNativeReady()` 每 500ms 调 JNI |
| `android/android_native_dual_screen.cpp` | JNI 实现 `nativeIsD2Ready()`：检查 `g_secondReady && g_secondEGLSurface != EGL_NO_SURFACE` |

**动画参数**：
- 圆圈半径：屏幕最小边 × 30%，脉动幅度 ±15%
- 圆圈颜色：白色描边（8px 宽）
- 文字颜色：蓝色 `#3366FF`，字号为屏幕 18%
- 背景颜色：深蓝 `#0A0F28`
- 帧率：由 Android Choreographer 驱动（~60fps）

**方案演进历史**：
1. ❌ C++ `draw2DLine` 画圆 — Irrlicht 2D API 不可靠
2. ❌ XML `bubble` widget — bubble 类型无 position 警告，且 label 不支持自定义颜色
3. ❌ C++ `getTitleFont()->draw()` + `draw2DRectangle` — 需要 3 秒最小显示计时器，与 Java 层不同步，且展示了不需要的中间过渡帧
4. ✅ **Java `LoadingCircleView`** — Android 原生 Canvas 绘制，脉动动画流畅，与 SDLSurface z-order 正确，JNI 精确同步隐藏时机

## 已知问题 & 修复记录

| # | 问题 | 根因 | 修复 |
|---|------|------|------|
| 1 | P1 widget 创建失败 | `setSelection` 会清除 `m_kart_widgets` | P1 创建移到 `setSelection` 之前 |
| 2 | D2 触控更新了 P0 的选车 | 内部 GUI 事件（DeviceID=0）污染 `m_last_touch_device` | 加 DeviceID-curDisp 匹配过滤 |
| 3 | P1 3D 模型不更新 | `setFocusForPlayer` 在 `getSelectedRibbon` 之后调用 | 调换顺序 |
| 4 | Ribbon 高亮两屏相同 | `skin.cpp` 硬编码 `PLAYER_ID_GAME_MASTER` | 用 curDisp 映射的 playerID |
| 5 | init 时自动确认 | `setSelection` 触发 `onSelectionChanged` | `m_init_done` 标志位 |
| 6 | 两 widget 都显示在 D0 | `setVisible(false)` 不隐藏子元素 | 移到屏幕外 x=5000 |
| 7 | **D2 比赛无法操控** | `setSinglePlayer(player0)` 劫持所有触控给 P0 | `setSinglePlayer(NULL)` |
| 8 | D2 初始 ~6s 黑屏 | EGL surface 创建前无渲染；Java view 被 SDLSurface 遮挡 | Java `LoadingCircleView` Canvas 绘制 + `bringToFront()` + JNI 轮询隐藏 |
| 9 | 选车界面残留 "KEMI" 文字 | XML `d2_loading` label 默认可见 | 移除 XML widget，加载画面完全由 Java 层管理 |
| 10 | C++ 加载过渡帧多余 | 3 秒最小显示计时器与 Java 层不同步 | 删除 C++ 加载覆盖层，native 就绪直接进选车 |

## 启动方式

```bash
# 编译
cd android && ./gradlew assembleDebug

# 安装
adb -s <IP>:5555 install -r build/outputs/apk/debug/android-debug.apk

# 启动（设备 ROM 定制，必须用 SuperTuxKartActivity）
adb -s <IP>:5555 shell am start -n org.supertuxkart.stk/.SuperTuxKartActivity

# 多设备切换
adb disconnect <旧IP>:5555 && adb connect <新IP>:5555
```
