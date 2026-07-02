#pragma once

#include <QSet>
#include <QString>

// HTTP/BLE 遥控命令名注册表与中文标签。
// 供 RemoteControlServer、BleControlServer 校验 POST 命令合法性。
namespace RemoteCommands
{

// 返回全部已注册的命令名集合。
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
        QStringLiteral("start_calculate"),
        QStringLiteral("stop_calculate"),
        QStringLiteral("calibrate"),
        QStringLiteral("status")};
    return kTable;
}

// 判断 cmd 是否在已注册命令表中。
inline bool accepts(const QString &cmd)
{
    return knownCommands().contains(cmd);
}

// 返回命令的中文展示标签；未知命令原样返回。
inline QString label(const QString &cmd)
{
    if (cmd == QStringLiteral("open_camera")) return QStringLiteral("打开相机");
    if (cmd == QStringLiteral("close_camera")) return QStringLiteral("关闭相机");
    if (cmd == QStringLiteral("start_capture")) return QStringLiteral("开始采集");
    if (cmd == QStringLiteral("stop_capture")) return QStringLiteral("停止采集");
    if (cmd == QStringLiteral("save_one")) return QStringLiteral("保存单张");
    if (cmd == QStringLiteral("start_stage")) return QStringLiteral("开始阶段采集");
    if (cmd == QStringLiteral("stop_stage")) return QStringLiteral("停止阶段采集");
    if (cmd == QStringLiteral("start_calculate")) return QStringLiteral("开始计算");
    if (cmd == QStringLiteral("stop_calculate")) return QStringLiteral("停止计算");
    if (cmd == QStringLiteral("calibrate")) return QStringLiteral("标定");
    if (cmd == QStringLiteral("status")) return QStringLiteral("查询状态");
    return cmd;
}

} // namespace RemoteCommands

// 校验遥控命令是否在 RemoteCommands 注册表中。
inline bool isKnownRemoteCommand(const QString &cmd)
{
    return RemoteCommands::accepts(cmd);
}

// 返回遥控命令的中文标签。
inline QString remoteCommandLabel(const QString &cmd)
{
    return RemoteCommands::label(cmd);
}
