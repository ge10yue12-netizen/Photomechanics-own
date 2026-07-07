# remote-qr 扫码浏览器遥控 — 移植与开发说明

| 项 | 说明 |
|----|------|
| 适用版本 | C++14 · Qt 5.15 Widgets |
| 通信方式 | 局域网 HTTP（`QTcpServer` 短连接） |
| 客户端 | 手机/平板浏览器（H5 单页，无小程序依赖） |
| 集成入口 | `MobileHost`（唯一必需对外类） |

本文档面向**二次开发与移植**：说明模块划分、实现位置、HTTP 契约及宿主接入方式。文中示例采用虚构宿主 `DeviceHostWindow`，不绑定任何特定产品工程。

---

## 目录

1. [系统概述](#1-系统概述)
2. [架构与模块](#2-架构与模块)
3. [目录与源文件职责](#3-目录与源文件职责)
4. [运行时序](#4-运行时序)
5. [HTTP 接口规范](#5-http-接口规范)
6. [状态 JSON 契约](#6-状态-json-契约)
7. [命令映射与宿主处理](#7-命令映射与宿主处理)
8. [配置文件](#8-配置文件)
9. [宿主集成](#9-宿主集成)
10. [H5 页面二次开发](#10-h5-页面二次开发)
11. [可选 PC 端组件](#11-可选-pc-端组件)
12. [工程与编译配置](#12-工程与编译配置)
13. [移植检查清单](#13-移植检查清单)
14. [故障排查](#14-故障排查)

---

## 1. 系统概述

### 1.1 功能边界

| 层级 | 职责 |
|------|------|
| **remote-qr 库** | 会话 token、HTTP 服务、预览 JPEG 缓存、H5 资源分发、命令路由 |
| **宿主应用** | 设备状态 JSON、命令业务实现、相机帧写入预览缓存 |
| **mobile.html** | 浏览器 UI、轮询状态/预览、按钮调用 REST API |

### 1.2 访问方式

会话建立后，客户端访问 URL 格式为：

```text
http://{展示用本机IPv4}:{port}/mobile?token={UUID}
```

- `{展示用本机IPv4}`：由宿主或 `RemoteControlDialog` 选定，写入二维码/链接；HTTP 服务绑定 `0.0.0.0`。
- `{token}`：会话级 UUID，除 `/favicon.ico` 外所有请求均须在 Query 中携带 `token=`。

### 1.3 与 `remote/` 套件的关系

| 组件 | 路径 | 是否必需 |
|------|------|----------|
| 命令互斥 | `remote/RemoteControlGuard.h/.cpp` | 推荐（多客户端并存时必需） |
| WiFi 小程序 HTTP | `remote/RemoteHost.*` | 否，与扫码通道独立 |
| BLE | `remote/ble/*` | 否 |

`remote-qr` 与 `remote/` 的 HTTP 端口、协议相互独立；二者可并存，通过 `RemoteControlGuard` 对 **POST 命令** 互斥，**GET 状态与预览** 不受互斥限制。

---

## 2. 架构与模块

```text
┌─────────────────────────────────────────────────────────────┐
│ 宿主 DeviceHostWindow                                        │
│  setStatusProvider ──► buildDeviceStatusJson()               │
│  commandReceived   ──► dispatchRemoteCommand(cmd)            │
│  取帧回调          ──► previewCache().updateFrame()           │
└───────────────────────────┬─────────────────────────────────┘
                            │
                    ┌───────▼────────┐
                    │   MobileHost   │  集成门面（会话、token、信号）
                    └───────┬────────┘
            ┌───────────────┼───────────────┐
            ▼               ▼               ▼
    MobileWebServer   PreviewFrameCache  TokenManager
    (HTTP 路由)       (JPEG 编码缓存)    (UUID 会话)
            │
            ▼
    assets/mobile.html（qrc 嵌入）
            │
            ▼
    手机浏览器（轮询 /api/status、/api/preview.jpg）
```

### 2.1 模块一览

| 模块 | 源文件 | 实现要点 |
|------|--------|----------|
| 集成门面 | `MobileHost.h/.cpp` | `startSession` / `stopSession`；聚合 WebServer、Token、Preview；发出 `commandReceived` 等信号 |
| HTTP 服务 | `MobileWebServer.h/.cpp` | `QTcpServer` 解析 HTTP/1.1；路由分发；token 校验；命令 POST 经 `MobileCommands` 映射 |
| 命令映射 | `MobileCommands.h` | REST 路径 → 命令名字符串（header-only） |
| 预览缓存 | `PreviewFrameCache.h/.cpp` | 帧序号去重、缩放、`QImage`→JPEG；供 `GET /api/preview.jpg` |
| 会话 token | `TokenManager.h/.cpp` | UUID 生成、UTC 过期、`verifyToken` |
| 配置 | `MobileConfig.h/.cpp` | 自 exe 向上查找 `config/mobile.ini` |
| 网络工具 | `NetworkHelper.h/.cpp` | 枚举本机 IPv4（弹窗 IP 列表） |
| 二维码 | `QrCodeHelper.h/.cpp` + `thirdparty/qrcodegen.*` | URL → `QImage` QR 码 |
| PC 弹窗（可选） | `RemoteControlDialog.h/.cpp` | IP 选择、二维码、刷新/断开会话 |
| H5 客户端 | `assets/mobile.html` | 状态轮询、预览轮询、按钮 POST |
| 资源 | `remote-qr.qrc` | 嵌入 `mobile.html` 至 `:/remote-qr/assets/mobile.html` |

---

## 3. 目录与源文件职责

```text
remote-qr/
  MobileHost.h / MobileHost.cpp           宿主唯一集成类
  MobileWebServer.h / MobileWebServer.cpp HTTP 服务实现
  MobileCommands.h                        POST 路径 → cmd 名
  PreviewFrameCache.h / .cpp              预览 JPEG
  TokenManager.h / .cpp                   会话 token
  MobileConfig.h / .cpp                   mobile.ini
  NetworkHelper.h / .cpp                  本机 IP
  QrCodeHelper.h / .cpp                   QR 图像
  RemoteControlDialog.h / .cpp            可选 PC UI
  assets/mobile.html                      浏览器控制页
  remote-qr.qrc                           Qt 资源
  thirdparty/qrcodegen.hpp / .cpp         Nayuki QR 库
  README.md                               本文档
```

部署时另需宿主可访问的 **`config/mobile.ini`**（见 [§8](#8-配置文件)）。

---

## 4. 运行时序

### 4.1 会话建立

```text
宿主调用 MobileHost::startSession(displayIp)
  → 读取 mobile.ini
  → 从 qrc 加载 mobile.html
  → TokenManager::refreshToken()
  → MobileWebServer::start(port) 绑定 0.0.0.0
  → 发出 sessionStarted(mobileUrl)
```

### 4.2 客户端接入

```text
浏览器 GET /mobile?token=…
  → 返回 mobile.html
  → mobile.html 加载后 GET /api/config、/api/status
  → 首次合法 token 请求 → MobileHost 置 phoneConnected=true → phoneConnected 信号
```

### 4.3 命令执行

```text
浏览器 POST /api/camera/open?token=…
  → verifyToken
  → RemoteControlGuard::tryCommand(QrBrowser)
  → MobileCommands::commandForApiPath → "open_camera"
  → emit commandReceived("open_camera")
  → 宿主 dispatchRemoteCommand 执行业务
  → 响应 JSON = statusWithGuard + ok:true
```

### 4.4 预览推送（拉模式）

```text
宿主取帧循环
  → previewCache().updateFrame(QImage, frameSeq)
浏览器（remotePreviewActive 为 true 时）
  → 串行 GET /api/preview.jpg?token=…&t=…
  → MobileWebServer 调用 previewProvider → getLatestJpeg()
```

### 4.5 断开与超时

| 事件 | 行为 |
|------|------|
| 宿主 `stopSession()` | 关闭 HTTP、失效 token、释放 Guard、`sessionStopped` |
| token 过期 | 定时器触发 `stopSession()` |
| 浏览器关页 | 轮询停止；约 `max(previewPollMs, statusPollMs)×4+1000` ms 后 `phoneDisconnected` |
| 浏览器 `pagehide` | POST `/api/release` 释放命令占用 |
| IP 变更 | 宿主应 `stopSession` 后对新 IP 重新 `startSession` |

---

## 5. HTTP 接口规范

除 `/favicon.ico` 外，**所有请求** Query 须含 `token={会话UUID}`。无效 token 返回 **403** JSON。

### 5.1 路由表

| 方法 | 路径 | 实现位置 | 说明 |
|------|------|----------|------|
| GET | `/mobile` | `MobileWebServer::handleRequest` | 返回 `mobile.html` |
| GET | `/api/config` | 同上 | 客户端轮询间隔，见下表 |
| GET | `/api/status` | 同上 | 设备状态 + Guard 字段 |
| GET | `/api/preview.jpg` | 同上 | JPEG 二进制；无帧时 body 为空 |
| POST | `/api/release` | 同上 | 释放本客户端命令占用 |
| POST | `/api/camera/open` 等 | 同上 + `MobileCommands.h` | 遥控命令，见 [§7](#7-命令映射与宿主处理) |
| GET | `/favicon.ico` | 同上 | 204，不校验 token |

### 5.2 GET `/api/config` 响应字段

由 `MobileHost::buildClientConfigJson()` 生成：

| 字段 | 类型 | 说明 |
|------|------|------|
| `ok` | bool | 固定 `true` |
| `previewPollMs` | int | 预览轮询间隔（ms） |
| `statusPollMs` | int | 状态轮询间隔（ms） |

### 5.3 POST 命令响应

成功：**200**，JSON 含 `ok: true` 及 [§6](#6-状态-json-契约) 状态字段。  
命令通道被其他客户端占用：**409**，`ok: false`，`message` 为中文说明。

---

## 6. 状态 JSON 契约

`GET /api/status` 与 POST 命令成功响应均经 `RemoteControlGuard::statusWithGuard` 合并占用信息后返回。

### 6.1 宿主必须提供的字段（`setStatusProvider`）

以下为 `mobile.html` 已使用的字段；移植时可先实现子集，扩展字段时须同步修改 H5。

| 字段 | 类型 | 说明 |
|------|------|------|
| `ok` | bool | `false` 表示服务不可用（如宿主关闭） |
| `remoteEnabled` | bool | `false` 时 H5 显示链路异常并禁用按钮 |
| `message` | string | 摘要文案，显示于反馈区 |
| `cameraOpen` | bool | 相机/设备是否已打开 |
| `remotePreviewActive` | bool | 远程预览是否开启（H5「开启预览/关闭预览」状态） |
| `liveViewActive` | bool | 可选；本地预览状态（H5 未直接绑定，可并入 message） |
| `acquisitionActive` | bool | 采集/计算是否进行中 |
| `calculateActive` | bool | 可选；与 `acquisitionActive` 等价时 H5 优先读此字段 |
| `stageRunning` | bool | 可选；阶段任务是否运行 |

**宿主关闭远程服务时**应返回示例：

```json
{"ok":false,"remoteEnabled":false,"message":"远程服务已关闭"}
```

**正常运行时**最小示例：

```json
{
  "ok": true,
  "remoteEnabled": true,
  "cameraOpen": false,
  "remotePreviewActive": false,
  "acquisitionActive": false,
  "message": "未连接"
}
```

### 6.2 Guard 附加字段（自动注入，宿主无需写入）

| 字段 | 类型 | 说明 |
|------|------|------|
| `remoteControlOwner` | string | 当前占用方键：`qr_browser` / `miniprogram_http` / `miniprogram_ble` / `none` |
| `remoteControlOwnerLabel` | string | 占用方中文标签 |
| `remoteControlBlocked` | bool | 当前扫码客户端是否被阻塞 |
| `remoteControlMessage` | string | 被阻塞时的提示 |

---

## 7. 命令映射与宿主处理

### 7.1 REST → 命令名

定义于 `MobileCommands.h`，由 `MobileWebServer` 在 POST 时调用：

| POST 路径 | 命令名 `cmd` |
|-----------|----------------|
| `/api/camera/open` | `open_camera` |
| `/api/camera/close` | `close_camera` |
| `/api/preview/open` | `open_preview` |
| `/api/preview/close` | `close_preview` |
| `/api/camera/calculate/start` | `start_calculate` |
| `/api/camera/calculate/stop` | `stop_calculate` |
| `/api/camera/calibrate` | `calibrate` |

**扩展命令**：在 `MobileCommands.h` 增加路径映射 → 宿主 `dispatchRemoteCommand` 增加分支 → `mobile.html` 增加按钮与 `postCmd` 调用。

### 7.2 宿主命令分发示例

```cpp
void DeviceHostWindow::dispatchRemoteCommand(const QString &cmd)
{
    if (cmd == QStringLiteral("open_camera"))
        onOpenDevice();
    else if (cmd == QStringLiteral("close_camera"))
        onCloseDevice();
    else if (cmd == QStringLiteral("open_preview"))
        onStartRemotePreview();
    else if (cmd == QStringLiteral("close_preview"))
        onStopRemotePreview();
    else if (cmd == QStringLiteral("start_calculate"))
        onStartCalculate();
    else if (cmd == QStringLiteral("stop_calculate"))
        onStopCalculate();
    else if (cmd == QStringLiteral("calibrate"))
        onCalibrate();
}
```

命令语义由宿主定义；H5 仅负责触发 POST。上述名称与默认 `mobile.html` 按钮一致，移植时可改为与业务一致的实现，但须保持 **cmd 字符串与映射表一致**。

### 7.3 语义约定（默认 H5 假设）

| 命令 | 典型宿主行为 |
|------|----------------|
| `open_camera` | 打开设备/相机 |
| `close_camera` | 关闭设备；应 `clearPreviewFrame()` |
| `open_preview` | 开启远程预览推送（非必须等同本地 preview） |
| `close_preview` | 停止远程预览；不清除设备连接 |
| `start_calculate` / `stop_calculate` | 启停计算或采集会话 |
| `calibrate` | 触发标定流程 |

---

## 8. 配置文件

### 8.1 路径解析

`MobileConfigHelper::configFilePath()` 自 `QCoreApplication::applicationDirPath()` 向上最多 8 层查找：

```text
{目录}/config/mobile.ini
```

未找到时使用 `MobileConfig` 结构体默认值。

### 8.2 完整示例

```ini
[mobile]
port=8080
token_lifetime_sec=600

[preview]
max_width=480
max_height=360
jpeg_quality=55
poll_ms=120
status_poll_ms=800
```

| 键 | 默认值 | 说明 |
|----|--------|------|
| `port` | 8080 | HTTP 监听端口 |
| `token_lifetime_sec` | 600 | 会话 token 有效期（秒） |
| `max_width` / `max_height` | 480 / 360 | 预览 JPEG 缩放上限 |
| `jpeg_quality` | 55 | JPEG 质量 30–90 |
| `poll_ms` | 120 | 下发给 H5 的预览轮询间隔 |
| `status_poll_ms` | 800 | 下发给 H5 的状态轮询间隔 |

---

## 9. 宿主集成

### 9.1 最小集成步骤

| 步骤 | 操作 |
|------|------|
| 1 | 复制 `remote-qr/`；复制 `remote/RemoteControlGuard.*`（或整个 `remote/`） |
| 2 | 工程加入 [§12](#12-工程与编译配置) 所列源文件与 qrc |
| 3 | 部署 `config/mobile.ini` |
| 4 | 实例化 `MobileHost`，注册 Provider 与信号 |
| 5 | 在取帧路径调用 `previewCache().updateFrame` |
| 6 | 用户触发时调用 `startSession(ip)` |

### 9.2 头文件

```cpp
#include "remote/RemoteControlGuard.h"
#include "remote-qr/MobileHost.h"

class DeviceHostWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit DeviceHostWindow(QWidget *parent = nullptr);

private:
    QJsonObject buildDeviceStatusJson() const;
    void dispatchRemoteCommand(const QString &cmd);
    void onFrameCaptured(const QImage &frame, quint64 frameSeq);

    RemoteControlGuard m_controlGuard;
    MobileHost m_mobileHost;
};
```

### 9.3 构造函数绑定

```cpp
DeviceHostWindow::DeviceHostWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_mobileHost.setControlGuard(&m_controlGuard);
    m_mobileHost.setStatusProvider([this]() {
        return buildDeviceStatusJson();
    });

    connect(&m_mobileHost, &MobileHost::commandReceived,
            this, &DeviceHostWindow::dispatchRemoteCommand);

    connect(&m_mobileHost, &MobileHost::sessionStarted,
            this, [](const QString &url) {
        qDebug() << "QR session:" << url;
    });
}
```

### 9.4 会话与预览

```cpp
// 用户选择本机 IP 后启动（ip 来自 NetworkHelper 或手动输入）
bool ok = m_mobileHost.startSession(QStringLiteral("192.168.1.100"));
if (!ok)
    qWarning() << m_mobileHost.lastError();

// 每帧（设备已打开且需远程预览时）
void DeviceHostWindow::onFrameCaptured(const QImage &frame, quint64 frameSeq)
{
    m_mobileHost.previewCache().updateFrame(frame, frameSeq);
}

// 关闭设备时
void DeviceHostWindow::onCloseDevice()
{
    m_mobileHost.clearPreviewFrame();
}

// 程序退出
void DeviceHostWindow::closeEvent(QCloseEvent *event)
{
    m_mobileHost.stopSession();
    QMainWindow::closeEvent(event);
}
```

### 9.5 状态 JSON 示例实现

```cpp
QJsonObject DeviceHostWindow::buildDeviceStatusJson() const
{
    if (!m_remoteServiceEnabled)
    {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("remoteEnabled"), false},
            {QStringLiteral("message"), QStringLiteral("远程服务已关闭")},
        };
    }

    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("remoteEnabled"), true},
        {QStringLiteral("cameraOpen"), m_device.isOpen()},
        {QStringLiteral("remotePreviewActive"), m_remotePreviewOn},
        {QStringLiteral("acquisitionActive"), m_calculating},
        {QStringLiteral("calculateActive"), m_calculating},
        {QStringLiteral("message"), m_statusMessage},
    };
}
```

### 9.6 MobileHost 信号

| 信号 | 说明 |
|------|------|
| `sessionStarted(url)` | HTTP 会话已建立 |
| `sessionStopped()` | 会话已结束 |
| `commandReceived(cmd)` | 收到合法 POST 命令 |
| `phoneConnected()` | 检测到浏览器首次访问 |
| `phoneDisconnected()` | 浏览器空闲超时 |

---

## 10. H5 页面二次开发

**文件**：`assets/mobile.html`（须通过 `remote-qr.qrc` 编入资源，路径 `:/remote-qr/assets/mobile.html`）。

### 10.1 页面结构

| 区域 | DOM / 逻辑 |
|------|------------|
| 预览 | `#previewImg`、`#previewHint`；`schedulePreview()` 串行加载 JPEG |
| 状态栏 | `#linkStatus`、`#camStatus`、`#calcStatus` |
| 相机 | `#btnOpen` / `#btnClose` → POST `/api/camera/open|close` |
| 远程预览 | `#btnPreviewOn` / `#btnPreviewOff` → POST `/api/preview/open|close` |
| 计算 | `#btnCalcStart` / `#btnCalcStop` |
| 标定 | `#btnCalibrate` |
| 反馈 | `#feedback` |

### 10.2 核心脚本流程

| 函数 | 作用 |
|------|------|
| `apiUrl(path)` | 拼接 `path?token=…` |
| `loadConfig()` | GET `/api/config`，更新轮询间隔 |
| `fetchStatus()` | GET `/api/status`，驱动 UI 与离线检测 |
| `schedulePreview()` | GET `/api/preview.jpg`（仅 `remotePreviewActive` 时） |
| `postCmd(path, label)` | POST 命令；409 时锁定按钮 |
| `releaseCommandLock()` | `pagehide` 时 POST `/api/release` |
| `applyOfflineState()` | `ok==false` 或网络失败时禁用 UI |

### 10.3 修改 H5 的常用场景

| 需求 | 修改位置 |
|------|----------|
| 增删按钮 | HTML `<button>` + `btns` 对象 + `postCmd` |
| 改状态展示 | `applyOnlineStatus`、`formatStateText` |
| 改轮询策略 | `loadConfig` 默认值或 `mobile.ini` |
| 改离线判定 | `fetchStatus` / `fetchTimeoutMs`（默认 5000 ms） |

修改 `mobile.html` 后须**重新编译 qrc** 方可在 PC 端生效。

---

## 11. 可选 PC 端组件

### 11.1 RemoteControlDialog

| 项 | 说明 |
|----|------|
| 类 | `RemoteControlDialog` |
| 依赖 | 已构造的 `MobileHost*` |
| 功能 | IP 下拉（`NetworkHelper`）、QR 显示（`QrCodeHelper`）、URL 复制、刷新 token、断开会话 |
| 行为 | 关闭对话框**不**调用 `stopSession`；仅「断开」或宿主主动 `stopSession` 结束会话 |

```cpp
#include "remote-qr/RemoteControlDialog.h"

RemoteControlDialog *dialog = new RemoteControlDialog(&m_mobileHost, this);
dialog->show();
```

### 11.2 QrCodeHelper

独立工具类，无 UI 依赖：

```cpp
QImage qr = QrCodeHelper::generateQrImage(m_mobileHost.mobileUrl(), 8, 4);
```

---

## 12. 工程与编译配置

### 12.1 编译单元

| 类型 | 文件 |
|------|------|
| ClCompile | `MobileHost.cpp`、`MobileWebServer.cpp`、`PreviewFrameCache.cpp`、`TokenManager.cpp`、`MobileConfig.cpp`、`NetworkHelper.cpp`、`QrCodeHelper.cpp`、`RemoteControlDialog.cpp`（可选）、`thirdparty/qrcodegen.cpp` |
| ClCompile（依赖） | `remote/RemoteControlGuard.cpp` |
| Qt Moc | `MobileHost.h`、`MobileWebServer.h`、`RemoteControlDialog.h`（若使用） |
| Qt Resource | `remote-qr.qrc` |

### 12.2 编译选项

- C++14 及以上
- MSVC 建议添加 **`/utf-8`**（源文件含中文）

### 12.3 包含路径

宿主代码：

```cpp
#include "remote-qr/MobileHost.h"
#include "remote-qr/RemoteControlDialog.h"  // 可选
#include "remote/RemoteControlGuard.h"
```

---

## 13. 移植检查清单

**工程**

- [ ] `remote-qr/` 与 `RemoteControlGuard` 已加入工程
- [ ] `remote-qr.qrc` 已编译，`mobile.html` 可加载
- [ ] `config/mobile.ini` 已部署至 exe 可发现路径
- [ ] Moc / `/utf-8` 已配置

**集成**

- [ ] `setControlGuard`、`setStatusProvider` 已注册
- [ ] `commandReceived` 已连接至命令分发
- [ ] 取帧路径调用 `previewCache().updateFrame`
- [ ] 关设备调用 `clearPreviewFrame`
- [ ] 退出调用 `stopSession`

**联调**

- [ ] PC 浏览器可打开 `mobileUrl()`
- [ ] 手机同网段可访问；防火墙放行 `port`
- [ ] POST 命令后状态 JSON 字段正确更新
- [ ] 关页约 4 s 内 PC 收到 `phoneDisconnected`
- [ ] token 过期后会话自动结束

---

## 14. 故障排查

| 现象 | 可能原因 | 处理 |
|------|----------|------|
| `startSession` 失败 | 端口占用 | 修改 `mobile.ini` 的 `port`；读取 `lastError()` |
| 未找到 mobile.html | qrc 未编入 | 确认 `remote-qr.qrc` 与资源前缀 |
| 403 token 无效 | token 过期或 URL 无 token | 重新 `startSession` 或刷新 QR |
| 手机无法连接 | 网段/防火墙 | 同 WiFi；放行入站 TCP `port` |
| 预览无画面 | 未 `updateFrame` 或 `remotePreviewActive` 为 false | 检查宿主取帧与状态 JSON |
| H5 显示链路异常 | `ok:false` 或 `remoteEnabled:false` | 检查 `buildDeviceStatusJson` 与远程总闸 |
| 命令 409 | 其他客户端占用 Guard | POST `/api/release` 或等待超时 |
| C4819 编译错误 | 编码 | 添加 `/utf-8` |

---

## 附录 A：MobileHost 公共 API 索引

| 方法 | 说明 |
|------|------|
| `startSession(displayIp)` | 启动会话 |
| `stopSession()` | 停止会话 |
| `isSessionActive()` | 会话是否有效 |
| `syncExpiredSession()` | 过期则停止 |
| `mobileUrl()` | 当前访问 URL |
| `lastError()` | 最近错误文案 |
| `previewCache()` | 预览缓存引用 |
| `clearPreviewFrame()` | 清空预览 |
| `isPhoneConnected()` | 浏览器是否在线 |
| `sessionDisplayIp()` | 当前展示 IP |
| `setStatusProvider(fn)` | 注册状态 JSON |
| `setControlGuard(guard)` | 注册互斥 |
| `hadRecentPreviewRequest(now, ttl)` | 预览请求是否在 TTL 内 |

---

**文档维护**：变更 HTTP 路由、状态字段或默认 H5 行为时，须同步更新本文档、`MobileCommands.h` 与 `mobile.html`。
