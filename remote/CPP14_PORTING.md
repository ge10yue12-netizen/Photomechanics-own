# C++14 工程接入蓝牙远程 — 完整清单

宿主工程 **全程 C++14**；仅 3 个 WinRT 源文件在 `vcxproj` 中设为 C++17。  
**重新生成不会改写 `vcxproj` 里的单文件语言标准**（配置写在 XML 中，非临时 UI 状态）。

---

## 一、复制文件

```text
remote/                 # 整目录
config/netconfig.ini    # 与 HTTP 共用
miniprogram/            # 可选，小程序端
```

---

## 二、加入 VS 工程

### 2.1 编译这些 `.cpp`

```text
remote/NetConfigHelper.cpp
remote/RemoteHost.cpp
remote/RemoteKit.cpp
remote/RemoteControlServer.cpp
remote/ble/BleCommandProtocol.cpp
remote/ble/BleControlServer.cpp
remote/ble/BleWinRtWorker.cpp
remote/ble/WinRtBootstrap.cpp      ← C++17
remote/ble/BleAdapterChecker.cpp   ← C++17
remote/ble/BleGattServerWin.cpp    ← C++17
```

### 2.2 Qt Moc（5 个头文件）

`RemoteHost`、`RemoteKit`、`RemoteControlServer`、`BleControlServer`、`BleWinRtWorker`

### 2.3 Qt 模块

`core`、`network`；有界面加 `gui`、`widgets`

---

## 三、工程配置（关键）

### 3.1 全工程（Debug + Release）

| 项 | 值 |
|----|-----|
| C++ 语言标准 | **ISO C++14** (`stdcpp14`) |
| C/C++ → 命令行 → 其他选项 | `/utf-8`（建议加 Pylon 等项目原有 `/wd` 可保留） |
| 链接器 → 附加依赖项 | `windowsapp.lib` |
| `/await` | **不要加** |

### 3.2 仅 3 个 WinRT 文件 → C++17

**推荐直接编辑 `xxx.vcxproj`**（比属性页更稳，重新生成不会丢）：

```xml
<ClCompile Include="remote\ble\WinRtBootstrap.cpp">
  <LanguageStandard>stdcpp17</LanguageStandard>
</ClCompile>
<ClCompile Include="remote\ble\BleAdapterChecker.cpp">
  <LanguageStandard>stdcpp17</LanguageStandard>
</ClCompile>
<ClCompile Include="remote\ble\BleGattServerWin.cpp">
  <LanguageStandard>stdcpp17</LanguageStandard>
</ClCompile>
```

**不要**给 `BleWinRtWorker.cpp` 设 C++17（已拆到 `WinRtBootstrap.cpp`）。

### 3.3 自检

重新生成后，在输出窗口确认：

- 上述 3 个文件编译命令含 **`/std:c++17`**
- 其余文件含 **`/std:c++14`**

---

## 四、业务代码（最少接入）

```cpp
#include "remote/RemoteHost.h"

RemoteHost m_remote;

m_remote.setStatusProvider([this]() { return buildStatusJson(); });
connect(&m_remote, &RemoteHost::commandReceived, this, &MainWindow::onRemoteCommand);
m_remote.bootstrap();   // 构造或初始化末尾
m_remote.shutdown();    // 析构
```

改命令：只编辑 `remote/RemoteCommands.h`。

---

## 五、Release 验证

1. 配置 **Release | x64** → **重新生成解决方案**
2. 启动 exe，日志蓝牙行应显示 **已启动** 或明确失败原因
3. 手机真机连 BLE，发 `status` 与业务命令

---

## 六、常见错误

| 现象 | 原因 | 处理 |
|------|------|------|
| `wstring_view` / `C2429` | WinRT 文件仍按 C++14 编 | 检查 vcxproj 是否含上述 3 段 XML |
| 只 Debug 能编 | 只给 Debug 设了 C++17 | XML 无 Condition，或属性页选「所有配置」 |
| 改了 UI 仍失败 | 继承勾选，未写入 vcxproj | 直接改 XML |
| 不要任何 C++17 文件 | 硬性政策 | 用 `remote-wifi/`（仅 HTTP） |

---

## 七、设计说明

| 原则 | 说明 |
|------|------|
| WinRT 只进 `.cpp` | 头文件 C++14 安全，避免隐式拖入 C++17 |
| 3 个固定 C++17 文件 | 与代码一一对应，不靠记忆 |
| 混用标准 | MSVC 官方支持，链接为一个 exe，无运行时问题 |
