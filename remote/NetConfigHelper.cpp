#include "NetConfigHelper.h"

#include <QAbstractSocket>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkInterface>
#include <QSettings>
#include <QTcpServer>

namespace
{
QString settingsPath()
{
    return NetConfigHelper::configFilePath();
}

quint16 readPort(const QSettings &settings, const QString &group, quint16 fallback)
{
    const int port = settings.value(group + QStringLiteral("/port"), fallback).toInt();
    if (port < 1 || port > 65535)
        return fallback;
    return static_cast<quint16>(port);
}

QString readBind(const QSettings &settings, const QString &group, const QString &fallback)
{
    const QString bind = settings.value(group + QStringLiteral("/bind"), fallback).toString().trimmed();
    return bind.isEmpty() ? fallback : bind;
}

QString readRemoteToken(const QSettings &settings)
{
    const QString token = settings.value(QStringLiteral("remote/token")).toString().trimmed();
    return token.isEmpty() ? QStringLiteral("1234") : token;
}

void writeDefaultNetConfig(const QString &path)
{
    QDir().mkpath(QFileInfo(path).absolutePath());

    QSettings settings(path, QSettings::IniFormat);
    settings.setValue(QStringLiteral("remote/token"), QStringLiteral("1234"));

    settings.beginGroup(QStringLiteral("http"));
    settings.setValue(QStringLiteral("bind"), QStringLiteral("0.0.0.0"));
    settings.setValue(QStringLiteral("port"), 18765);
    settings.endGroup();

    settings.beginGroup(QStringLiteral("ble"));
    settings.setValue(QStringLiteral("device_name"), QStringLiteral("PhotoMech"));
    settings.endGroup();

    settings.sync();
}

bool configDirUsable(const QString &dirPath)
{
    if (!QDir(dirPath).exists())
        return false;
    return QFile::exists(QDir(dirPath).filePath(QStringLiteral("netconfig.ini")));
}

bool isSkippedInterface(const QNetworkInterface &iface)
{
    if (!(iface.flags() & QNetworkInterface::IsUp) || !(iface.flags() & QNetworkInterface::IsRunning))
        return true;
    if (iface.flags() & QNetworkInterface::IsLoopBack)
        return true;
    const QString hint = (iface.humanReadableName() + QLatin1Char(' ') + iface.name()).toLower();
    static const QStringList skipHints = {
        QStringLiteral("vethernet"), QStringLiteral("hyper-v"), QStringLiteral("vmware"),
        QStringLiteral("virtualbox"), QStringLiteral("wsl"), QStringLiteral("docker"),
        QStringLiteral("default switch"), QStringLiteral("tap-windows")};
    for (const QString &s : skipHints)
    {
        if (hint.contains(s))
            return true;
    }
    return false;
}
QString findProjectConfigDir(const QDir &exeDir)
{
    QDir d(exeDir);
    if (d.cdUp() && d.cdUp())
    {
        const QString projectConfig = d.filePath(QStringLiteral("config"));
        if (QDir(projectConfig).exists())
            return projectConfig;
    }
    return QString();
}
}


QString NetConfigHelper::configDirPath()
{
    const QDir exeDir(QCoreApplication::applicationDirPath());
    const QString besideExe = exeDir.filePath(QStringLiteral("config"));
    const QString projectConfig = findProjectConfigDir(exeDir);

    if (!projectConfig.isEmpty())
        return projectConfig;

    if (configDirUsable(besideExe))
        return besideExe;
    return besideExe;
}

QString NetConfigHelper::configFilePath()
{
    return QDir(configDirPath()).filePath(QStringLiteral("netconfig.ini"));
}

QString NetConfigHelper::loadSharedToken()
{
    const QString path = configFilePath();
    if (!QFile::exists(path))
        return QStringLiteral("1234");
    QSettings settings(path, QSettings::IniFormat);
    return readRemoteToken(settings);
}

bool NetConfigHelper::ensureDefaultConfigFile()
{
    const QString path = configFilePath();
    if (QFile::exists(path))
        return true;
    writeDefaultNetConfig(path);
    return QFile::exists(path);
}

QHostAddress NetConfigHelper::parseBindAddress(const QString &bind)
{
    const QString trimmed = bind.trimmed();
    if (trimmed.isEmpty() || trimmed == QStringLiteral("0.0.0.0"))
        return QHostAddress::Any;
    const QHostAddress address(trimmed);
    return address.isNull() ? QHostAddress::Any : address;
}

HttpNetConfig NetConfigHelper::loadHttpConfig()
{
    HttpNetConfig cfg;
    QSettings settings(settingsPath(), QSettings::IniFormat);
    cfg.port = readPort(settings, QStringLiteral("http"), cfg.port);
    cfg.token = readRemoteToken(settings);
    cfg.bindAddress = parseBindAddress(readBind(settings, QStringLiteral("http"), QStringLiteral("0.0.0.0")));
    return cfg;
}

bool NetConfigHelper::isLocalOnlyBind(const QHostAddress &bindAddress)
{
    return bindAddress == QHostAddress::LocalHost || bindAddress == QHostAddress(QStringLiteral("127.0.0.1"));
}

QString NetConfigHelper::bindAddressText(const QHostAddress &bindAddress)
{
    if (bindAddress == QHostAddress::Any)
        return QStringLiteral("0.0.0.0");
    return bindAddress.toString();
}

QStringList NetConfigHelper::listLanIpv4()
{
    QStringList result;
    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : interfaces)
    {
        if (isSkippedInterface(iface))
            continue;
        for (const QNetworkAddressEntry &entry : iface.addressEntries())
        {
            const QHostAddress ip = entry.ip();
            if (ip.protocol() != QAbstractSocket::IPv4Protocol || ip.isLoopback())
                continue;
            const QString s = ip.toString();
            if (s.startsWith(QStringLiteral("169.254.")))
                continue;
            if (!result.contains(s))
                result.append(s);
        }
    }
    return result;
}

QString NetConfigHelper::preferredLanIpv4()
{
    QString best10;
    QString fallback;
    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : interfaces)
    {
        if (isSkippedInterface(iface))
            continue;
        for (const QNetworkAddressEntry &entry : iface.addressEntries())
        {
            const QHostAddress ip = entry.ip();
            if (ip.protocol() != QAbstractSocket::IPv4Protocol || ip.isLoopback())
                continue;
            const QString s = ip.toString();
            if (s.startsWith(QStringLiteral("169.254.")))
                continue;
            if (s.startsWith(QStringLiteral("192.168.")))
                return s;
            if (s.startsWith(QStringLiteral("10.")) && best10.isEmpty())
                best10 = s;
            if (fallback.isEmpty())
                fallback = s;
        }
    }
    if (!best10.isEmpty())
        return best10;
    return fallback.isEmpty() ? QStringLiteral("127.0.0.1") : fallback;
}

QString NetConfigHelper::httpEndpointHint(quint16 port)
{
    return QStringLiteral("%1:%2").arg(preferredLanIpv4()).arg(port);
}

bool NetConfigHelper::tryListen(QTcpServer &server,
                                const QHostAddress &bindAddress,
                                quint16 port,
                                QString *lastError)
{
    auto recordError = [&](const QString &msg) {
        if (lastError)
            *lastError = msg;
    };

    if (server.isListening())
        server.close();

    if (server.listen(bindAddress, port))
        return true;

    const QString firstErr = server.errorString();
    if (bindAddress != QHostAddress::Any && bindAddress != QHostAddress::AnyIPv4)
    {
        server.close();
        if (server.listen(QHostAddress::Any, port))
        {
            recordError(QStringLiteral("绑定 %1 失败（%2），已改用 0.0.0.0")
                            .arg(bindAddressText(bindAddress), firstErr));
            return true;
        }
    }

    recordError(firstErr);
    return false;
}