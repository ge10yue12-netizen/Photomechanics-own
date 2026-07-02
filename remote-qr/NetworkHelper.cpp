#include "NetworkHelper.h"

#include <QHostAddress>
#include <QNetworkInterface>

namespace
{

// 判断网卡是否为常见虚拟或回环接口。
bool isVirtualInterface(const QNetworkInterface &iface)
{
    const QString hint = iface.humanReadableName() + QLatin1Char(' ') + iface.name();
    static const QStringList kBlockedHints = {
        QStringLiteral("VMware"),
        QStringLiteral("VirtualBox"),
        QStringLiteral("vEthernet"),
        QStringLiteral("Hyper-V"),
        QStringLiteral("TAP"),
        QStringLiteral("Teredo"),
        QStringLiteral("蓝牙"),
        QStringLiteral("Bluetooth"),
        QStringLiteral("Loopback"),
    };
    for (const QString &blocked : kBlockedHints)
    {
        if (hint.contains(blocked, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

// 判断网卡是否为无线接口。
bool isWifiInterface(const QNetworkInterface &iface)
{
    const QString hint = iface.humanReadableName() + QLatin1Char(' ') + iface.name();
    return hint.contains(QStringLiteral("Wi-Fi"), Qt::CaseInsensitive)
        || hint.contains(QStringLiteral("WLAN"), Qt::CaseInsensitive)
        || hint.contains(QStringLiteral("无线"), Qt::CaseInsensitive);
}

// 去重追加 IP 至列表。
void appendIp(QStringList &list, const QString &ip)
{
    if (!list.contains(ip))
        list.append(ip);
}

} // namespace

// 枚举已启用网卡的 IPv4 地址，无线地址优先。
QStringList NetworkHelper::getLocalIPv4List()
{
    QStringList wifiIps;
    QStringList otherIps;

    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces())
    {
        if (!(iface.flags() & QNetworkInterface::IsUp))
            continue;
        if (iface.flags() & QNetworkInterface::IsLoopBack)
            continue;
        if (isVirtualInterface(iface))
            continue;

        const bool wifi = isWifiInterface(iface);
        for (const QNetworkAddressEntry &entry : iface.addressEntries())
        {
            const QHostAddress addr = entry.ip();
            if (addr.protocol() != QAbstractSocket::IPv4Protocol || addr.isLoopback())
                continue;

            const QString ip = addr.toString();
            if (ip.startsWith(QStringLiteral("169.254.")))
                continue;

            if (wifi)
                appendIp(wifiIps, ip);
            else
                appendIp(otherIps, ip);
        }
    }

    wifiIps.sort();
    otherIps.sort();
    return wifiIps + otherIps;
}

// 返回 getLocalIPv4List 的首项。
QString NetworkHelper::preferredDefaultIp()
{
    const QStringList ips = getLocalIPv4List();
    return ips.isEmpty() ? QString() : ips.first();
}
