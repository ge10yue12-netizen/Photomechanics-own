#pragma once

#include <QtGlobal>
#include <QString>

struct RemoteConfig
{
    QString token;
    QString httpBind;
    quint16 httpPort = 0;

    QString httpEndpoint() const;
};

class NetConfigHelper
{
public:
    static QString configFilePath();
    static bool load(RemoteConfig &cfg, QString *error = nullptr);
};
