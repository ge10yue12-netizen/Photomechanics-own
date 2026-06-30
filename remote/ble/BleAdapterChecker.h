#pragma once

#include <QMetaType>
#include <QString>

// Windows 默认蓝牙适配器查询结果，供启动前自检与 UI 展示。
struct BleAdapterInfo
{
    bool available = false;           // 适配器可用且支持 BLE 外设模式
    QString message;                  // 人类可读状态或错误说明
    QString adapterId;                // WinRT DeviceId
    QString address;                  // MAC 地址，冒号分隔大写十六进制
    QString friendlyName;             // 系统设备友好名称（广播名参考）
    bool leSupported = false;         // 是否支持 BLE
    bool peripheralSupported = false; // 是否支持外设（GATT Server）角色
};

Q_DECLARE_METATYPE(BleAdapterInfo)

// 同步查询本机默认蓝牙适配器；非 Windows 平台 available 恒为 false。
BleAdapterInfo queryBleAdapter();