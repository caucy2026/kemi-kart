# STK 双屏异显 — 版本记录

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
