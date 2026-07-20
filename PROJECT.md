# SuperTuxKart Android 双屏对战 — 项目说明

> RK356x 平板（huanglong），Display 0 + Display 2 双屏异显卡丁车对战。
> 基于 [SuperTuxKart](https://supertuxkart.net/) 开源项目（GPLv3），Android NDK arm64-v8a 移植。

## 项目文档索引

### 根目录文档

| 文档 | 作用 |
|------|------|
| [`PROJECT.md`](./PROJECT.md) | **本文档** — 项目总说明，文档索引 |
| [`README.md`](./README.md) | 上游 SuperTuxKart 官方 README（英文），含硬件要求、License、编译说明 |
| [`INSTALL.md`](./INSTALL.md) | 上游编译安装详细指南 |
| [`CLAUDE.md`](./CLAUDE.md) | LLM 编程行为规范（上游），偏减少低级错误 |
| [`CHANGELOG.md`](./CHANGELOG.md) | 上游版本更新日志 |
| [`chip.md`](./chip.md) | **芯片平台 & 双屏开发实战手册** — RK356x/V900 芯片特性、双屏架构、ADB 调试、签名部署、性能优化、踩坑记录 |
| [`dual_screen_kart_selection.md`](./dual_screen_kart_selection.md) | **双屏独立选车与大厅架构文档** — 选车流程、双方确认后 P0 选地图、同步开赛、触控隔离与已知陷阱 |

### docs/ 目录文档

| 文档 | 作用 |
|------|------|
| [`docs/requirements.md`](./docs/requirements.md) | **双屏对战需求文档 v1.1.0** — 功能需求（F1-F5）、非功能需求、验收条件 |
| [`docs/cl.md`](./docs/cl.md) | **版本变更记录** — v1.1.0 统一触控架构、双 MultitouchDevice、HUD 完整渲染 |
| [`docs/stk-android-build.md`](./docs/stk-android-build.md) | **Android 编译记录** — NDK 26.1 + arm64-v8a 编译命令、依赖库清单、关键参数说明 |
| [`docs/dual_cursor_plan.md`](./docs/dual_cursor_plan.md) | **双光标实现计划** — SMouseInput DeviceID、CIrrDeviceStub 双光标、EventHandler 按设备跟踪 |

### 内存备忘

| 文档 | 作用 |
|------|------|
| `/memories/repo/dual_screen_kart_selection.md` | 双屏选车架构（副本，主文档在根目录） |
| `/memories/repo/detailControl.md` | detailControl 参数调优（Go3DGlobe 项目） |

## 文档阅读顺序建议

### 新手入门
1. [`README.md`](./README.md) — 了解 STK 项目
2. [`chip.md`](./chip.md) — 了解硬件平台和开发环境
3. [`docs/stk-android-build.md`](./docs/stk-android-build.md) — 搭编译环境

### 开发双屏功能
1. [`docs/requirements.md`](./docs/requirements.md) — 明确要做什么
2. [`dual_screen_kart_selection.md`](./dual_screen_kart_selection.md) — 选车流程架构和踩坑
3. [`docs/cl.md`](./docs/cl.md) — 版本变更，理解架构演进
4. [`docs/dual_cursor_plan.md`](./docs/dual_cursor_plan.md) — 双光标实现细节

## 技术栈

| 层 | 技术 |
|----|------|
| 语言 | C++17, Java (Android 壳) |
| 引擎 | Irrlicht (GLES2 驱动) |
| 构建 | NDK 26.1.10909125 arm64-v8a + Gradle 8.9 |
| 图形 | OpenGL ES 3.0+, Mali-G52 GPU |
| 触控 | SDL Touch → Irrlicht STouchInput → STK MultitouchDevice |
| 双屏 | Display 0（内置）+ Display 2（外接 HDMI），Presentation API |

## 快速命令

```bash
# 编译
cd android && ./gradlew assembleDebug

# 安装 & 启动
adb connect 192.168.3.54:5555
adb install -r build/outputs/apk/debug/android-debug.apk
adb shell am start -n org.supertuxkart.stk/.SuperTuxKartActivity

# 截屏验证（D0 和 D2 分别截）
adb shell screencap -d 0 -p /sdcard/d0.png && adb pull /sdcard/d0.png
adb shell screencap -d 2 -p /sdcard/d2.png && adb pull /sdcard/d2.png

# 模拟副屏触控
adb shell input -d 2 swipe 500 600 500 600 100
```
