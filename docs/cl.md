# STK 双屏异显 — 版本记录

## v1.6.5 (2026-07-23) — AI 优化：平滑接管 + 弯道减速 + 默认角色修复

### AI 优化：方案 A — 复用 + 平滑过渡

**改动**：不再 delete/recreate `SkiddingAI` 实例，改为复用 + `reset()`。

```text
修复前：松手 → delete AI → new AI → 立即切到 AI 方向（可能一抖）
修复后：松手 → AI.reset() → 0.3s 内 steer 从玩家值渐变到 AI 值
```

**代码**（`local_player_controller.cpp`）：

```cpp
// 复用 AI，不再 delete/new
if (m_auto_drive_ai)
    m_auto_drive_ai->reset();    // ← 替代 delete + new
else
    m_auto_drive_ai = new SkiddingAI(m_kart);

m_auto_drive_blend = 0.0f;       // ← 启动 0.3s blend

// 平滑过渡
m_auto_drive_blend += dt / 0.3f;  // 0→1 over 0.3s
steer = player_steer * (1-blend) + ai_steer * blend;
```

**新增字段**：`m_auto_drive_blend`（`local_player_controller.hpp`）

### AI 优化：方案 B — 弯道预减速

AI 打方向越狠 → 提前收油，避免全速冲弯。

```cpp
if      (abs_steer > 0.85f) accel *= 0.25f;  // 急弯：大幅减速
else if (abs_steer > 0.60f) accel *= 0.55f;  // 中弯
else if (abs_steer > 0.35f) accel *= 0.80f;  // 缓弯
// else: 直道全速
```

### 默认角色修复：P1 默认第二个角色

**症状**：副屏显示第二个角色，但点击"继续"确认的是第一个。必须手动点一次才正确。

**调试过程**：

1. init 代码已设 `w->setSelection(1, 1, true)`（P0=0, P1=1），视觉上正确
2. 日志显示 `onSelectionChanged: pid=1 sel=amanda` 回调产生
3. 但 `m_kart_widgets[1].getKartInternalName()` 为空 → 确认的是第一个

**第一层**：`KartHoverListener::onSelectionChanged()` 的跨屏过滤器

init 阶段 `getLastTouchDevice()` 返回 0（默认，无真实触摸），过滤器算出
`expected_player=0`，把 `player_id=1` 的回调 discard。P1 的 `setKartInternalName()`
从未被调用。

修复：在跨屏过滤条件中加入 `m_parent->m_init_done` 判断——init 阶段不过滤。

**第二层**：`m_init_done` 未初始化（根因）

C++ 不会自动给 `bool` 成员变量赋 `false`。`m_init_done` 持有随机垃圾值，
有概率已经是 `true`，导致第一层修复失效。

修复：构造函数中显式 `m_init_done = false;`

```cpp
KartSelectionScreen::KartSelectionScreen(...) {
    ...
    m_init_done = false;  // ← 必须显式初始化
}
```

**验证**：
```
P0 continue: widgetSel='adiumy' ✅
P1 continue: widgetSel='amanda' ✅
```

### 改动文件

| 文件 | 改动 |
|---|---|
| `src/karts/controller/local_player_controller.cpp` | AI 复用替代 delete/new + 0.3s blend + 弯道减速 |
| `src/karts/controller/local_player_controller.hpp` | 新增 `m_auto_drive_blend` 字段 |
| `src/states_screens/kart_selection.cpp` | 跨屏过滤器加 `m_init_done` 判断 + 构造函数初始化 |

---

## v1.6.4 (2026-07-23) — 自动驾驶 AI 走直线根因修复

### 症状

自动驾驶 AI 不会转弯，只走直线，最终撞墙卡死。但同一地图上的 NPC AI（bot）正常驾驶。

### 根因：PlayerController::steer() 与 SkiddingAI 抢同一个 m_controls

**关键发现**：`Controller::Controller()` 初始化时 `m_controls = &(kart->getControls())`。
所有附着在同一个 kart 上的 controller 实例共享**同一份** `m_controls` 指针，不是各自拥有副本。

```text
每帧执行顺序：
  1. LocalPlayerController::update()
  2.   → PlayerController::update()
  3.     → steer(ticks, 0)          ← steer_val=0（无触控输入）
  4.       steer = m_controls->getSteer()  ← 读到 AI 上一帧设置的值（如 0.5）
  5.       steer -= STEER_CHANGE           ← 把方向往 0 拉！
  6.       m_controls->setSteer(steer)     ← 覆盖为 0.45
  7.   → auto-drive 代码
  8.     → m_auto_drive_ai->update()       ← AI 的 setSteering() 尝试恢复
  9.       → 从已衰减的 0.45 开始 ramp → 永远追不上弯道
```

**NPC AI 为什么正常**：NPC kart 的 controller 就是 `SkiddingAI`，没有 `PlayerController`
在每帧抢控制权。`steer()` 是 player controller 专有的"回中"行为。

### 修复

`src/karts/controller/local_player_controller.cpp` — `update()` 中加一行条件判断：

```cpp
// auto-drive 激活时不调用 PlayerController::update()，避免 steer 被拉回 0
if (!m_auto_drive_active)
{
    PlayerController::update(ticks);
}
```

同时修复了 auto-drive AI 在比赛倒计时期间每帧重复创建的问题：
- 根因：`m_auto_drive_ai == NULL || !m_auto_drive_active` 在 `isStartPhase()=true`
  期间一直满足 → AI 每帧 delete + new
- 修复：把 AI 创建移到接管点内部（惰性创建），倒计时期间不创建

### 验证数据

| 指标 | 修复前 | 修复后 |
|---|---|---|
| 最大 steer | 0.083 (8%) | 0.942 (94%) |
| 速度 | 17→0 (卡墙) | 13-15 稳定 |
| 连续驾驶时间 | ~15s 后卡住 | 55s+ 无中断 |
| 卡墙/rescue | 触发 | 零次 |

### 新加诊断日志

为方便后续调试，在 `SkiddingAI::handleSteering()` 和 `LocalPlayerController::update()` 中
增加了三级 AI 诊断日志：
- `AI-path`: 每 60 帧输出 AI 内部跟踪节点、瞄准点、转向角、弯道半径
- `AI-diag`: 增强版（含 worldKart ID）
- `AI-ZERO`: AI 接管后 3 秒 steer/accel/brake 全零时告警（定位 DriveGraph 问题）

### 改动文件

| 文件 | 改动 |
|---|---|
| `src/karts/controller/local_player_controller.cpp` | AI 惰性创建 + 跳过 PlayerController::update() + 三级诊断日志 |
| `src/karts/controller/skidding_ai.cpp` | AI-path 诊断日志 + aim_point 作用域修正 |
| `data/gui/icons/android/auto_drive*.png` | 图标点对点 138×138 px |

---

## v1.6.3 (2026-07-23) — 双屏自动驾驶独立操作根治

### 实机结果

- Display 0 自动驾驶按钮只切换 Player 0；Display 2 只切换 Player 1。
- 两名玩家默认 `m_auto_drive_wanted=true`，进入比赛均显示绿色 `auto_drive.png`。
- 单独关闭某一玩家后，仅该屏切换为灰色 `auto_drive_off.png`，另一屏状态不变。
- 两套虚拟方向盘、自动驾驶 AI 和状态图标使用同一套 ActivePlayer 所有权关系。

### 两个根因

#### 1. ActivePlayer ID 被错误当成 World kart 下标

旧实现多处使用：

```cpp
World::getWorld()->getKart(player_id);
```

`player_id` 是本地 `ActivePlayer` 编号，`World::getKart()` 参数是世界 kart 数组下标。
世界数组可能包含 AI 或网络 kart，两者没有稳定的一一对应关系。取错 kart 后，
`dynamic_cast<LocalPlayerController*>` 失败，图标状态保留默认 `false`，因此默认显示灰色；
按钮回调也可能切换错误 controller。

修复为唯一正确路径：

```text
player_id -> StateManager::getActivePlayer(player_id)
		  -> ActivePlayer::getKart()
		  -> LocalPlayerController
```

绘制函数已经收到当前视图的 `const AbstractKart* kart`，图标和背景颜色直接从这个 kart
的 controller 读取，不再二次通过 world 下标猜测。

#### 2. 自动驾驶按钮 ID 与 FIRE 按钮冲突

`MultitouchDevice::addButton()` 默认将按钮数组下标作为 ID。自动驾驶按钮创建前已有一个
普通按钮使用 ID 5，旧实现又把自动驾驶硬编码为 ID 5。克隆 D2 按钮时，所有 ID 5 都被
改为 15，导致两个不同按钮 ID 相同，回调和绘制不能可靠区分。

修复为：

```cpp
AUTO_DRIVE_P0_BUTTON_ID = 100;
AUTO_DRIVE_P1_BUTTON_ID = 101;
```

D2 克隆时只把自动驾驶 100 映射为 101；其他按钮 ID 不变。回调只接受 100/101，拒绝
其他 ID，不使用 P0 fallback。

### 图标规则

| 状态 | 文件 | 显示 |
|---|---|---|
| ON（默认） | `data/gui/icons/android/auto_drive.png` | 绿色 |
| OFF | `data/gui/icons/android/auto_drive_off.png` | 灰色 |

`auto_drive_on.png` 不参与当前运行时加载。曾错误地根据文件名把它当作 ON 图，导致显示
与产品定义不一致。以后必须查看图片实际像素，并对比源码、APK、设备解压目录的哈希，
不能根据文件名猜测。

### 代码改动

| 文件 | 改动 |
|---|---|
| `src/states_screens/race_gui_multitouch.cpp` | 新增唯一按钮 ID 100/101；通过 ActivePlayer 获取 kart；图标直接读当前视图 controller；增加初始状态和错误日志 |
| `src/states_screens/race_gui_base.cpp` | 全局状态提示按 current display 映射 ActivePlayer，不再固定读 world kart 0/1 |
| `src/karts/controller/local_player_controller.cpp/.hpp` | 每名玩家独立持有 `m_auto_drive_wanted`、`m_auto_drive_active`、`SkiddingAI`；AI 接管 steer/accel/brake |
| `src/input/input_manager.cpp` | 现有 DeviceID 路由：D0 -> multitouch 0，D2 -> multitouch 1 |
| `src/input/device_manager.cpp` | 现有 ActivePlayer 绑定：multitouch 0 -> P0，multitouch 1 -> P1 |
| `src/config/user_config.hpp` | `m_multitouch_auto_drive=true` 只作为两名 controller 的初始默认值 |

### 防回归规则

1. 禁止用 `World::getKart(player_id)` 查找本地玩家 kart。
2. 自定义按钮 ID 必须避开 `addButton()` 自动分配的数组下标范围。
3. 运行时状态必须存在 `LocalPlayerController`，不能写回共享 UserConfig。
4. 图标绘制必须读取当前视图 kart 的 controller，不允许失败时默认为另一玩家或全局状态。
5. 每次必须完成编译、ADB 安装、冷启动、双屏按钮操作、双屏截图和日志验证。

详细架构与验收步骤见 `docs/dual_screen_kart_selection.md` 的“6.1 双屏自动驾驶独立操作”和
“自动驾驶回归验收”。

## v1.6.2 (2026-07-21) — 双屏选车稳定性根治 + 启动流程精简 + APP图标完善

### 改动概要

- **双屏选车"点击即确认"回退**：删除 `onSelectionChanged` 中自动确认逻辑，恢复"选中→继续"两步分离；删除渲染帧依赖的 hover 过滤。
- **启动弹窗全部跳过**：三项 user_config 默认值修改，首次启动直接进选角色界面。
- **APP 图标替换为 STK 企鹅**：全 density 替换 + 自适应图删除 + 桌面 SQLite 缓存清理。
- **资产提取自动修复**：`files.txt` 补 1.6 版本标记文件，新装不再崩溃。
- **代码全量入 Git**：`.gitignore` 移除 `android/res` 忽略，全部资源可追溯。

### 用户可见改动

- **点一次头像必选中**：主屏副屏均稳定响应，不需多次点击。
- **点头像不会自动进下一步**：必须点击 Continue 才确认。
- **首次启动无弹窗**：操控选择/驱动警告/联网许可三个弹窗全部跳过，直接进选角色。
- **APP 桌面图标**：显示 STK 企鹅徽标（蓝色圆形+Tux）。
- **比赛中退出**：点击"退出比赛"直接关闭 App，不再返回主菜单。

### 关键改动

#### 1. 双屏事件路由稳定化 (`event_handler.cpp`)

| 改动 | 说明 |
|------|------|
| 删除 `curDisp`-`expectedDev` 匹配过滤 | 原过滤用 `getCurrentDisplayId()` 判断"当前渲染帧"，双屏交替渲染时会把正常 D2 触摸事件丢弃 |
| 新增 mouse 事件更新 `m_last_touch_device` | 确保按钮点击激活路径使用正确的设备号 |
| 简化 ribbon hover 路由 | 仅按 `m_last_touch_device` 判断，不再参考渲染帧 |

#### 2. 选车确认逻辑修复 (`kart_selection.cpp`)

| 改动 | 说明 |
|------|------|
| **删除** `onSelectionChanged` 中自动确认 | 这是之前"副屏偶发不生效"时加的临时补丁，副作用是点头像立刻进下一级 |
| **删除** pid 非法时的 fallback 猜测 | 原逻辑用 `curDisp` 猜测 pid，双屏切换时会串路由；改为直接 return |
| **删除** 每帧重复 `TracksAndGPScreen::syncDisplayWidgets` 调用 | 每帧调两次造成界面抖动，打断事件链路 |
| **删除** 每帧 widget 状态刷屏日志 | 日志瞬间淹没缓冲区，真正有效事件不可见 |

#### 3. 启动弹窗跳过 (`user_config.hpp`)

| 配置项 | 原值 | 新值 | 对应弹窗 |
|--------|------|------|----------|
| `m_multitouch_controls` | 0 (UNDEFINED) | 1 (STEERING_WHEEL) | 操控方式选择 |
| `m_old_driver_popup` | true | false | 显卡驱动警告 |
| `m_internet_status` | 0 (NOT_ASKED) | 2 (NOT_ALLOWED) | 联网/隐私许可 |

注意：已安装设备需删除 `home/supertuxkart/config-0.10/` 目录才生效（旧配置覆盖新默认值）。

#### 4. 资产自动提取 (`android/assets/files.txt`)

- `data/supertuxkart.1.6` 文件在 `assets/data/` 中存在，但 5902 行的 `files.txt` 提取清单中缺失
- 加到 `files.txt` 末尾，确保新安装后自动解压，不再因缺失标记文件 fatal crash

#### 5. Git 仓库完善 (`.gitignore`)

- 删除 `.gitignore` 第 70 行 `android/res` 忽略规则
- `git add -f` 强制追踪 68 个 res 文件（图标、strings、styles、banner）
- 新增 `docs/cl.md` 本变更日志

### 图标问题排查全过程

| 步骤 | 操作 | 结论 |
|------|------|------|
| 1 | 替换 `android/res/drawable*/icon.png` 所有 density | 桌面未更新 |
| 2 | `adb uninstall` + 重装 + 重启设备 | 桌面未更新 |
| 3 | 从 APK 解压验证 MD5 = STK 企鹅源图 | APK 正确 |
| 4 | 从设备 `/data/app/.../base.apk` 提取验证 MD5 | 设备 APK 正确 |
| 5 | `pm list packages` 发现 `com.kart.stk` 旧包 | 旧包占位，卸载 |
| 6 | 发现 `app_icons.db` SQLite 缓存 | 桌面不读 APK，用 DB 里的 BLOB |
| 7 | `DELETE FROM icons` + `pm clear launcher3` | 图标终于更新 |
| 8 | 纯红图标测试验证 DB 缓存机制 | 确认根因 |

**根因**: RK356x 定制 ROM 的 `com.android.launcher3` 将图标 BLOB 存入 SQLite `app_icons.db`，卸载重装、重启、`pm clear` 均不刷新，必须 `DELETE FROM icons`。

### 📁 改动文件明细

| 文件 | 改动类型 | 说明 |
|------|----------|------|
| `src/guiengine/event_handler.cpp` | 修改 | 删除 render-pass 过滤 + 实时 m_last_touch_device |
| `src/states_screens/kart_selection.cpp` | 修改 | 删自动确认 + 删 fallback + 删重复 sync + 删刷屏日志 |
| `src/states_screens/kart_selection.hpp` | 修改 | clearTrackSelectionWaitingState 等 |
| `src/states_screens/dialogs/race_paused_dialog.cpp` | 修改 | Android exit → main_loop->abort() |
| `src/states_screens/track_info_screen.cpp/.hpp` | 修改 | 双屏等待 + EscapePressed 处理 |
| `src/states_screens/tracks_and_gp_screen.cpp/.hpp` | 修改 | 双屏地图解耦 + 等待界面 |
| `src/guiengine/engine.cpp` | 修改 | 等待文字覆盖层 + 计时器 |
| `src/config/user_config.hpp` | 修改 | 三项默认值跳过首次弹窗 |
| `android/assets/files.txt` | 修改 | 补 data/supertuxkart.1.6 |
| `android/res/drawable*/icon*.png` | 替换 | 全 density STK 企鹅 + 删除 icon.xml |
| `.gitignore` | 修改 | 删除 android/res 忽略规则 |
| `docs/cl.md` | 新增 | 本变更日志 |

---

## v1.6.1 (2026-07-21) — 副屏加载动画 + UI 文案标准化

### 改动概要

- **副屏加载动画**：Display 2 从 Presentation 创建到原生渲染就绪之间有 ~6 秒黑屏，现在显示白色脉动圆圈 + 蓝色"KEMI"文字的加载动画，native 就绪后瞬间切换到选车界面。
- **UI 文案标准化**：所有等待提示文案从 C++ 硬编码迁移到 XML `raw_text`，删除 `engine.cpp` 中的硬编码覆盖层。

### 用户可见改动

- **D2 不再黑屏**：从 App 启动到选车界面出现之间，副屏始终显示白色圆圈 + 蓝色"KEMI"脉动动画。
- **无中间过渡帧**：native 就绪后直接进入选车界面，无额外的 C++ 覆盖层过渡。
- **等待提示中文化**：P0/P1 等待界面显示中文文案（"等待副屏玩家确认赛车…"等）。

### 关键改动

| 文件 | 改动 |
|------|------|
| `android/.../LoadingCircleView.java` | **新建**：自定义 View，Canvas 绘制白圆圈 + 蓝 KEMI，脉动动画 |
| `android/.../DualScreenPresentation.java` | 添加 LoadingCircleView（SDLSurface 之上，`bringToFront()`）；`startPollingNativeReady()` JNI 轮询 |
| `android/android_native_dual_screen.cpp` | 新增 JNI `nativeIsD2Ready()`，检查 EGL surface 就绪状态 |
| `src/guiengine/engine.cpp` | **删除**硬编码等待文案覆盖层（曾导致 XML label 不显示）；**删除** C++ 加载画面绘制代码 |
| `src/states_screens/kart_selection.cpp` | **删除** 3 秒加载计时器逻辑和 `g_d2_show_loading_screen` 全局变量 |
| `data/gui/screens/karts.stkgui` | `p0_waiting`/`p1_waiting` 改用 `raw_text` 中文；移除废弃的 `d2_loading` div |
| `data/gui/screens/tracks_and_gp.stkgui` | `waiting_p1` 改用 `raw_text` 中文 |
| `docs/dual_screen_kart_selection.md` | 新增 Section 7（UI 文案标准）、Section 9（加载动画最终方案）、修复记录 #8-#10 |

### 方案演进（加载动画）

| 尝试 | 方案 | 结果 |
|------|------|------|
| 1 | C++ `draw2DLine` 画圆 | Irrlicht 2D API 不可靠，未渲染 |
| 2 | XML `<bubble>` widget | 无 position 警告，label 不支持自定义颜色 |
| 3 | C++ `getTitleFont()->draw()` + 3 秒 timer | 与 Java 层不同步，展示不需要的中间过渡帧 |
| 4 ✅ | **Java `LoadingCircleView`** | Canvas 绘制流畅，JNI 精确同步，动画全程可见 |

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
