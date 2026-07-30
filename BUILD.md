# kemi-cart 无脑编译指南

## 环境要求

| 工具 | 版本 | 说明 |
|------|------|------|
| Android NDK | **26.x** (必须) | NDK 27/28 有 API 兼容问题 |
| Android SDK | 任意 | build-tools 30+ |
| Java | JDK 17 | |
| macOS / Linux | | |

## 一键编译

```bash
# 1. 配置 NDK 和 SDK 路径
cd android
ln -sfn /path/to/your/android-ndk/r26x android-ndk/28.1.13356709
ln -sfn /path/to/your/android-sdk android-sdk

# 2. 编译 (debug)
bash make.sh

# 3. 编译 (release)
STK_KEYSTORE=/path/to/keystore \
STK_STOREPASS=yourpass \
STK_ALIAS=youralias \
PROJECT_VERSION="1.0.0" \
PROJECT_CODE="1" \
BUILD_TYPE=release \
bash make.sh
```

## 产出

- `android/build/outputs/apk/debug/` — debug APK
- `android/build/outputs/apk/release/` — release APK
- `bin/kemi-cart.apk` — 预编译版本 (157MB, arm64-v8a)

## 目录说明

```
kemi-cart/
├── src/            C++ 游戏引擎源码
├── lib/            第三方库 (bullet, irrlicht, sdl2...)
├── data/           游戏配置
├── android/        Android 构建系统
│   ├── make.sh         主构建脚本
│   ├── Android.mk      NDK 编译配置
│   ├── deps-arm64-v8a/ 预编译依赖
│   └── assets/data/    游戏素材
├── stk-assets/ →   → 指向 ../stk-assets 素材库
└── bin/            预编译 APK
```

## 已知问题

- NDK 27+ 需要 patch: `lib/sdl2/src/sensor/android/SDL_androidsensor.c` 中的 `ALooper_pollAll` → `ALooper_pollOnce`
- 首次编译需要 stk-assets 素材库在 `../stk-assets/`
- 首次编译耗时约 10-30 分钟（取决于机器）
