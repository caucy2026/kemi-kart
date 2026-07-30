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
| `minghuadev/stk-assets` (submodule) | 赛道/卡丁车/贴图/音效 | 724MB |

## 环境要求

| 工具 | 版本 | 说明 |
|------|------|------|
| Android NDK | 26.x | NDK 27+ 需 patch SDL2 |
| Android SDK | 任意 | build-tools 30+ |
| macOS / Linux | | |

## 编译

```bash
cd android

# 配置 NDK/SDK 路径
ln -sfn /path/to/ndk-r26x android-ndk/28.1.13356709
ln -sfn /path/to/android-sdk android-sdk

# debug 构建
bash make.sh

# release 构建
STK_KEYSTORE=/path/to/keystore \
STK_STOREPASS=xxx \
STK_ALIAS=xxx \
PROJECT_VERSION="1.0.0" PROJECT_CODE="1" \
BUILD_TYPE=release bash make.sh
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
