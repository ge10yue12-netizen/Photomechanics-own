# 遥控套件（HTTP + BLE）— 可移植

复制 **`remote/`** 整目录 + **`config/netconfig.ini`** 到新工程即可。  
与业务代码隔离：协议、传输、状态行文案在 `remote/` 内；宿主工程只接 **`RemoteHost`** 并实现命令处理。

## 目录职责

| 文件 | 移植时 |
|------|--------|
| `RemoteHost.h/.cpp` | **集成入口**，一般不改 |
| `RemoteKit.h/.cpp` | HTTP+BLE 核心，不改 |
| `RemoteCommands.h` | **只改这个**：命令白名单与中文标签 |
| `RemoteStatusText.h` | 状态行「已启动/未启动/失败原因」，不改 |
| `NetConfigHelper.*` | 读 `config/netconfig.ini`，不改 |
| `RemoteControlServer.*` | HTTP 服务，不改 |
| `RemoteControlGuard.*` | 跨通道**命令**互斥；`POST /api/release` 断开即释放 |
| `ble/*` | BLE GATT（仅 Windows），不改 |
| `miniprogram/`（项目根） | 小程序端，按业务改按钮 |

## 配置

`config/netconfig.ini`：

```ini
[remote]
token=1234

[http]
bind=0.0.0.0
port=18765

[ble]
device_name=PhotoMech
```

程序从 exe 目录向上查找第一份 `config/netconfig.ini`。改后重启。

`http/bind` 即监听地址，也是日志与小程序填写的 IP；请填本机 **WiFi 网卡 IPv4**（与手机同网段）。`0.0.0.0` 表示全部网卡监听，但日志会原样显示 `0.0.0.0`，手机无法使用，不推荐。

## VS 工程（C++14 宿主）

完整步骤见 **`CPP14_PORTING.md`**。摘要：

- **全工程 C++14**；仅下列 3 个 `.cpp` 在 `vcxproj` 中设 **C++17**（写进 XML，重新生成不会丢）：
  - `ble/WinRtBootstrap.cpp`
  - `ble/BleAdapterChecker.cpp`
  - `ble/BleGattServerWin.cpp`
- 规则：只有 `.cpp` 里直接 `#include <winrt/...>` 的才要 C++17；所有 `.h` 与其余 `remote/` 源文件保持 C++14
- **Qt 模块**：`core`、`network`（有界面加 `gui`、`widgets`）
- 附加选项 `/utf-8`；链接 `windowsapp.lib`；**无需** `/await`
- 将 `remote/` 下全部 `.cpp` 加入工程；`RemoteKit`、`RemoteHost`、`RemoteControlServer`、`BleControlServer`、`BleWinRtWorker` 走 **Qt Moc**
- 预览（可选）：`RemoteHost::setPreviewProvider()` → GET `/api/preview.jpg`；宿主可共用 `PreviewFrameCache`

## 集成（推荐 RemoteHost）

```cpp
#include "remote/RemoteHost.h"

// 成员
RemoteHost m_remote;
RemoteControlGuard m_remoteGuard;
QLabel *m_bleStatusLabel = nullptr;
QLabel *m_httpStatusLabel = nullptr;

// 构造里（可选：日志区两行状态标签）
m_remote.setControlGuard(&m_remoteGuard);
m_remote.setPreviewProvider([]() { return previewCache.getLatestJpeg(); }); // 可选
m_remote.setStatusLabels(m_bleStatusLabel, m_httpStatusLabel);
m_remote.setStatusProvider([this]() { return buildStatusJson(); });
connect(&m_remote, &RemoteHost::commandReceived, this, &MainWindow::onRemoteCommand);
connect(&m_remote, &RemoteHost::notify, this, [this](const QString &msg) { log(msg); });

if (!m_remote.bootstrap()) { /* 配置加载失败 */ }
for (const QString &line : m_remote.startupWarnings())
    log(line);

// 析构 / closeEvent
m_remote.shutdown();

// 命令执行后
m_remote.pushBleStatus();
```

### 宿主只需实现

1. **`buildStatusJson()`** — 返回给手机的状态 JSON  
2. **`onRemoteCommand(const QString &cmd)`** — 命令 → 业务逻辑  
3. **改 `RemoteCommands.h`** — 命令名与小程序一致  

## 自定义命令

编辑 **`remote/RemoteCommands.h`**：

```cpp
inline const QSet<QString> &knownCommands() {
    static const QSet<QString> kTable = {
        QStringLiteral("start_calc"),
        QStringLiteral("clear"),
        QStringLiteral("status")};
    return kTable;
}
```

同步修改：小程序 `CMD_LABELS`、`onRemoteCommand` 分支。

## 状态行

`RemoteHost` 自动刷新标签：

- 成功 → **已启动**
- 未运行 → **未启动**
- 失败 → **失败原因**（中文）

`startupWarnings()` 仅返回失败项，供写日志。

## 低级 API（可选）

不用 `RemoteHost` 时可直接用 `RemoteKit`（自行刷新 UI、写日志）。

## 手机端

`miniprogram/utils/` 分层：

| 文件 | 职责 |
|------|------|
| `remote-buttons.js` | 命令名与按钮禁用规则（WiFi/BLE 共用） |
| `wifi-link.js` / `ble-link.js` | 模式链路封装，互不交叉 |
| `http.js` / `ble.js` / `protocol.js` / `errors.js` | 传输层，与 PC 协议对齐 |

业务页面在 `pages/control/`；切换模式时会断开另一链路。

## 冲突说明

- 类型均带 `Remote` 前缀，无全局宏  
- 命令校验仅在 `RemoteCommands.h`，不与宿主符号冲突  
- BLE UUID 在 `ble/BleProtocol.h` 与 `miniprogram/utils/protocol.js`；若修改须两端一致
