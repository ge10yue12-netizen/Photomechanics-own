#pragma once

#include "BleControlServer.h"
#include "BleProtocol.h"
#include "../RemoteCommandList.h"
#include "../RemoteControlServer.h"
#include "../NetConfigHelper.h"

// 可移植遥控套件（复制整个 remote/ + config/ + miniprogram/utils/）
//
// PC 集成：
// 1. 复制 remote/，加入 vcxproj；BLE 单元需 C++17、/await、windowsapp.lib
// 2. 开发时维护项目根 config/；发布时将 config/ 与 exe 同目录打包
// 3. RemoteControlServer + BleControlServer 均连接 commandReceived → 你的命令处理
// 4. setStatusProvider 返回 JSON 状态
// 5. 启动前 NetConfigHelper::ensureDefaultConfigFile()
//
// 配置（同一目录 config/）：
//   netconfig.ini  [remote] token；[http] bind/port；[ble] device_name
//
// 手机端：miniprogram/utils/protocol.js + http.js（WiFi 推荐）+ ble.js（Windows BLE 可选）
// 命令白名单：RemoteCommandList.h