# kemi-cart 无脑编译指南

## 一键克隆

```bash
# 代码 + 资源全部下载
git clone --recursive https://github.com/caucy2026/kemi-kart.git

# 如果忘了 --recursive，补拉资源:
git submodule update --init
```

## 仓库结构

| 仓库 | 内容 | 大小 |
|------|------|------|
| `caucy2026/kemi-kart` | C++ 引擎 + Android 构建 | ~600MB |
| `caucy2026/stk-assets` (submodule) | 赛道/卡丁车/贴图/音效 | 724MB |

## 环境要求

| 工具 | 版本 | 说明 |
|------|------|------|
| Android NDK | 26.1.10909125 | 已验证版本 |
| Android SDK | 34 | minSdk 24，targetSdk 34 |
| ImageMagick | 任意 | 用于生成 Android 图标 |
| macOS / Linux | | |

## 编译

```bash
cd android

# 配置 SDK 路径；NDK 应安装在 SDK 的 ndk/26.1.10909125 下
export ANDROID_SDK_ROOT=/path/to/android-sdk

# debug 构建
STK_MIN_ANDROID_SDK=24 STK_TARGET_ANDROID_SDK=34 \
STK_NDK_VERSION=26.1.10909125 \
NDK_PATH="$ANDROID_SDK_ROOT/ndk" SDK_PATH="$ANDROID_SDK_ROOT" \
COMPILE_ARCH=aarch64 bash make.sh

# release 构建
STK_KEYSTORE=/path/to/keystore \
STK_STOREPASS=xxx \
STK_ALIAS=xxx \
PROJECT_VERSION="1.6" PROJECT_CODE="2" \
STK_MIN_ANDROID_SDK=24 STK_TARGET_ANDROID_SDK=34 \
STK_NDK_VERSION=26.1.10909125 \
NDK_PATH="$ANDROID_SDK_ROOT/ndk" SDK_PATH="$ANDROID_SDK_ROOT" \
COMPILE_ARCH=aarch64 BUILD_TYPE=release bash make.sh

# APK 输出: android/build/outputs/apk/release/android-release.apk
```

## 安装测试

```bash
adb install -r android/build/outputs/apk/debug/app-debug.apk
adb shell am start -n org.supertuxkart.stk/.SuperTuxKartActivity

# 双屏截图
adb shell screencap -d 0 -p /sdcard/d0.png
adb shell screencap -d 2 -p /sdcard/d2.png
```

## 目录

```
kemi-cart/
├── src/            C++ 引擎
├── lib/            第三方库 (bullet/irrlicht/sdl2)
├── data/           游戏配置
├── android/        Android 构建系统
│   ├── make.sh         主构建脚本
│   ├── deps.tar.xz     预编译依赖 (14MB)
│   └── assets/data/    构建生成的素材
├── stk-assets/     → submodule: 游戏资源
└── bin/            预编译 APK (不上传 git)
```
