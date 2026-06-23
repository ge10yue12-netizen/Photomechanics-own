# 遥控套件（HTTP + BLE）

## 配置

目录：开发时用 **项目根 `config/`**；单独发布程序时放 **exe 旁 `config/`**

| 文件 | 说明 |
|------|------|
| `netconfig.ini` | HTTP + BLE 共用；`[remote] token` 口令；`[http] bind/port`；`[ble] device_name` |

## 小程序 WiFi 连接

1. PC 日志：`HTTP 遥控已启动：http://192.168.x.x:18765`（**勿用** `172.27.x` 等虚拟网卡 IP）
2. 多网卡时看「可用 IP」行，选与手机 **同网段** 的地址
3. 开发者工具勾选 **不校验合法域名**；用 **预览** 扫码（非真机调试局域网模式）
4. 小程序填 `IP:端口`，口令与 `[remote] token` 一致

## 命令

`open_camera` `close_camera` `start_capture` `stop_capture` `save_one` `start_stage` `stop_stage` `status`

`status`：仅刷新状态，PC 端不记警告日志；WiFi 模式走 GET `/api/status`（更快）。

详见 `docs/BLE_REMOTE_GUIDE.md`。

**说明**：改 `config/netconfig.ini` 后重启 PC 软件即可；重新编译**不会**覆盖该配置。x64/Debug/config/ 可删，程序不再优先读它。旧版 `bleconfig.ini` 若仍存在，仅作 `[ble] device_name` 兼容读取，请合并到 `netconfig.ini` 后删除。
