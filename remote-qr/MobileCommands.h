#pragma once

#include <QString>

// POST /api/camera/* 路径与逻辑遥控命令名的映射；须与命令分发器保持一致
namespace MobileCommands
{

// 将 HTTP 路径解析为逻辑命令名；未注册路径返回空字符串
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

/** POST 路径：请求 PC 关闭远程服务（remote-qr 模块自有，与 remote/ 无头文件依赖）。 */
inline QString remoteOffApiPath()
{
    return QStringLiteral("/api/remote/off");
}

} // namespace MobileCommands
