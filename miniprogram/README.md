# PhotoMech 遥控小程序

| 文档 | 读者 |
|------|------|
| **[远程控制开发说明](../docs/REMOTE_CONTROL_GUIDE.md)** | 统一文档；小程序见 §4 |

## 快速开始（WiFi）

1. PC 日志复制 `HTTP 遥控已启动，手机可连接 IP:端口`
2. 开发者工具打开本目录，勾选 **不校验合法域名**
3. WiFi 模式填地址与口令（默认 `1234`）→ **连接 PC**

## 代码结构

| 文件 | 说明 |
|------|------|
| `pages/control/` | 主页面 |
| `utils/wifi-link.js` / `ble-link.js` | 双模式链路 |
| `utils/remote-buttons.js` | 命令、按钮规则、状态摘要（与 PC / 扫码页一致） |
| `utils/http.js` / `ble.js` / `protocol.js` | 传输与协议；WiFi 预览 `/api/preview.jpg` |

详细说明见 **[远程控制开发说明](../docs/REMOTE_CONTROL_GUIDE.md)** §4。
