# 双屏独立选车 — 架构文档

> 最后更新：2026-07-23
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
| `src/input/input_manager.cpp` | 比赛触控按 `TouchInput.DeviceID` 分发到两套 `MultitouchDevice` |
| `src/input/multitouch_device.cpp/.hpp` | 两套按钮状态、触点状态和 controller 绑定 |
| `src/states_screens/race_gui_multitouch.cpp/.hpp` | 双屏方向盘和自动驾驶按钮创建、点击回调、逐屏状态绘制 |
| `src/karts/controller/local_player_controller.cpp/.hpp` | 每名玩家独立的自动驾驶状态和 AI 接管逻辑 |
| `src/states_screens/race_gui_base.cpp` | 按当前 display 对应的 ActivePlayer 显示自动驾驶状态文字 |
| `src/config/user_config.hpp` | 自动驾驶初始默认值；只用于初始化每名玩家的独立状态 |
| `data/gui/icons/android/auto_drive.png` | 自动驾驶开启图标（绿色） |
| `data/gui/icons/android/auto_drive_off.png` | 自动驾驶关闭图标（灰色） |
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

### 6.1 双屏自动驾驶独立操作（2026-07-23 实机验证）

自动驾驶必须复用方向盘已经验证过的所有权关系：

```text
D0 Touch DeviceID 0
  -> MultitouchDevice 0
  -> ActivePlayer 0
  -> ActivePlayer 0::getKart()
  -> LocalPlayerController 0::m_auto_drive_wanted

D2 Touch DeviceID 2
  -> MultitouchDevice 1
  -> ActivePlayer 1
  -> ActivePlayer 1::getKart()
  -> LocalPlayerController 1::m_auto_drive_wanted
```

#### 状态分层

| 层级 | 状态 | 规则 |
|---|---|---|
| 用户配置 | `m_multitouch_auto_drive=true` | 只定义新建 controller 的默认值，不作为运行时共享开关 |
| 玩家控制器 | `LocalPlayerController::m_auto_drive_wanted` | 每名本地玩家独立持有；按钮只能切换所属玩家这一份状态 |
| 触控设备 | `m_multitouch_device` / `m_multitouch_device_2` | D0/D2 各自维护按钮和触点，不共享 `pressed`、`event_id` |
| 绘制 | `RaceGUIMultitouch::draw(const AbstractKart* kart, ...)` | 必须读取当前视图传入 kart 的 controller 状态，不能重新猜 kart |
| 全局提示 | `RaceGUIBase::drawGlobalMusicDescription()` | 用当前 display 选择 ActivePlayer，再读取该玩家的 kart/controller |

#### 正确的玩家到 kart 查找方式

`player_id` 是本地 ActivePlayer 编号，不是 `World` kart 数组下标。正确路径是：

```cpp
StateManager::ActivePlayer* player =
    StateManager::get()->getActivePlayer(player_id);
AbstractKart* kart = player ? player->getKart() : NULL;
LocalPlayerController* controller = kart
    ? dynamic_cast<LocalPlayerController*>(kart->getController()) : NULL;
```

禁止使用：

```cpp
World::getWorld()->getKart(player_id);
```

世界 kart 数组包含本地玩家、AI、网络玩家，并按比赛世界顺序编号；其下标不保证等于
ActivePlayer ID。此前用 `World::getKart(0/1)` 取 P0/P1，取到非本地 controller 时
`dynamic_cast<LocalPlayerController*>` 失败，绘制代码保留默认 `false`，表现为默认图标
始终灰色；按钮回调也可能切错 kart，表现为两屏不能独立。

#### 自动驾驶按钮 ID 必须全局唯一

`MultitouchDevice::addButton()` 默认用按钮数组下标作为 `button->id`。方向盘 GUI 在自动
驾驶按钮之前已经创建 FIRE 等按钮，因此数字 `5` 已经被其他按钮占用。不能再把自动
驾驶按钮硬编码为 `5`，也不能在克隆 D2 按钮时把所有 `id == 5` 的按钮都改成 `15`。

当前固定使用不与普通按钮下标重叠的 ID：

```cpp
AUTO_DRIVE_P0_BUTTON_ID = 100;  // D0 / ActivePlayer 0
AUTO_DRIVE_P1_BUTTON_ID = 101;  // D2 / ActivePlayer 1
```

D2 克隆时只将 `100` 映射为 `101`，其他按钮 ID 原样保留。点击回调只接受 100/101，
收到其他 ID 立即拒绝，不做 P0 fallback。

#### 图标映射

只使用两张产品图，不根据文件名猜测，也不静默回退到其他图标：

| 状态 | 文件 | 实机效果 | 源码 SHA-256 |
|---|---|---|---|
| `isAutoDriveWanted() == true` | `android/auto_drive.png` | 绿色图标，默认状态 | `ad1cf8ea447439107bd7cdb428914560d9dec701a7ecfc4b603cc357dc9f6f20` |
| `isAutoDriveWanted() == false` | `android/auto_drive_off.png` | 灰色图标 | `84e414bd005064b63fe0775888a79ff3c0056d7237b63ea8d0451dc7a986b947` |

`auto_drive_on.png` 不是当前产品状态图，不参与运行时加载；其源码 SHA-256 为
`ef34019cf5ed030e1466480cbfcb111311799d21197a7a0639bf70ee4129bd13`。两张有效资源必须
同时列在 `android/assets/files.txt`，并验证 APK 内资源、设备解压资源与上表一致。

两张有效资源均为 138×138 PNG，与 1920×1280、`multitouch_scale=1.2` 时计算出的
自动驾驶图标矩形 138×138 完全一致，因此运行时按 1:1 像素绘制，不再将 128×128
源图放大到 138×138。顶层 `data/` 与 Android 打包目录 `android/assets/data/` 的副本
必须同步更新；只修改顶层资源会导致 APK 继续包含旧图。

#### AI 功能独立性

每个 `LocalPlayerController` 独立创建自己的 `SkiddingAI`，并在所属玩家没有方向盘、
油门或刹车输入时把 AI 的 steer/accel/brake 复制到该 kart 的 controls。玩家主动操作
只暂停自己的 AI 接管，不影响另一名玩家的 `m_auto_drive_wanted`。氮气、漂移、道具、
救援、后视等显式动作在 AI 接管期间继续保留。

#### 必须保留的诊断日志

进入比赛 HUD 后，两名玩家各打印一次：

```text
RaceGUIMultitouch: Initial auto-drive P0: worldKart=<id> state=ON device=D0
RaceGUIMultitouch: Initial auto-drive P1: worldKart=<id> state=ON device=D2
```

按钮切换打印：

```text
RaceGUIMultitouch: Auto-drive P0 OFF/ON
RaceGUIMultitouch: Auto-drive P1 OFF/ON
```

若 ActivePlayer 没有 kart 或 kart 不是 LocalPlayerController，必须输出 error，不允许静默
回退到全局配置或其他玩家。

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
| 11 | 自动驾驶默认始终显示灰色 | 把 ActivePlayer ID 当作 `World::getKart()` 下标；取到错误 controller 后状态默认 false | 改用 `ActivePlayer::getKart()`，绘制直接读取当前视图 kart 的 controller |
| 12 | 两屏自动驾驶按钮不能独立 | 自动驾驶复用按钮 ID 5，与已有 FIRE 按钮 ID 冲突；D2 克隆时两个按钮一起被重映射 | 使用唯一 ID 100/101，仅映射自动驾驶按钮 |
| 13 | ON/OFF 图标反复混乱 | 根据文件名猜图，且存在第三张 `auto_drive_on.png` 干扰 | 固定 `auto_drive.png`=绿色 ON、`auto_drive_off.png`=灰色 OFF，第三张不加载 |
| 14 | **自动驾驶 AI 只走直线** | `PlayerController::steer()` 与 `SkiddingAI` 抢同一份 `m_controls`——每帧 `steer()` 把方向往 0 拉，AI 无法累积足够的转角来过弯 | 见下方 "## 自动驾驶 AI：走直线根因分析" |

## 自动驾驶 AI：走直线根因分析（v1.6.4 实机验证）

### 背景

同一地图上的 NPC AI（bot）正常驾驶、能转弯，但双屏模式下两个玩家的自动驾驶
AI 只走直线，最终撞墙卡死。多次尝试放大 steer、限速等手段均无法根治。

### 核心发现：m_controls 是共享指针

`Controller::Controller()`（[controller.cpp](/Users/newlink/kemi/stk-code/src/karts/controller/controller.cpp)）：

```cpp
Controller::Controller(AbstractKart *kart) {
    m_controls = &(kart->getControls());   // 指针，不是副本！
    m_kart     = kart;
}
```

所有附着在同一个 kart 上的 controller（`LocalPlayerController` + 内部 `SkiddingAI`）
共享**同一份** `m_controls`。对 `m_controls` 的任何修改直接作用在 kart 的控制量上。

### 冲突机制（每帧）

```text
帧 N:
  1. SkiddingAI::setSteering() → m_controls->setSteer(0.5)  ← AI 想右转
  2. kart 物理用 steer=0.5 计算

帧 N+1:
  3. LocalPlayerController::update()
  4.   PlayerController::update()
  5.     steer(ticks, 0)                   ← steer_val=0，无触控
  6.     steer = m_controls->getSteer()    ← 读到 0.5（AI 上一帧的值）
  7.     steer -= STEER_CHANGE             ← 往 0 拉！→ 0.45
  8.     m_controls->setSteer(0.45)        ← 覆盖！
  9.   auto-drive 代码
 10.     SkiddingAI::update()
 11.       handleSteering() → steerToPoint() → steer_angle = 0.3 rad
 12.       setSteering(0.3, dt)
 13.         steer_fraction = 0.3 / maxSteerAngle
 14.         max_steer_change = dt / time_full_steer
 15.         old_steer = m_controls->getSteer()  ← 读到 0.45（已被步骤 7 衰减）
 16.         new_steer = old_steer + max_steer_change = 0.47
 17.         m_controls->setSteer(0.47)          ← AI 勉强恢复一点
 18.   → 净效果：AI 想转到 0.5，但只到达 0.47
```

每帧循环一次：PlayerController 把 steer 往 0 拉 → AI 试图恢复 →
AI 的 `setSteering()` 从被衰减的值开始 ramp → 永远追不上需要的大转角。

**NPC AI 为什么正常**：NPC kart 的 controller 就是 `SkiddingAI`，没有
`PlayerController` 在每帧执行 `steer()`。"回中"行为是 player controller 专用的。

### 修复

在 `LocalPlayerController::update()` 中，当 `m_auto_drive_active == true` 时
跳过 `PlayerController::update()`——只有一行：

```cpp
if (!m_auto_drive_active)
{
    PlayerController::update(ticks);
}
```

### 附带修复：AI 惰性创建

原代码在 `m_auto_drive_ai == NULL || !m_auto_drive_active` 条件下创建 AI，
但在比赛 3-2-1 倒计时期间 `isStartPhase()=true` 阻止了接管，
`m_auto_drive_active` 保持 `false`，导致 AI 每帧 delete + new。
修复：把 AI 创建移入实际接管块内部（惰性创建）。

### 验证数据

| 指标 | 修复前 | 修复后 |
|---|---|---|
| 最大 steer | 0.083 (8%) | 0.942 (94%) |
| 速度曲线 | 17→0 (卡墙) | 13-15 稳定 |
| 连续无中断驾驶 | ~15s | 55s+ |
| 卡墙/rescue 次数 | 1 次/15s | 0 |
| 每帧 AI 重复创建 | 上百次 | 0（仅接管时创建 1 次） |

### AI 诊断日志体系

三级诊断，覆盖 AI 全生命周期：

| 级别 | 标签 | 频率 | 内容 |
|---|---|---|---|
| 创建 | `Auto-drive AI created` | 接管时 1 次 | track 名、DriveGraph 节点数、kart 位置 |
| 运行时 | `AI-path` / `AI-diag` | 每 60 帧 | 跟踪节点、瞄准点、转向角、弯道半径、速度 |
| 异常 | `AI-ZERO` | 连续 3s 零输出 | 可能的 DriveGraph 问题告警 |

调试命令：`adb logcat -d | grep -E 'AI-diag|AI-path|AI-ZERO|Auto-drive'`

## 自动驾驶回归验收

每次修改自动驾驶、方向盘、玩家创建顺序或比赛 kart 数量后，必须执行：

1. `./gradlew assembleDebug`，确认 `BUILD SUCCESSFUL`。
2. `adb install -r`，随后 force-stop、清 logcat、冷启动 Activity。
3. 双方完成选车并进入比赛，检查 P0/P1 两条 `Initial auto-drive ... state=ON` 日志。
4. D0 点击一次自动驾驶：只允许出现 `Auto-drive P0 OFF`，D0 图标变灰，D2 保持绿色。
5. D2 点击一次自动驾驶：只允许出现 `Auto-drive P1 OFF`，D2 图标变灰，D0 状态不变。
6. 分别再点击一次，P0/P1 各自恢复 ON 和绿色。
7. 截取 Display 0/2 两张截图，并检查无 `FATAL`、`SIGSEGV`、`SIGABRT`、
  `ActivePlayer has no kart`、`has no local controller`。

判断标准以截图和日志为准，不能只凭源码推断“应该独立”。

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
