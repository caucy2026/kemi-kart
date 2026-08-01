# STK Android arm64-v8a 编译记录 (macOS + NDK 26.1.10909125)

## 编译命令

```bash
cd /Volumes/ORICO/kemi/kemi-cart/android
/Users/newlink/android-sdk/ndk/26.1.10909125/ndk-build \
  NDK_PROJECT_PATH=. \
  APP_BUILD_SCRIPT=Android.mk \
  APP_ABI=arm64-v8a \
  APP_PLATFORM=android-24 \
  APP_STL=c++_static \
  PROJECT_VERSION=1.5 \
  PACKAGE_NAME=org.supertuxkart.stk \
  APP_DIR_NAME=SuperTuxKart \
  PACKAGE_CLASS_NAME=org/supertuxkart/stk/SuperTuxKartActivity \
  -j4
```

**关键**：`PROJECT_VERSION=1.5` 是必须的！Android.mk 第372行用它传给 `-DSUPERTUXKART_VERSION`，最终定义 `STK_VERSION`（`constants.cpp:33`）。缺少则 STK_VERSION 为空字符串，file_manager 找不到 `supertuxkart.1.5` 版本标记文件而崩溃。

## 依赖库 (23个静态库，均在 deps-arm64-v8a/)

| 库 | 说明 |
|----|------|
| zlib, libpng, freetype, harfbuzz | 字体/图像 |
| openal | 音频 |
| mbedtls (3个: libmbedtls, libmbedcrypto, libmbedx509) | 加密 |
| curl | 网络 |
| libjpeg | JPEG |
| libogg, libvorbis, libvorbisfile | 音频编解码 |
| libsquish | 纹理压缩 |
| astc-encoder (v4.8.0) | ASTC 纹理压缩 |
| shaderc (stub) | Vulkan shader 编译（桩） |
| libadrenotools (stub) | Adreno GPU 工具（桩） |
| mesa (stub) | Mesa 图形（桩） |

## 源码修改清单

### 1. `android/Android.mk` — 移除 ifaddrs
- ifaddrs 模块和引用完全移除（API 24+ 原生支持 `getifaddrs`）
- `LOCAL_STATIC_LIBRARIES` 中移除 ifaddrs
- 原因：macOS NDK 无 ifaddrs 源码，且 API 24 已内置

### 2. `android/make_deps.sh` — macOS 适配
- 添加 `realpath()` shim（macOS 无 realpath 命令，用 python3 替代）
- `linux-x86_64` → `darwin-x86_64`（工具链路径、sysroot 路径）
- `nproc` → `sysctl -n hw.ncpu`（CPU 核心数获取）
- 去掉 `--undefined-version` 链接标志（macOS linker 不支持）
- curl 编译添加 `--without-libpsl --without-libidn2 --without-nghttp2`

### 3. `android/make.sh` — macOS 适配
- 添加 `realpath()` shim
- `nproc` → `sysctl -n hw.ncpu`
- SDL HIDDevice/SDLActivity 拷贝步骤禁用（已有本地修改版）

### 4. `android/build.gradle` — 禁用 externalNativeBuild
- 移除 `externalNativeBuild { ndkBuild { path 'Android.mk' } }` 块
- 原因：手动 ndk-build，不走 Gradle 的 native build 系统

### 5. `lib/graphics_engine/src/ge_compressor_astc_4x4.cpp` — astc-encoder API 修复
- `astcenc_context_alloc(&cfg, 1, &context, nullptr)` → `astcenc_context_alloc(&cfg, 1, &context)`
- 原因：astc-encoder v4.8.0 的 API 从 4 参数改为 3 参数（去掉 thread_count）

### 6. `android/deps-arm64-v8a/shaderc/stub.o` — shaderc 桩
- 手写汇编桩，所有 shaderc 函数返回 calloc 分配的空结构体（非 NULL）
- 编译：`clang -target aarch64-linux-android24 -c stub.s -o stub.o`
- 关键：不能返回 NULL，否则访问 `result->len`（offset 0x18）时 SIGSEGV

### 7. `android/src/main/java/org/supertuxkart/stk/SDLActivity.java` — STK 兼容
- 添加 `reFocusAfterSTKEditText()` 方法
- 添加 `moveView()` 方法
- 双屏支持预留

## APK 打包流程

```bash
# 1. 解压原始 APK
unzip /tmp/stk_original.apk -d /tmp/stk_mix/

# 2. 替换 libmain.so（保留原始 libhook_impl.so, libmain_hook.so, libvulkan_freedreno.so, libSDL2.so）
cp libs/arm64-v8a/libmain.so /tmp/stk_mix/lib/arm64-v8a/

# 3. 删除旧签名
rm -rf /tmp/stk_mix/META-INF/

# 4. 重新打包（resources.arsc 必须不压缩）
zip -r -0 /tmp/stk.apk resources.arsc
zip -r /tmp/stk.apk . -x "*.DS_Store" "resources.arsc" "META-INF/*"

# 5. 对齐 + 签名
zipalign -f -p 4 /tmp/stk.apk /tmp/stk_aligned.apk
apksigner sign --ks ~/.android/debug.keystore --ks-pass pass:android --key-pass pass:android /tmp/stk_aligned.apk

# 6. 安装
adb -s 192.168.3.54:5555 install -r /tmp/stk_aligned.apk
```

## 经验教训

1. **版本宏名**：`SUPERTUXKART_VERSION`（不是 PROJECT_VERSION），定义在 CMakeLists.txt:5，传给 constants.cpp:33 的 `STK_VERSION`
2. **ndk-build 变量传递**：`PROJECT_VERSION=x` 作为 make 变量传给 ndk-build，Android.mk 中 `$(PROJECT_VERSION)` 引用
3. **Vulkan 钩子系统**：原始 APK 有 libhook_impl.so + libmain_hook.so + libvulkan_freedreno.so，必须保留。只替换 libmain.so
4. **shaderc stub**：不能返回 NULL，必须返回有效指针（即使内容为空）
5. **resources.arsc**：打包时必须 `zip -0`（不压缩），否则 Android 11+ 安装失败 (-124)
6. **ifaddrs**：API 24+ 原生支持，不需要单独编译

## 当前状态 (2026-07-19)

- libmain.so: 21.8MB, arm64-v8a
- APK: 145MB (Gradle 构建)，引擎完整运行，含完整游戏 assets
- 目标设备：192.168.1.142:5555，双屏（display 0 + display 2，各 1920x1280）
- GPU: Mali-G52, OpenGL ES 3.2
- 双屏：帧镜像 + 双人自动开赛已跑通

---

# 双屏异显开发全程

## 架构决策

- **方案A（同进程分屏）**：一个 STK 进程，Camera 0→Display 0，Camera 1→Display 2。利用 STK 现有分屏多人代码。
- **Android 层**：`Presentation` API 在 Display 2 创建第二个 SurfaceView
- **C++ 层**：共享 EGL Context，第二个 EGL Surface 独立管理
- **当前阶段**：帧镜像（Display 0 画面拷贝到 Display 2），后续改为独立相机视角

## 关键修改文件

### 新增文件
| 文件 | 用途 |
|------|------|
| `android/src/main/java/org/libsdl/app/DualScreenPresentation.java` | Display 2 的 Presentation 容器 |
| `android/android_native_dual_screen.cpp` | C++ EGL 双屏管理 + JNI + 帧镜像 |
| `android/android_native_dual_screen.h` | 双屏 C API 头文件 |

### 修改文件
| 文件 | 改动 |
|------|------|
| `android/src/main/java/org/libsdl/app/SDLActivity.java` | +双屏字段、`initDualScreen()`、`onSDLRenderingReady()`、STK JNI 方法 |
| `android/src/main/java/org/libsdl/app/SDLSurface.java` | +`mDisplayId` 字段，按 display 路由 surface 回调 |
| `android/src/main/java/SuperTuxKartActivity.java` | 最小化 STK Activity，补齐所有 JNI 方法 |
| `android/build.gradle` | 恢复 `externalNativeBuild`，添加 ndk 构建参数 |
| `android/Android.mk` | +`android_native_dual_screen.cpp`，+`-lEGL -lGLESv2 -landroid` |
| `src/graphics/irr_driver.cpp` | +双屏镜像调用（菜单路径）、+`onSDLRenderingReady` JNI 回调 |
| `src/graphics/shader_based_renderer.cpp` | +双屏镜像调用（比赛路径） |
| `src/main.cpp` | +`startDualScreenRace()` 自动双人开赛 |
| `src/main_android.cpp` | +`ExceptionClear()` 防止 JNI 异常连锁崩溃 |
| `src/io/assets_android.cpp` | +`ExceptionClear()` |
| `lib/irrlicht/source/Irrlicht/CIrrDeviceAndroid.cpp` | +`ExceptionClear()` |

## 调试踩坑记录

### 坑1：绕过 Gradle 手工打包 → files.txt 缺失
- **错误**：`Package doesn't have assets`
- **原因**：手工用 aapt2+zip 拼凑 APK，缺少 `assets/files.txt`（资产清单文件）
- **根因**：`hasAssets()` 检查 `SDL_RWFromFile("has_assets.txt")` — 文件在 APK 根目录，不是 `data/` 下
- **修复**：恢复 Gradle 构建，自动生成所有打包文件。需要 `assets.srcDirs = ['assets']`

### 坑2：Gradle ndk 参数缺失 → JNI 类名错误
- **错误**：`JNI DETECTED ERROR: illegal class name '/SuperTuxKartActivity'`
- **原因**：Gradle 的 `ndkBuild.arguments` 只传了 `APP_PLATFORM, APP_STL, cpu_core`，没传 `PACKAGE_CLASS_NAME`
- **修复**：在 build.gradle 中添加 `PROJECT_VERSION`, `PACKAGE_NAME`, `PACKAGE_CLASS_NAME` 等参数

### 坑3：Java 文件冲突 → 编译失败
- **错误**：`src/main/java/SDLActivity.java` 与 `org/libsdl/app/SDLActivity.java` 重复
- **原因**：STK 项目有两套 Java 源文件（`src/main/java/` 和 `org/libsdl/app/`），部分重复
- **修复**：删除 `src/main/java/` 中与 `org/libsdl/app/` 重复的 SDL 文件，只保留 STK 特有文件

### 坑4：JNI NoSuchMethodError 连锁崩溃
- **错误**：`JNI DETECTED ERROR: CallStaticObjectMethod called with pending exception`
- **原因**：`GetMethodID` 找不到方法时会设置 pending exception。下一个 JNI 调用检测到 pending exception 直接 abort，而不是继续执行
- **模式**：所有 `GetMethodID` 调用即使检查了 NULL 返回值，没检查的 pending exception 仍会传播
- **修复**：每次 `GetMethodID` 失败后调用 `env->ExceptionCheck()` + `env->ExceptionClear()`

### 坑5：Presentation 干扰 SDL 窗口创建
- **错误**：`Could not initialize display!` → Irrlicht 无法创建渲染设备
- **原因**：`initDualScreen()` 在 `onCreate()` 中调用 → 此时 SDL 还未初始化 GL 上下文。Presentation 占用 Display 2 资源导致 SDL 在 Display 0 创建窗口失败
- **修复**：将 `initDualScreen()` 延迟到 SDL/GL 就绪后 → 添加 JNI 回调 `onSDLRenderingReady()`，从 `irr_driver.cpp` 的 `CVS->init()` 后触发

### 坑6：GLES2 无 glBlitFramebuffer → 编译失败
- **错误**：`use of undeclared identifier 'GL_READ_FRAMEBUFFER'`
- **原因**：`glBlitFramebuffer` 是 OpenGL ES 3.0+，我们编译目标是 GLES2
- **修复**：改用 GLES2 兼容方案 — `glCopyTexSubImage2D` 捕获后缓冲 → 全屏 quad 绘制到 Display 2

### 坑7：EGL 创建时机 → Display 2 黑屏
- **症状**：EGL surface 存储在窗口，但 `g_secondReady` 为 false → 镜像函数直接 return
- **原因**：ANativeWindow 在 SDL 上下文就绪前创建，EGL surface 延迟到渲染时才创建。但菜单路径不触发 `dualScreenIsReady()` 重试
- **修复**：`dualScreenMirrorCapture/Present` 内部调用 `dualScreenIsReady()` 主动重试 EGL 创建

### 坑8：getKeyboard(1) NULL → SIGSEGV
- **错误**：`Fatal signal 11 (SIGSEGV), fault addr 0x18` in `InputDevice::getConfiguration()+12`
- **原因**：`startDualScreenRace()` 调用 `getKeyboard(1)` 返回 NULL，未检查即调用方法
- **修复**：添加 NULL 检查，fallback 到 keyboard 0

### 坑9：SuperTuxKartActivity 缺失方法（逐个击破）
- 缺少的方法：`getScreenSize`, `getDisplayDPI`, `showExtractProgress`, `getLocaleString`, `hideSplashScreen`, `showKeyboard`, `hideKeyboard`, `getMovedHeight`, `getKeyboardHeight`, `getDNSSrvRecords`, `getDNSTxtRecords`
- **修复**：创建最小化 `SuperTuxKartActivity` 继承 `SDLActivity`，补齐所有 JNI 需要的 stub 方法 + native 声明

## Gradle 构建参数（最终版）

```bash
cd android
./gradlew assembleDebug \
  -Ppackage_name=org.supertuxkart.stk \
  -Pversion_code=1 \
  -Pversion_name=1.5 \
  -Pcompile_arch=arm64-v8a
```

产出：`build/outputs/apk/debug/android-debug.apk` (145MB)

## 设备信息

| 设备 | IP | Display 0 | Display 2 | GPU |
|------|-----|-----------|-----------|-----|
| huanglong 平板 | 192.168.1.142 | 1920x1280 (内置) | 1920x1280 (外接) | Mali-G52 GLES 3.2 |
| (离线) | 192.168.3.54 | - | - | - |

---

# Git 仓库自包含构建 — .gitignore 踩坑全记录 (2026-07-25)

## 背景

本地 `./gradlew assembleDebug` 能通过，但 `git clone` 后无法编译。根因是 `.gitignore` 中有多条规则排除了编译必需的源文件和预编译库——本地有文件所以能编过，clone 下来就没有。

## 核心原则

> **`git clone` 后只需 `./gradlew assembleDebug` 就能出 APK，不需要任何额外步骤。**

## 被排除的关键文件及修复

### 1. NDK 预编译依赖 `android/deps-*`（~1.23GB）

- **现象**：`ndk-build` 报 `libopenal.a not found`
- **gitignore 规则**：第67行 `android/deps-*` + 第97-100行 `android/deps-arm64-v8a/` 等
- **修复**：移除这些 gitignore 行，提交 `deps-arm64-v8a/` 和 `deps-armeabi-v7a/` 全部内容
- **注意**：deps 中存在嵌套 `.git` 目录（shaderc 第三方子模块构建残留，~159MB），提交前必须 `find -name '.git' -type d -exec rm -rf {} +` 清理

### 2. `*.a` 全局规则误杀

- **现象**：deps 目录已提交，但里面所有 `.a` 文件仍然缺失
- **gitignore 规则**：第36行 `*.a`（通用 C/C++ 编译产物排除规则）
- **修复**：在 `*.a` 行后添加例外：
  ```
  !android/deps-arm64-v8a/**/*.a
  !android/deps-armeabi-v7a/**/*.a
  ```

### 3. `build*/` 规则误杀

- **现象**：`freetype.a` 和 `harfbuzz.a` 仍然缺失
- **gitignore 规则**：第2行 `build*/`
- **影响路径**：`deps-*/freetype/build/` 和 `deps-*/harfbuzz/build/`
- **修复**：在 `build*/` 行后添加例外：
  ```
  !android/deps-*/freetype/build/
  !android/deps-*/harfbuzz/build/
  ```

### 4. mbedtls 嵌套 `.gitignore`

- **现象**：`git add` 拒绝添加 `libmbedtls.a`、`libmbedcrypto.a`、`libmbedx509.a`
- **原因**：`deps-*/mbedtls/library/.gitignore`（上游库自带）包含 `libmbed*` 规则
- **修复**：`git add -f` 强制添加。不要删除上游 `.gitignore`（会影响 deps 重构建）

### 5. `lib/sdl2` 源码（~79MB）

- **现象**：NDK 编译报 `../lib/sdl2/src/atomic/SDL_atomic.c` 找不到
- **gitignore 规则**：第92行 `lib/sdl2`
- **原因**：`Android.mk` 第321行 `-I../lib/sdl2/include/` — SDL2 是从源码编译的，不是预编译 `.a`
- **修复**：从 `.gitignore` 移除 `lib/sdl2`，提交 1,644 个源文件

### 6. `lib/shaderc` 头文件（~1.7MB）

- **现象**：`fatal error: 'shaderc/shaderc.h' file not found`
- **gitignore 规则**：原第94行 `lib/shaderc`
- **原因**：`Android.mk` 第203行 `-I../lib/shaderc/libshaderc/include`
- **修复**：从 `.gitignore` 移除 `lib/shaderc`

## 误判警告

### NDK r26 `fcntl(): Bad file descriptor`

- 这个警告在构建日志中大量出现（原始工作目录也有 ~999 条），但**不影响构建结果**
- 是 NDK r26 在 macOS 上的已知问题，NDK 27+ 已修复
- 若同事要彻底消除此警告，可将 `gradle.properties` 中 `ndk_version` 升级到 `27.0.12077973` 或更高
- 但升级 NDK 可能引入新的编译兼容性问题（如 NDK 28 已移除 `ALooper_pollAll`）

## 最终 gitignore 修改汇总

| 行号 | 操作 | 说明 |
|------|------|------|
| 2 | 添加例外 | `!android/deps-*/freetype/build/` 和 `!android/deps-*/harfbuzz/build/` |
| 36 | 添加例外 | `!android/deps-arm64-v8a/**/*.a` 和 `!android/deps-armeabi-v7a/**/*.a` |
| 67 | 删除 | `android/deps-*` |
| 92 | 删除 | `lib/sdl2` |
| 94 | 删除 | `lib/shaderc` |
| 97-100 | 删除 | `android/deps-arm64-v8a/` 等具体路径（已被第67行覆盖） |

## 自包含验证命令

```bash
# 模拟同事全新 clone 后一键构建
git clone --depth 1 git@github.com:caucy2026/kemi-kart.git /tmp/stk-test
cd /tmp/stk-test/android
./gradlew assembleDebug
# 预期：BUILD SUCCESSFUL，产出 build/outputs/apk/debug/android-debug.apk (~142MB)
```
