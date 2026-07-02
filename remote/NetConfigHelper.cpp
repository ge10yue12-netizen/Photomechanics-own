#include "NetConfigHelper.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QSettings>

namespace
{

const QString kConfigRel = QStringLiteral("config/netconfig.ini");

// 写入 error 并返回 false。
bool fail(QString *error, const QString &msg)
{
    if (error)
        *error = msg;
    return false;
}

} // namespace

// 拼接 httpBind 与 httpPort 为对外 endpoint 字符串。
QString RemoteConfig::httpEndpoint() const
{
    return httpBind.trimmed() + QLatin1Char(':') + QString::number(httpPort);
}

// 自 exe 目录向上查找 netconfig.ini。
QString NetConfigHelper::configFilePath()
{
    QDir dir(QCoreApplication::applicationDirPath());
    for (int depth = 0; depth < 8; ++depth)
    {
        const QString path = dir.absoluteFilePath(kConfigRel);
        if (QFile::exists(path))
            return path;
        if (!dir.cdUp())
            break;
    }
    return QString();
}

// 解析 netconfig.ini 各段并校验必填项与 bind 地址格式。
bool NetConfigHelper::load(RemoteConfig &cfg, QString *error)
{
    const QString path = configFilePath();
    if (path.isEmpty())
        return fail(error, QStringLiteral("配置文件缺失：config/netconfig.ini"));

    QSettings settings(path, QSettings::IniFormat);
    cfg.token = settings.value(QStringLiteral("remote/token")).toString().trimmed();
    cfg.httpBind = settings.value(QStringLiteral("http/bind")).toString().trimmed();
    cfg.bleDeviceName = settings.value(QStringLiteral("ble/device_name")).toString().trimmed();

    const int port = settings.value(QStringLiteral("http/port")).toInt();
    if (port < 1 || port > 65535)
        return fail(error, QStringLiteral("http/port 无效（1-65535）"));
    cfg.httpPort = static_cast<quint16>(port);

    if (cfg.token.isEmpty())
        return fail(error, QStringLiteral("remote/token 未配置"));
    if (cfg.httpBind.isEmpty())
        return fail(error, QStringLiteral("http/bind 未配置"));

    const QHostAddress bindAddr(cfg.httpBind);
    if (bindAddr.isNull())
        return fail(error, QStringLiteral("http/bind 不是有效地址：") + cfg.httpBind);

    if (cfg.bleDeviceName.isEmpty())
        return fail(error, QStringLiteral("ble/device_name 未配置"));

    return true;
}
