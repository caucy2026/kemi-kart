# 双屏独立选车 — 架构文档

> 最后更新：2026-07-20
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
| `data/gui/screens/karts.stkgui` | `p1_waiting` 等待提示 label |
| `data/gui/screens/tracks_and_gp.stkgui` | `waiting_p1` 等待提示 label |

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
