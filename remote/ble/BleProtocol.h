#pragma once

#include <QString>

namespace BleProtocol
{
inline const char *serviceUuid()
{
    return "A1B2C3D4-E5F6-4A5B-8C9D-0E1F2A3B4C5D";
}

inline const char *cmdCharacteristicUuid()
{
    return "A1B2C3D4-E5F6-4A5B-8C9D-0E1F2A3B4C5E";
}

inline const char *statusCharacteristicUuid()
{
    return "A1B2C3D4-E5F6-4A5B-8C9D-0E1F2A3B4C5F";
}

inline QString defaultDeviceName()
{
    return QStringLiteral("PhotoMech");
}
}
