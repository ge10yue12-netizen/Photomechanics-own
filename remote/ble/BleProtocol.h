#pragma once

#include <QString>

// PhotoMech BLE GATT 协议常量，与微信小程序 protocol.js 保持一致。
namespace BleProtocol
{
// GATT 主服务 UUID。
inline const char *serviceUuid()
{
    return "A1B2C3D4-E5F6-4A5B-8C9D-0E1F2A3B4C5D";
}

// 命令写入特征 UUID（Central 写入，Peripheral 接收）。
inline const char *cmdCharacteristicUuid()
{
    return "A1B2C3D4-E5F6-4A5B-8C9D-0E1F2A3B4C5E";
}

// 状态通知特征 UUID（Peripheral Notify，Central 订阅）。
inline const char *statusCharacteristicUuid()
{
    return "A1B2C3D4-E5F6-4A5B-8C9D-0E1F2A3B4C5F";
}

// BLE 广播显示名称默认值，可在 netconfig.ini 覆盖。
inline QString defaultDeviceName()
{
    return QStringLiteral("PhotoMech");
}
}