# BLE 遥控模块（可移植）

将本目录与 `remote/RemoteCommandList.h` 一并复制到目标 Qt 工程即可复用。

## PC 端

| 类 | 职责 |
|----|------|
| `BleControlServer` | 对外 API：`start()` / `stop()` / `commandReceived` |
| `BleAdapterChecker` | 检测 Windows 默认蓝牙适配器、MAC、外设模式 |
| `BleWinRtWorker` | WinRT GATT 专用线程（勿在主线程调 WinRT） |
| `BleConfigHelper` | `config/netconfig.ini（[ble] 段）`（与 HTTP 同目录，`NetConfigHelper::configDirPath()`） |

启动成功后 `adapterInfo().address` 为当前 **Windows 默认适配器** MAC。USB 蓝牙棒若未设为系统默认，则不会用到。

## 手机端

复制 `miniprogram/utils/protocol.js` 与 `ble.js`。交互流程：

1. **刷新设备列表**（仅扫描，不自动连接）
2. 用户 **点选设备**
3. 校验 GATT 服务 UUID 后进入已连接状态

微信小程序 **不能** 调用系统蓝牙配对界面，这是平台限制，不是实现疏漏。

## 诊断对照

| 现象 | PC 端 | 手机端 |
|------|-------|--------|
| 适配器不支持外设 | 日志：不支持 BLE 外设模式 | — |
| 未启动 GATT | 日志：启动失败 | 连接后：未找到遥控服务 |
| 扫不到设备 | — | 开蓝牙/定位、靠近 PC |
| token 错误 | 日志：token 无效 | 命令无响应或报错 |

## UUID

见 `BleProtocol.h`，须与小程序 `protocol.js` 一致。
