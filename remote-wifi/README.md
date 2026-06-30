# WiFi 遥控套件（C++14 可移植版）

从完整版 `remote/` 剥离的 **纯 HTTP/WiFi** 实现，不含 BLE、WinRT、`windowsapp.lib`。  
适用于目标工程 **全局 C++14**、无法按文件设 C++17 的场景。

## 交付物

```text
remote-wifi/
  RemoteHost.h/.cpp          集成门面（推荐入口）
  RemoteKit.h/.cpp           HTTP 核心
  RemoteControlServer.h/.cpp   QTcpServer HTTP 服务
  NetConfigHelper.h/.cpp       读取 config/netconfig.ini
  RemoteCommands.h             命令白名单（移植时主要改这里）
  RemoteStatusText.h           状态行文案（header-only）
  config/netconfig.ini         运行参数样本
  README.md                    本文件
  PORTING_AUDIT.md             审计与可移植性验证报告
```

小程序仍使用项目根 `miniprogram/`，选择 **WiFi 模式** 即可对接。

## 环境要求

| 项 | 要求 |
|----|------|
| C++ | **C++14**（全局即可，无需按文件例外） |
| Qt | 5.12+（推荐 5.15.2）：`core`、`network`；有界面加 `gui`、`widgets` |
| 编译器 | MSVC 2017+ 或 GCC 7+；建议 `/utf-8` |
| 平台 | Windows / Linux / macOS（纯 Qt Network，无 WinRT） |
| 链接库 | 仅 Qt 模块，**不需要** `windowsapp.lib` |

## VS 工程配置

### 源文件（全部加入工程）

```text
remote-wifi/NetConfigHelper.cpp
remote-wifi/RemoteHost.cpp
remote-wifi/RemoteKit.cpp
remote-wifi/RemoteControlServer.cpp
```

### Moc 类

`RemoteHost`、`RemoteKit`、`RemoteControlServer`

### 编译选项

| 配置项 | 值 |
|--------|-----|
| C++ 语言标准 | ISO C++14 (`/std:c++14` 或 `stdcpp14`) |
| 附加选项 | `/utf-8`（推荐） |
| Qt 模块 | `QT += core network`（有 UI 时加 `gui widgets`） |

### 部署

将 `config/netconfig.ini` 放到 exe 同级或上级目录（程序向上最多查 8 层）。

## 集成示例

```cpp
#include "remote-wifi/RemoteHost.h"

RemoteHost m_remote;
QLabel *m_httpStatusLabel = nullptr;

m_remote.setStatusLabel(m_httpStatusLabel);
m_remote.setStatusProvider([this]() { return buildStatusJson(); });
connect(&m_remote, &RemoteHost::commandReceived, this, &MainWindow::onRemoteCommand);
connect(&m_remote, &RemoteHost::notify, this, [this](const QString &msg) { log(msg); });

if (!m_remote.bootstrap()) { /* 配置或监听失败 */ }
for (const QString &line : m_remote.startupWarnings())
    log(line);

// 析构或 closeEvent
m_remote.shutdown();
```

### 与完整版 `remote/RemoteHost` 的差异

| 完整版 API | WiFi 版 API |
|------------|-------------|
| `setStatusLabels(ble, http)` | `setStatusLabel(http)` |
| `pushBleStatus()` | 无（HTTP 由小程序轮询） |
| `bootstrap()` 任一通道成功即可 | 仅 HTTP 成功 |

## 宿主须实现

1. **`buildStatusJson()`** — 返回状态 JSON（字段须满足小程序 `computeBtnState`）
2. **`onRemoteCommand(const QString &cmd)`** — 命令分发
3. **改 `RemoteCommands.h`** — 命令名与小程序 `remote-buttons.js` 一致

## HTTP 接口

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/api/status?token=...` | 查询状态 |
| POST | `/api/command` | body: `{"cmd":"...","token":"..."}` |

## 自定义命令

编辑 `remote-wifi/RemoteCommands.h` 的 `knownCommands()`，并同步小程序 `miniprogram/utils/remote-buttons.js`。

## 与完整 remote/ 的选择

| 场景 | 选用 |
|------|------|
| 目标工程 C++14、不要 BLE | **remote-wifi/** |
| 需要蓝牙近场遥控 | `remote/`（BLE 部分须 C++17） |
| 两种都要且工程可 C++17 | `remote/` 完整版 |

详细审计见 [PORTING_AUDIT.md](PORTING_AUDIT.md)。
