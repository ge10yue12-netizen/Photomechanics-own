#include "BleAdapterChecker.h"

#ifdef _WIN32
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Foundation.h>

using namespace winrt;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Enumeration;
#endif

// 将 WinRT BluetoothAddress 格式化为 AA:BB:CC:DD:EE:FF。
static QString formatBluetoothAddress(quint64 addr)
{
    return QStringLiteral("%1:%2:%3:%4:%5:%6")
        .arg((addr >> 40) & 0xFF, 2, 16, QChar('0'))
        .arg((addr >> 32) & 0xFF, 2, 16, QChar('0'))
        .arg((addr >> 24) & 0xFF, 2, 16, QChar('0'))
        .arg((addr >> 16) & 0xFF, 2, 16, QChar('0'))
        .arg((addr >> 8) & 0xFF, 2, 16, QChar('0'))
        .arg(addr & 0xFF, 2, 16, QChar('0'))
        .toUpper();
}

// 查询默认蓝牙适配器：LE 与外设角色均须支持方可启动 GATT Server。
BleAdapterInfo queryBleAdapter()
{
    BleAdapterInfo info;
#ifdef _WIN32
    try
    {
        const auto adapter = BluetoothAdapter::GetDefaultAsync().get();
        if (!adapter)
        {
            info.message = QStringLiteral("未检测到蓝牙适配器（请确认 USB 适配器已插好且驱动已安装）");
            return info;
        }

        info.adapterId = QString::fromWCharArray(adapter.DeviceId().c_str());
        info.address = formatBluetoothAddress(adapter.BluetoothAddress());
        info.leSupported = adapter.IsLowEnergySupported();
        info.peripheralSupported = adapter.IsPeripheralRoleSupported();

        try
        {
            const auto devInfo = DeviceInformation::CreateFromIdAsync(adapter.DeviceId()).get();
            if (devInfo)
                info.friendlyName = QString::fromWCharArray(devInfo.Name().c_str());
        }
        catch (...)
        {
        }

        if (!info.leSupported)
        {
            info.message = QStringLiteral("适配器 %1 不支持 BLE").arg(info.address);
            return info;
        }
        if (!info.peripheralSupported)
        {
            info.message = QStringLiteral("适配器 %1 不支持 BLE 外设模式").arg(info.address);
            return info;
        }

        info.available = true;
        if (info.friendlyName.isEmpty())
            info.message = QStringLiteral("适配器 %1 就绪").arg(info.address);
        else
            info.message = QStringLiteral("适配器 %1，手机扫描名称「%2」")
                               .arg(info.address, info.friendlyName);
    }
    catch (const winrt::hresult_error &ex)
    {
        info.message = QStringLiteral("BLE 检测失败: %1").arg(QString::fromWCharArray(ex.message().c_str()));
    }
    catch (...)
    {
        info.message = QStringLiteral("BLE 检测失败: 未知错误");
    }
#else
    info.message = QStringLiteral("BLE 遥控仅支持 Windows");
#endif
    return info;
}
