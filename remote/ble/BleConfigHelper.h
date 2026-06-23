#pragma once

#include <QString>

struct BleNetConfig
{
    QString deviceName = QStringLiteral("PhotoMech");
    QString token;
};

class BleConfigHelper
{
public:
    static QString configFilePath();
    static bool ensureDefaultConfigFile();
    static BleNetConfig load();
};