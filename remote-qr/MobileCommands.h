#pragma once

#include <QString>

/**
 * @brief POST /api/camera/* → 宿主 onRemoteCommand 命令名。
 * 须与 remote/RemoteCommands.h 一致；移植时两处同步。
 */
namespace MobileCommands
{

inline QString commandForApiPath(const QString &path)
{
    if (path == QStringLiteral("/api/camera/open"))
        return QStringLiteral("open_camera");
    if (path == QStringLiteral("/api/camera/close"))
        return QStringLiteral("close_camera");
    if (path == QStringLiteral("/api/camera/start"))
        return QStringLiteral("start_capture");
    if (path == QStringLiteral("/api/camera/stop"))
        return QStringLiteral("stop_capture");
    if (path == QStringLiteral("/api/camera/snap"))
        return QStringLiteral("save_one");
    if (path == QStringLiteral("/api/camera/stage/start"))
        return QStringLiteral("start_stage");
    if (path == QStringLiteral("/api/camera/stage/stop"))
        return QStringLiteral("stop_stage");
    return QString();
}

} // namespace MobileCommands
