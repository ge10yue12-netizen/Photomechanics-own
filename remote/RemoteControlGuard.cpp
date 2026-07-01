#include "RemoteControlGuard.h"

RemoteControlGuard::Decision RemoteControlGuard::tryCommand(RemoteControlGuard *guard,
                                                            RemoteControlSource source)
{
    if (!guard)
        return {};
    QMutexLocker lock(&guard->m_mutex);
    if (guard->m_cmdOwner != RemoteControlSource::None && guard->m_cmdOwner != source)
    {
        Decision d;
        d.blocked = true;
        d.message = guard->blockedMessage(guard->m_cmdOwner);
        return d;
    }
    guard->m_cmdOwner = source;
    return {};
}

QJsonObject RemoteControlGuard::statusWithGuard(RemoteControlGuard *guard,
                                                  RemoteControlSource source,
                                                  const QJsonObject &base)
{
    if (!guard)
        return base;
    QMutexLocker lock(&guard->m_mutex);
    QJsonObject obj = base;
    const RemoteControlSource owner = guard->m_cmdOwner;
    const bool blocked = owner != RemoteControlSource::None && owner != source;
    obj.insert(QStringLiteral("remoteControlOwner"), sourceKey(owner));
    obj.insert(QStringLiteral("remoteControlOwnerLabel"), sourceLabel(owner));
    obj.insert(QStringLiteral("remoteControlBlocked"), blocked);
    if (blocked)
        obj.insert(QStringLiteral("remoteControlMessage"), guard->blockedMessage(owner));
    return obj;
}

void RemoteControlGuard::release(RemoteControlSource source)
{
    QMutexLocker lock(&m_mutex);
    if (m_cmdOwner == source)
        m_cmdOwner = RemoteControlSource::None;
}

void RemoteControlGuard::releaseAll()
{
    QMutexLocker lock(&m_mutex);
    m_cmdOwner = RemoteControlSource::None;
}

QString RemoteControlGuard::sourceKey(RemoteControlSource source)
{
    switch (source)
    {
    case RemoteControlSource::QrBrowser:
        return QStringLiteral("qr_browser");
    case RemoteControlSource::MiniProgramHttp:
        return QStringLiteral("miniprogram_http");
    case RemoteControlSource::MiniProgramBle:
        return QStringLiteral("miniprogram_ble");
    default:
        return QStringLiteral("none");
    }
}

QString RemoteControlGuard::sourceLabel(RemoteControlSource source)
{
    switch (source)
    {
    case RemoteControlSource::QrBrowser:
        return QStringLiteral("Web 客户端");
    case RemoteControlSource::MiniProgramHttp:
        return QStringLiteral("HTTP 客户端");
    case RemoteControlSource::MiniProgramBle:
        return QStringLiteral("BLE 客户端");
    default:
        return QStringLiteral("无占用");
    }
}

QString RemoteControlGuard::blockedMessage(RemoteControlSource owner) const
{
    return QStringLiteral("命令通道被 %1 占用，须先释放连接").arg(sourceLabel(owner));
}
