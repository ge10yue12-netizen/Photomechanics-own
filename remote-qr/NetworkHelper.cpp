#include "NetworkHelper.h"

#include <QHostAddress>
#include <QNetworkInterface>

namespace
{

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

bool isWifiInterface(const QNetworkInterface &iface)
{
    const QString hint = iface.humanReadableName() + QLatin1Char(' ') + iface.name();
    return hint.contains(QStringLiteral("Wi-Fi"), Qt::CaseInsensitive)
        || hint.contains(QStringLiteral("WLAN"), Qt::CaseInsensitive)
        || hint.contains(QStringLiteral("无线"), Qt::CaseInsensitive);
}

void appendIp(QStringList &list, const QString &ip)
{
    if (!list.contains(ip))
        list.append(ip);
}

} // namespace

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

QString NetworkHelper::preferredDefaultIp()
{
    const QStringList ips = getLocalIPv4List();
    return ips.isEmpty() ? QString() : ips.first();
}
