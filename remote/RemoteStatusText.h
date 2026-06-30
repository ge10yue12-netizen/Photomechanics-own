#pragma once

#include <QString>

/** PC 端状态行文案：已启动 / 未启动 / 失败原因（header-only，无额外链接依赖）。 */
namespace RemoteStatusText
{

inline QString started()
{
    return QStringLiteral("已启动");
}

inline QString stopped()
{
    return QStringLiteral("未启动");
}

inline QString humanizeHttpListenError(const QString &raw)
{
    if (raw.isEmpty())
        return QString();
    if (raw.contains(QStringLiteral("not available"), Qt::CaseInsensitive))
    {
        return QStringLiteral(
            "监听地址不可用，请将 config/netconfig.ini 中 http/bind"
            "改为本机 WiFi 网卡 IPv4（与手机同网段）");
    }
    return raw;
}

inline QString httpSummary(bool listening, const QString &lastError)
{
    if (listening)
        return started();
    const QString err = humanizeHttpListenError(lastError);
    return err.isEmpty() ? stopped() : err;
}

inline QString bleSummary(bool running, const QString &lastError)
{
    if (running)
        return started();
    return lastError.isEmpty() ? stopped() : lastError;
}

} // namespace RemoteStatusText
