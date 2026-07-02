#pragma once

#include <QString>

// HTTP POST 路径至遥控命令名映射。
// 供 MobileWebServer 将 /api/camera/* 转换为 commandReceived 信号参数。
namespace MobileCommands
{

// 将 API 路径映射为遥控命令名；未匹配时返回空字符串。
inline QString commandForApiPath(const QString &path)
{
    if (path == QStringLiteral("/api/camera/open"))
        return QStringLiteral("open_camera");
    if (path == QStringLiteral("/api/camera/close"))
        return QStringLiteral("close_camera");
    if (path == QStringLiteral("/api/camera/calculate/start"))
        return QStringLiteral("start_calculate");
    if (path == QStringLiteral("/api/camera/calculate/stop"))
        return QStringLiteral("stop_calculate");
    if (path == QStringLiteral("/api/camera/calibrate"))
        return QStringLiteral("calibrate");
    return QString();
}

} // namespace MobileCommands

