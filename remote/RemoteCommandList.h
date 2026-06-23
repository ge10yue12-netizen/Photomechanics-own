#pragma once

#include <QSet>
#include <QString>

// BLE / 网络遥控共用命令白名单
inline bool isKnownRemoteCommand(const QString &cmd)
{
    static const QSet<QString> commands = {
        QStringLiteral("open_camera"),
        QStringLiteral("close_camera"),
        QStringLiteral("start_capture"),
        QStringLiteral("stop_capture"),
        QStringLiteral("save_one"),
        QStringLiteral("start_stage"),
        QStringLiteral("stop_stage"),
        QStringLiteral("status")};
    return commands.contains(cmd);
}
