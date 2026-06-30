#pragma once

#include <QSet>
#include <QString>

/**
 * 项目命令表 — 移植时只改本文件。
 * 命令名须与小程序 CMD、onRemoteCommand 分支一致；须包含 status。
 */
namespace RemoteCommands
{

inline const QSet<QString> &knownCommands()
{
    static const QSet<QString> kTable = {
        QStringLiteral("open_camera"),
        QStringLiteral("close_camera"),
        QStringLiteral("start_capture"),
        QStringLiteral("stop_capture"),
        QStringLiteral("save_one"),
        QStringLiteral("start_stage"),
        QStringLiteral("stop_stage"),
        QStringLiteral("status")};
    return kTable;
}

inline bool accepts(const QString &cmd)
{
    return knownCommands().contains(cmd);
}

inline QString label(const QString &cmd)
{
    if (cmd == QStringLiteral("open_camera")) return QStringLiteral("打开相机");
    if (cmd == QStringLiteral("close_camera")) return QStringLiteral("关闭相机");
    if (cmd == QStringLiteral("start_capture")) return QStringLiteral("开始采集");
    if (cmd == QStringLiteral("stop_capture")) return QStringLiteral("停止采集");
    if (cmd == QStringLiteral("save_one")) return QStringLiteral("保存单张");
    if (cmd == QStringLiteral("start_stage")) return QStringLiteral("开始阶段采集");
    if (cmd == QStringLiteral("stop_stage")) return QStringLiteral("停止阶段采集");
    if (cmd == QStringLiteral("status")) return QStringLiteral("查询状态");
    return cmd;
}

} // namespace RemoteCommands

// 全局便捷函数：HTTP/BLE/宿主统一通过此处校验命令与取中文标签。
inline bool isKnownRemoteCommand(const QString &cmd)
{
    return RemoteCommands::accepts(cmd);
}

inline QString remoteCommandLabel(const QString &cmd)
{
    return RemoteCommands::label(cmd);
}
