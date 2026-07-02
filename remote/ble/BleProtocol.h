#pragma once

#include <QString>

// PhotoMech BLE GATT 协议常量。
// 定义服务 UUID、命令特征 UUID 与状态 Notify 特征 UUID。
namespace BleProtocol
{
// 返回 GATT 主服务 UUID。
inline const char *serviceUuid()
{
    return "A1B2C3D4-E5F6-4A5B-8C9D-0E1F2A3B4C5D";
}

// 返回命令写入特征 UUID（Central 写入，Peripheral 接收）。
inline const char *cmdCharacteristicUuid()
{
    return "A1B2C3D4-E5F6-4A5B-8C9D-0E1F2A3B4C5E";
}

// 返回状态 Notify 特征 UUID（Peripheral Notify，Central 订阅）。
inline const char *statusCharacteristicUuid()
{
    return "A1B2C3D4-E5F6-4A5B-8C9D-0E1F2A3B4C5F";
}

// 返回 BLE 广播默认设备名；可在 netconfig.ini 中覆盖。
inline QString defaultDeviceName()
{
    return QStringLiteral("PhotoMech");
}
}
