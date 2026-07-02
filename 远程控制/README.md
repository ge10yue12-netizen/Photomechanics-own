# 远程控制（独立小程序）

与 `miniprogram/`（采集控制）**完全独立**，请在微信开发者工具中 **导入本目录** 作为新项目。

## 命令表（与 PC `knownCommands` 对齐）

| 命令 | 按钮 |
|------|------|
| `open_camera` | 打开相机 |
| `close_camera` | 关闭相机 |
| `open_preview` | 开启预览 |
| `close_preview` | 关闭预览 |
| `start_calculate` | 开始计算 |
| `stop_calculate` | 停止计算 |
| `calibrate` | 标定 |
| `status` | 查询状态 |

## 快速开始

1. 微信开发者工具 → 导入项目 → 选择本目录 `远程控制/`
2. 勾选 **不校验合法域名**
3. WiFi 模式填写 PC 日志中的 `IP:端口` 与 `config/netconfig.ini` 中的 token

## 预览

WiFi 通道使用 `downloadFile` + 双缓冲拉帧（`utils/preview-stream.js`）。

### 看不到画面时逐项检查

1. **须用 WiFi 模式**：BLE 不支持 `/api/preview.jpg`，预览区会提示「图像预览仅支持 WiFi 通道」。
2. **开发者工具**：详情 → 本地设置 → 勾选 **不校验合法域名、web-view、TLS 版本以及 HTTPS 证书**。
3. **连接地址**：填 PC 日志里的 `IP:18765`（与 `config/netconfig.ini` 中 `[http] bind/port` 一致），手机与 PC 同一局域网。
4. **口令**：与 `[remote] token` 一致（默认 `1234`）。
5. **打开相机后看状态格**：「相机」须为 **已打开**；「计算」须为 **预览**（对应 PC 的 `liveViewActive`）。若显示「实时预览未就绪」，说明 PC 端 grab 未启动，查 PC 日志是否有「实时预览启动失败」。
6. **PC 遥控 HTTP 已启动**：主界面日志应有 `HTTP 遥控已启动，手机可连接 IP:端口`。
7. **预览首帧**：打开相机后约 1 秒内应出现画面；若长期「暂无图像数据」，在 PC 本地预览是否正常、相机是否物理连接。

## 注意

PC 端 `remote/RemoteCommands.h` 须同步包含上述命令，并在 `onRemoteCommand` 中实现对应分支，否则 HTTP/BLE 会返回未知命令。
