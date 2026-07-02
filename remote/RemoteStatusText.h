#pragma once

#include <QString>

// PC 端 HTTP/BLE 状态行文案生成（header-only）。
namespace RemoteStatusText
{

// 通道已启动时的状态文案。
inline QString started()
{
    return QStringLiteral("已启动");
}

// 通道未启动且无错误时的状态文案。
inline QString stopped()
{
    return QStringLiteral("未启动");
}

// 将 Qt 监听错误翻译为中文配置提示。
inline QString humanizeHttpListenError(const QString &raw)
{
    if (raw.isEmpty())
        return QString();
    if (raw.contains(QStringLiteral("not available"), Qt::CaseInsensitive))
    {
        return QStringLiteral(
            "监听地址不可用：须将 config/netconfig.ini 中 http/bind"
            "设为本机局域网 IPv4（与客户端同网段）");
    }
    return raw;
}

// 根据 HTTP 监听状态与 lastError 生成摘要文案。
inline QString httpSummary(bool listening, const QString &lastError)
{
    if (listening)
        return started();
    const QString err = humanizeHttpListenError(lastError);
    return err.isEmpty() ? stopped() : err;
}

// 根据 BLE 运行状态与 lastError 生成摘要文案。
inline QString bleSummary(bool running, const QString &lastError)
{
    if (running)
        return started();
    return lastError.isEmpty() ? stopped() : lastError;
}

} // namespace RemoteStatusText
