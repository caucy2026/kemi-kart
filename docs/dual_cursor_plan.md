# 双光标实现计划

## 目标
两屏选单界面独立操作：各自的光标、各自的焦点、各自的确认。

## 原理
SDL 的 Finger 事件携带 `touchId`（已在 `CIrrDeviceSDL` 写入 `TouchInput.DeviceID`）。
`CIrrDeviceStub::postEventFromUser` 将 Touch 转 Mouse 时丢失了这个 ID——只维护了单光标。
修复：让这个转换保留 DeviceID，维护两个光标。

## 改动清单

### Step 1: SMouseInput + DeviceID
- 文件: `lib/irrlicht/include/IEventReceiver.h`
- 在 `SMouseInput` 增加 `size_t DeviceID;`

### Step 2: CIrrDeviceStub 双光标
- 文件: `lib/irrlicht/source/Irrlicht/CIrrDeviceStub.cpp`
- `mouse_pos` 改为 `mouse_pos[2]` 数组
- 从 `event.TouchInput.DeviceID` 读设备号，选对应光标
- 设置 `irrevent.MouseInput.DeviceID = event.TouchInput.DeviceID`

### Step 3: STK EventHandler 按 DeviceID 跟踪光标
- 文件: `src/guiengine/event_handler.cpp`
- `m_mouse_pos` 改为 `m_mouse_pos[2]`
- EET_MOUSE_INPUT_EVENT 时用 DeviceID 更新对应光标
- EET_TOUCH_INPUT_EVENT 时同样处理

### Step 4: GUI 焦点按 DeviceID
- 文件: `src/guiengine/event_handler.cpp`
- 传 DeviceID 给 Irrlicht GUI `postEventFromUser`

### Step 5: 编译验证
- 编译 arm64-v8a
- 部署到 192.168.1.142
- 启动测试选单触摸是否独立

## 验证标准
- [ ] Display 0 触摸 → 仅 Display 0 的光标/焦点变化
- [ ] Display 2 触摸 → 仅 Display 2 的光标/焦点变化
- [ ] 同时触摸两屏 → 互不干扰
- [ ] 比赛阶段方向盘操控不受影响（回归测试）

## 不改的
- STK KartSelectionScreen 逻辑
- PlayerKartWidget 逻辑
- 渲染层
- 比赛触控链路
