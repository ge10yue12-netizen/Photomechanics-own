#include "BleAdapterChecker.h"

#ifdef _WIN32
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Foundation.h>

using namespace winrt;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Enumeration;
#endif

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

BleAdapterInfo queryBleAdapter()
{
    BleAdapterInfo info;
#ifdef _WIN32
    try
    {
        const auto adapter = BluetoothAdapter::GetDefaultAsync().get();
        if (!adapter)
        {
            info.message = QStringLiteral("\u672a\u68c0\u6d4b\u5230\u84dd\u7259\u9002\u914d\u5668\uff08\u8bf7\u786e\u8ba4 USB \u9002\u914d\u5668\u5df2\u63d2\u597d\u4e14\u9a71\u52a8\u5df2\u5b89\u88c5\uff09");
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
            info.message = QStringLiteral("\u9002\u914d\u5668 %1 \u4e0d\u652f\u6301 BLE").arg(info.address);
            return info;
        }
        if (!info.peripheralSupported)
        {
            info.message = QStringLiteral("\u9002\u914d\u5668 %1 \u4e0d\u652f\u6301 BLE \u5916\u8bbe\u6a21\u5f0f").arg(info.address);
            return info;
        }

        info.available = true;
        if (info.friendlyName.isEmpty())
            info.message = QStringLiteral("\u9002\u914d\u5668 %1 \u5c31\u7eea").arg(info.address);
        else
            info.message = QStringLiteral("\u9002\u914d\u5668 %1\uff0c\u624b\u673a\u626b\u63cf\u540d\u79f0\u300c%2\u300d")
                               .arg(info.address, info.friendlyName);
    }
    catch (const winrt::hresult_error &ex)
    {
        info.message = QStringLiteral("BLE \u68c0\u6d4b\u5931\u8d25: %1").arg(QString::fromWCharArray(ex.message().c_str()));
    }
    catch (...)
    {
        info.message = QStringLiteral("BLE \u68c0\u6d4b\u5931\u8d25: \u672a\u77e5\u9519\u8bef");
    }
#else
    info.message = QStringLiteral("BLE \u9065\u63a7\u4ec5\u652f\u6301 Windows");
#endif
    return info;
}
