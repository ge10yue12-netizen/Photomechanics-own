#include "BleConfigHelper.h"
#include "BleProtocol.h"
#include "../NetConfigHelper.h"

#include <QDir>
#include <QFile>
#include <QSettings>

namespace
{
QString legacyBleConfigPath()
{
    return QDir(NetConfigHelper::configDirPath()).filePath(QStringLiteral("bleconfig.ini"));
}

QString readBleDeviceName(const QSettings &settings)
{
    QString name = settings.value(QStringLiteral("ble/device_name")).toString().trimmed();
    if (!name.isEmpty())
        return name;

    const QString legacyPath = legacyBleConfigPath();
    if (!QFile::exists(legacyPath))
        return BleProtocol::defaultDeviceName();

    QSettings legacy(legacyPath, QSettings::IniFormat);
    name = legacy.value(QStringLiteral("ble/device_name")).toString().trimmed();
    return name.isEmpty() ? BleProtocol::defaultDeviceName() : name;
}
}

QString BleConfigHelper::configFilePath()
{
    return NetConfigHelper::configFilePath();
}

bool BleConfigHelper::ensureDefaultConfigFile()
{
    return NetConfigHelper::ensureDefaultConfigFile();
}

BleNetConfig BleConfigHelper::load()
{
    BleNetConfig cfg;
    QSettings settings(configFilePath(), QSettings::IniFormat);
    cfg.deviceName = readBleDeviceName(settings);
    cfg.token = NetConfigHelper::loadSharedToken();
    return cfg;
}
