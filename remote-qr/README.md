# remote-qr 扫码遥控套件

C++14 · Qt 5.15 · 局域网浏览器扫码控制。

**移植**：复制 `remote-qr/` + 宿主 `config/mobile.ini` + 下文 §5 约 12 行代码。与 `remote/`（18765 / 小程序）独立并存。

---

## 1. 目录

```text
remote-qr/
  MobileHost.h/.cpp           唯一集成入口
  RemoteControlDialog.h/.cpp  二维码弹窗（可选）
  MobileWebServer.h/.cpp      HTTP
  MobileConfig / TokenManager / NetworkHelper / QrCodeHelper / PreviewFrameCache
  MobileCommands.h            REST → 命令名
  assets/mobile.html + remote-qr.qrc + thirdparty/qrcodegen.*
  README.md
```

配置样本：项目根 `config/mobile.ini`（部署到 exe 向上可找到的 `config/`）。

---

## 2. 行为摘要

| 项 | 说明 |
|----|------|
| 端口 | `mobile.ini` → `port`，绑定 `0.0.0.0` |
| URL | `http://{所选IP}:{port}/mobile?token={uuid}` |
| 会话 | 关弹窗不断开；「断开」或退出程序才 `stopSession` |
| 手机连接 | 首次带 token 的 HTTP 请求 → `phoneConnected` |
| 手机关页 | 轮询停止约 4s → `phoneDisconnected` |
| 预览 | **框始终保留**；关相机清图像（`clearPreviewFrame`），开相机轮询 JPEG |
| token | 过期自动 `stopSession`；「刷新」生成新 token |

---

## 3. HTTP（均需 `?token=`）

| 方法 | 路径 | 命令 |
|------|------|------|
| GET | `/mobile` | 手机页 |
| GET | `/api/config` | 轮询间隔 |
| GET | `/api/status` | 状态 JSON |
| GET | `/api/preview.jpg` | JPEG |
| POST | `/api/camera/open` … `/stage/stop` | 见 `MobileCommands.h` |

---

## 4. mobile.ini

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

---

## 5. 宿主集成（最小）

**工程**：`remote-qr/*.cpp`、`thirdparty/qrcodegen.cpp`、`remote-qr.qrc`；Moc：`MobileHost`、`MobileWebServer`、`RemoteControlDialog`。

```cpp
#include "remote-qr/MobileHost.h"
#include "remote-qr/RemoteControlDialog.h"

MobileHost m_mobileHost;
RemoteControlDialog *m_mobileDialog = nullptr;

// 构造
m_mobileHost.setStatusProvider([this]() { return buildRemoteStatusJson(); });
connect(&m_mobileHost, &MobileHost::commandReceived, this, &MainWindow::onRemoteCommand);
connect(&m_mobileHost, &MobileHost::sessionStarted, this, &MainWindow::refreshMobileStatusLabel);
connect(&m_mobileHost, &MobileHost::sessionStopped, this, &MainWindow::refreshMobileStatusLabel);
connect(&m_mobileHost, &MobileHost::phoneConnected, this, &MainWindow::refreshMobileStatusLabel);
connect(&m_mobileHost, &MobileHost::phoneDisconnected, this, &MainWindow::refreshMobileStatusLabel);

// 每帧（相机已打开；UI 跳帧之前）
m_mobileHost.previewCache().updateFrame(frame, frameSeq);

// 关相机
m_mobileHost.clearPreviewFrame();

// 打开弹窗
if (!m_mobileDialog)
    m_mobileDialog = new RemoteControlDialog(&m_mobileHost, this);
m_mobileDialog->show();

// 退出
m_mobileHost.stopSession();
```

**`buildRemoteStatusJson`** 须含：`cameraOpen`、`liveViewActive`、`acquisitionActive`、`stageRunning`、`message`。

**`onRemoteCommand`** 处理 `MobileCommands.h` 全部命令，逻辑同主界面按钮。

---

## 6. 排错

| 现象 | 处理 |
|------|------|
| 等待手机 | 同网段 / 防火墙 8080 / PC 浏览器试 URL / 点刷新重扫 |
| 一直已连接 | 手机关页后约 4s 应收 `phoneDisconnected` |
| 启动失败 | 看 `lastError()`（端口占用、qrc 未编入） |

---

## 7. 移植清单

- [ ] 复制 `remote-qr/`
- [ ] 部署 `config/mobile.ini`
- [ ] vcxproj + qrc + Moc
- [ ] §5 代码 + `onRemoteCommand` + 取帧 `updateFrame`
