# PhotoMech 遥控小程序

| 文档 | 读者 |
|------|------|
| **[开发文档](../docs/MINIPROGRAM_REMOTE_DEV.md)** | 协议、目录、扩展命令、联调 |
| **[使用手册](../docs/BLE_REMOTE_GUIDE.md)** | WiFi/BLE 连接步骤、排错 |

## 快速开始（WiFi）

1. PC 日志复制 `HTTP 遥控已启动，手机可连接 IP:端口`
2. 开发者工具打开本目录，勾选 **不校验合法域名**
3. WiFi 模式填地址与口令（默认 `1234`）→ **连接 PC**

## 代码结构

| 文件 | 说明 |
|------|------|
| `pages/control/` | 主页面 |
| `utils/wifi-link.js` / `ble-link.js` | 双模式链路 |
| `utils/remote-buttons.js` | 命令与按钮规则（与 PC 一致） |
| `utils/http.js` / `ble.js` / `protocol.js` | 传输与协议 |

详细说明见 **[开发文档](../docs/MINIPROGRAM_REMOTE_DEV.md)**。
