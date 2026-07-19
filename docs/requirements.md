# STK 双屏对战 v1.1.0 — 需求文档

> 基于完整聊天记录整理，2026-07-18 ~ 2026-07-19
> 当前版本：v1.1.0 统一触控架构

---

## 核心目标

**双屏异显卡丁车对战**：一块 RK356x 平板（huanglong），内置 Display 0 + 外接 Display 2，各 1920×1280。
两个玩家各看一个屏幕，独立操作，同场竞技。

---

## 功能需求

### F1. 双屏渲染架构
- Display 0 渲染 Player 1 的 Camera 0 视角（全屏）
- Display 2 渲染 Player 2 的 Camera 1 视角（全屏）
- 非镜像！两个屏幕看到的是各自玩家的独立视角
- 帧率目标：**双屏都是 60Hz**，不卡顿

### F2. 操控方式 — 虚拟方向盘
- **两个屏幕都默认使用虚拟方向盘操作**，不要弹出选操控方式的界面
- Display 0 触控 → Player 1 的方向盘（左转/右转/加速）
- Display 2 触控 → Player 2 的方向盘（左转/右转/加速）
- 虚拟方向盘要在副屏上也看得见！
- 两边触摸互不干扰

### F3. 选单流程
- **主屏 (Display 0)**：选人物/赛车 → 选地图 → 点击"开始游戏"（创建房间）
- **副屏 (Display 2)**：选人物/赛车 → 如果主屏还没创建地图，显示"等待中" → 主屏建好地图后，显示"加入"按钮 → 点击加入
- 精简版：跳过太复杂的选单，默认赛车+随机地图也可以接受

### F4. 比赛流程
- 双方都加入地图后，**3-2-1 倒计时**开始比赛
- 两屏同时看到倒计时
- 比赛结束显示排名

### F5. 架构约束
- 复用 STK 原有架构（RaceManager、PlayerManager、DeviceManager）
- 同进程方案：一个 STK 进程渲染两个 Camera
- Android 层用 Presentation API 管理 Display 2 的 SurfaceView
- C++ 层用共享 EGL Context，第二个 EGL Surface 独立管理

---

## 技术实现状态

| 功能 | 状态 | 说明 |
|------|:--:|------|
| F1 双屏独立视角 | ✅ | Camera 1 直出 Display 2 的 EGL surface |
| F2 虚拟方向盘(两屏) | ✅ | 两屏都有，用 kart->getSteer() 独立渲染 |
| F2 触控分离 | ✅ | SDL 统一路径，按 routedDevId 分流 P0/P1 |
| F2 道具按钮 | ✅ | FIRE/NITRO/DRIFT/LOOK_BACK 各屏独立 |
| F2 双屏 HUD | ✅ | minimap + timer + player list 全部两屏渲染 |
| F3 选单流程 | 🔄 | 赛车赛道每局自动轮换（实用替代），完整选单待做 |
| F4 3-2-1 倒计时 | ✅ | 两屏都渲染 drawGlobalReadySetGo |
| F4 比赛结束 | ✅ | 两屏都渲染 drawGlobalGoal |
| F5 架构复用 | ✅ | 两屏共用 SDL→Irrlicht→STK 标准触控链路 |

---

## 架构演进

### v1.0 → v1.1 重大重构
- **废弃**：Display 2 的 JNI 触控路径（~250行手工代码）
- **统一**：两屏都走 SDL→Irrlicht→STK→MultitouchDevice 标准链路
- **原理**：SDL 原生支持多触控设备，`SDL_androidtouch.c` 按 `touchDeviceId` 区分
- **关键改动**：`STouchInput` + `DeviceID`（4行）+ `CIrrDeviceSDL` 传递（1行）+ `InputManager` 分流（5行）

### 踩坑教训（详见 cl.md）
1. 手工复刻框架逻辑必然有 bug（press/release/zone 极其复杂）
2. 硬编码坐标在不同分辨率下必然出错
3. C++ 变量遮蔽（内层重声明覆盖外层）是经典陷阱
4. 统一代码路径 >> 重复实现（删250行换5行核心改动）

## 下一步优先级

### P0 — 验证
1. 实机测试两屏完全独立操控（方向盘 + 道具 + 后视镜）
2. 道具收集/使用端到端测试
3. 帧率验证（60Hz）

### P1 — 选单
4. 选车/选赛道界面
5. 主屏建房间 → 副屏等待 → 加入

### P2 — 完善
6. 3-2-1 倒计时双屏同步验证
7. 比赛结束排名展示
8. 暂停功能

---

## 设备信息

| 项目 | 值 |
|------|-----|
| 型号 | huanglong 平板 (RK356x) |
| IP | 192.168.1.142:5555 |
| GPU | Mali-G52, OpenGL ES 3.2 |
| Display 0 | 1920×1280 内置 |
| Display 2 | 1920×1280 外接 |
| Android | 12, API 31 |
| 编译 | JDK 17, NDK 26.1, Gradle 8.9 |

---

## Git 版本

| Commit | 内容 |
|--------|------|
| `2aecf9d` | v1.1.0 文档 + 角色反转修复 |
| `88ecc1c` | 统一触控架构（SDL 标准路径） |
| `50afc73` | 触控经验沉淀 + 视觉独立修复 |
| `0b7dd9d` | 触控分离 |
| `2b2bc70` | 独立相机视角 |
| `17b3326` | 帧镜像 + 双人自动开赛 |

**GitHub**：https://github.com/caucy2026/kemi-kart
