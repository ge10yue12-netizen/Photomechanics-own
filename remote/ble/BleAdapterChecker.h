#pragma once

#include <QMetaType>
#include <QString>

struct BleAdapterInfo
{
    bool available = false;
    QString message;
    QString adapterId;
    QString address;
    QString friendlyName;
    bool leSupported = false;
    bool peripheralSupported = false;
};

Q_DECLARE_METATYPE(BleAdapterInfo)

BleAdapterInfo queryBleAdapter();
