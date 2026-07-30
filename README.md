# kemi-cart

SuperTuxKart 双屏对战 Android 版。

## 快速开始

```bash
# 安装预编译 APK
adb install -r bin/kemi-cart.apk

# 启动
adb shell am start -n org.supertuxkart.stk/.SuperTuxKartActivity

# 双屏截图验证
adb shell screencap -d 0 -p /sdcard/d0.png
adb shell screencap -d 2 -p /sdcard/d2.png
```

## 特性

- 双屏支持 (Display0 + Display2)
- 触屏操控
- 单人/双人对战
- 多张赛道可选

## 编译

详见 [BUILD.md](BUILD.md)

## 技术栈

- C++ 引擎: SuperTuxKart
- 渲染: Irrlicht + SDL2
- 构建: NDK + Gradle
- 双屏: Android Presentation API
