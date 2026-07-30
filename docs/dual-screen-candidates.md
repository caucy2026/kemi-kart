# RK356x 双屏平台 — 3D开源项目推荐

> 设备：RK356x / Mali-G52 / OpenGL ES 3.2 / Android 12 / 双屏 1920×1280
> 筛选标准：C/C++ 引擎、已有或可适配 Android NDK、有联网/多人基础（双屏拆分基础）、适合 Mali-G52 移动 GPU

---

## 🥇 第一梯队：已适配 Android，双屏移植风险低

### 1. Xash3D FWGS（反恐精英/半条命引擎）

| 维度 | 详情 |
|------|------|
| **GitHub** | [FWGS/xash3d-fwgs](https://github.com/FWGS/xash3d-fwgs) ⭐ 2.6k |
| **类型** | FPS 动作探索 |
| **语言** | C (95%) + Kotlin (Android UI) |
| **渲染器** | OpenGL + GLESv1 + GLESv2 + 软件渲染器（4 选 1） |
| **Android** | ✅ Gradle 构建，Google Play 可下载 |
| **多人** | ✅ 完整 CS 1.6 协议，专用服务器、语音、观战 |
| **代码量** | ~20 万行，结构清晰 |
| **双屏适配难度** | 🟢 **低** — 已有 ref_gl 多渲染后端，加双 viewport 即可 |

**双屏对战场景**：P0/P1 各看一屏，同场竞技（死亡竞赛/团队）。Half-Life 单机剧情可改合作模式。

**移植要点**：
- 已有 `ref_gl/` 和 `ref_soft/` 渲染后端架构 → 仿照加 `ref_dual`
- Mobility API 已有触摸控制 → 按 Display ID 分流即可
- Half-Life 游戏资源需用户自有（版权原因引擎不包含）

---

### 2. Luanti（原名 Minetest，体素沙盒）

| 维度 | 详情 |
|------|------|
| **GitHub** | [luanti-org/luanti](https://github.com/luanti-org/luanti) ⭐ 13.3k |
| **类型** | 体素沙盒 / 建造探索 |
| **语言** | C++ (81%) + Lua (10%) + Java (2%) |
| **渲染器** | Irrlicht 引擎 fork → OpenGL ES |
| **Android** | ✅ Gradle 构建，Google Play / F-Droid |
| **多人** | ✅ 完整 Client-Server，Lua mod 生态 |
| **代码量** | ~30 万行 + Irrlicht fork |
| **双屏适配难度** | 🟢 **低-中** — 已有 Android GLES 渲染，Irrlicht 引擎与 STK 同类 |

**双屏对战场景**：两个玩家在同一世界建造/探索/PvP。可做 Lua mod 实现双屏专属玩法（合作闯关、竞速建造等）。

**移植要点**：
- 渲染引擎是 Irrlicht fork — STK 也基于 Irrlicht，可复用双屏 Irrlicht 适配经验
- Lua API 可实现双屏游戏逻辑原型，无需改 C++
- 体素渲染轻量，Mali-G52 上单屏跑 60fps 无压力

---

### 3. ioquake3（雷神之锤3 引擎）

| 维度 | 详情 |
|------|------|
| **GitHub** | [ioquake/ioq3](https://github.com/ioquake/ioq3) ⭐ 2.8k |
| **类型** | FPS 竞技场 |
| **语言** | C (97%) |
| **渲染器** | OpenGL 1.x + OpenGL 2.x/ES 2+ (`renderergl2`) |
| **Android** | ⚠️ SDL2 可移植，无现成 Gradle 项目 |
| **多人** | ✅ 经典客户端-服务器，专用服务器 |
| **代码量** | ~15 万行（三候选中最紧凑） |
| **双屏适配难度** | 🟡 **低-中** — 已有立体渲染 (`r_stereoEnabled`)，可扩展为双 viewport |

**双屏对战场景**：经典死亡竞赛/夺旗，两屏各一个玩家视角。

**移植要点**：
- 立体渲染代码 (`r_stereoEnabled`) 是双 viewport 的基础
- 需先做 SDL2 Android 封装（写一个 SDLActivity）
- C 代码极紧凑，是学习 3D 引擎双屏改造的好教材

---

## 🥈 第二梯队：RPG/探索类，移植难度中-高

### 4. OpenMW（上古卷轴3：晨风 引擎）

| 维度 | 详情 |
|------|------|
| **GitHub** | [OpenMW/openmw](https://github.com/OpenMW/openmw) ⭐ 6.5k |
| **类型** | 开放世界 RPG 探索 |
| **语言** | C++ (93%) |
| **渲染器** | OpenSceneGraph (OSG) → OpenGL / GLES |
| **Android** | ✅ 有 Android 构建，Google Play 可下载 |
| **多人** | ⚠️ 官方无，社区 fork [TES3MP](https://github.com/TES3MP/TES3MP) ⭐ 856 支持 |
| **代码量** | ~50 万行（OSG + Bullet 物理 + MyGUI + FFmpeg） |
| **双屏适配难度** | 🔴 **中-高** — 代码量大，但已有立体渲染 (`components/stereo/`) |

**双屏探索场景**：两个玩家在同一开放世界独立探索/合作战斗。TES3MP 服务端加上双屏客户端可实现真正双人 RPG。

**移植要点**：
- `components/stereo/multiview.cpp` — 已有 VR 立体渲染，多 viewport 基础存在
- 需先集成 TES3MP 的网络层以支持双人
- OSG 渲染管线复杂，改动需深入理解其 cull/draw 遍历
- **最大优势是真正的 3D RPG**，这是其他候选不具备的

---

### 5. GemRB（博德之门/异域镇魂曲 引擎）

| 维度 | 详情 |
|------|------|
| **GitHub** | [gemrb/gemrb](https://github.com/gemrb/gemrb) ⭐ 1.2k |
| **类型** | 等距 RPG（博德之门、冰风谷、异域镇魂曲） |
| **语言** | C++ (79%) + Python (18%) |
| **渲染器** | SDL2 → OpenGL ES |
| **Android** | ⚠️ 有 ndk-build，非 Gradle |
| **多人** | ❌ 无 |
| **双屏适配难度** | 🔴 **中** — 等距 2D，双屏价值有限 |

> 等距 RPG 双屏意义不大，除非想做"双人桌游"模式。

---

## 汇总对比

| 排名 | 项目 | 星级 | 类型 | Android | 多人 | 降级到 GLES | 代码量 | 双屏难度 |
|:--:|------|:--:|------|:--:|:--:|:--:|:--:|:--:|
| 🥇 | **Xash3D** | 2.6k | FPS探索 | ✅ Gradle | ✅ 完整 | ✅ GLESv2 | 20万行 | 🟢 低 |
| 🥈 | **Luanti** | 13.3k | 体素沙盒 | ✅ Gradle | ✅ C/S | ✅ GLES | 30万行 | 🟢 低-中 |
| 🥉 | **ioquake3** | 2.8k | FPS竞技 | ⚠️ SDL2 | ✅ 完整 | ✅ GLESv2 | 15万行 | 🟡 低-中 |
| 4 | **OpenMW** | 6.5k | 开放RPG | ✅ 有 | ⚠️ TES3MP | ✅ GLES | 50万行 | 🔴 中-高 |

---

## 按用户场景推荐

| 想要的效果 | 推荐 | 理由 |
|-----------|------|------|
| **快出成果验证双屏** | Xash3D | Android 开箱即用，GLES 稳妥，多人协议成熟 |
| **体素建造+探索** | Luanti | 社区最大，Lua 可快速原型双屏玩法 |
| **最小代码量学习** | ioquake3 | 15 万行 C，是理解 3D 引擎双屏改造的最佳教材 |
| **真正的 3D RPG** | OpenMW | 唯一的开放世界 RPG，已有立体渲染基础 |

---

## 双屏移植共性路径（与 STK 经验对应）

```
STK 双屏改造 → 可复用到新项目
─────────────────────────────────
D2 Activity         → AndroidManifest 注册 + setLaunchDisplayId
子 SurfaceControl   → 副屏方向固定（如需要）
EGL 双 Surface      → 共享 Context + 两个 Surface
Camera 独立         → 两个 Viewport/Camera
触控 DeviceID 分流  → InputManager 路由
选单双光标          → GUI 焦点隔离
fence 隔离          → 每 Camera 独立同步
```

所有候选都基于 SDL2 或类似架构，触控分离路径与 STK 一致。Xash3D 和 Luanti 已有 Gradle 构建，可直接 fork 后按上述路径改造。

---

> 📅 2026-07-25 | 基于 GitHub API 实时数据
