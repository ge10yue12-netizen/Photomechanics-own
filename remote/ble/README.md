# BLE 模块

外设 GATT 服务，命令格式 `cmd:token`。配置由 **`RemoteKit::loadConfig()`** 一次读入，与 HTTP 共用 `netconfig.ini`。

| 类 | 职责 |
|----|------|
| `BleControlServer` | 对外 API：`start(cfg)` / `stop()` / `commandReceived` |
| `BleWinRtWorker` | WinRT 专用线程 |
| `BleAdapterChecker` | 适配器与外设模式检测 |

## C++14 宿主工程：语言标准规则

WinRT 头 **只出现在下列 3 个 `.cpp`** 中，宿主工程保持 **C++14**，仅这 3 个文件在 `vcxproj` 里标 **C++17**：

| 文件 | 标准 | 原因 |
|------|------|------|
| `BleAdapterChecker.cpp` | C++17 | `#include <winrt/...>` |
| `BleGattServerWin.cpp` | C++17 | `#include <winrt/...>` |
| `WinRtBootstrap.cpp` | C++17 | `#include <winrt/...>` |
| 其余 `ble/*.cpp` 与全部 `.h` | C++14 | 无 WinRT 头 |

`WinRtBootstrap.h` 仅声明函数，**可被 C++14 代码安全 include**。

### vcxproj 片段（复制到工程，生成不会覆盖）

```xml
<!-- 全工程 ItemDefinitionGroup：LanguageStandard = stdcpp14 -->

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

另需：链接 `windowsapp.lib`；附加选项 `/utf-8`；**不要** `/await`。

集成见上级 **`../README.md`**。
