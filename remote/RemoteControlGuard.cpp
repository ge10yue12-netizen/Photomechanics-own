#include "RemoteControlGuard.h"

namespace
{

// 判断来源是否属于 WiFi/BLE 客户端组。
bool isMiniProgramSource(RemoteControlSource source)
{
    return source == RemoteControlSource::MiniProgramHttp
           || source == RemoteControlSource::MiniProgramBle;
}

// 判断两来源是否属于同一互斥组。
bool sameClientGroup(RemoteControlSource a, RemoteControlSource b)
{
    if (a == b)
        return true;
    return isMiniProgramSource(a) && isMiniProgramSource(b);
}

} // namespace

// 尝试占用命令通道；异组已占用时拒绝。
RemoteControlGuard::Decision RemoteControlGuard::tryCommand(RemoteControlGuard *guard,
                                                            RemoteControlSource source)
{
    if (!guard)
        return {};
    QMutexLocker lock(&guard->m_mutex);
    if (guard->m_cmdOwner != RemoteControlSource::None
        && !sameClientGroup(guard->m_cmdOwner, source))
    {
        Decision d;
        d.blocked = true;
        d.message = guard->blockedMessage(guard->m_cmdOwner);
        return d;
    }
    guard->m_cmdOwner = source;
    return {};
}

// 合并命令占用信息至状态 JSON。
QJsonObject RemoteControlGuard::statusWithGuard(RemoteControlGuard *guard,
                                                  RemoteControlSource source,
                                                  const QJsonObject &base)
{
    if (!guard)
        return base;
    QMutexLocker lock(&guard->m_mutex);
    QJsonObject obj = base;
    const RemoteControlSource owner = guard->m_cmdOwner;
    const bool blocked = owner != RemoteControlSource::None && !sameClientGroup(owner, source);
    obj.insert(QStringLiteral("remoteControlOwner"), sourceKey(owner));
    obj.insert(QStringLiteral("remoteControlOwnerLabel"), sourceLabel(owner));
    obj.insert(QStringLiteral("remoteControlBlocked"), blocked);
    if (blocked)
        obj.insert(QStringLiteral("remoteControlMessage"), guard->blockedMessage(owner));
    return obj;
}

// 释放与 source 同组的命令占用。
void RemoteControlGuard::release(RemoteControlSource source)
{
    QMutexLocker lock(&m_mutex);
    if (m_cmdOwner == source || sameClientGroup(m_cmdOwner, source))
        m_cmdOwner = RemoteControlSource::None;
}

// 清空命令占用方。
void RemoteControlGuard::releaseAll()
{
    QMutexLocker lock(&m_mutex);
    m_cmdOwner = RemoteControlSource::None;
}

// 将枚举映射为 JSON 协议键。
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

// 将枚举映射为 UI 展示标签。
QString RemoteControlGuard::sourceLabel(RemoteControlSource source)
{
    switch (source)
    {
    case RemoteControlSource::QrBrowser:
        return QStringLiteral("Web 客户端");
    case RemoteControlSource::MiniProgramHttp:
        return QStringLiteral("WiFi 客户端");
    case RemoteControlSource::MiniProgramBle:
        return QStringLiteral("BLE 客户端");
    default:
        return QStringLiteral("无占用");
    }
}

// 构造占用冲突提示文案。
QString RemoteControlGuard::blockedMessage(RemoteControlSource owner) const
{
    return QStringLiteral("命令通道被 %1 占用，须先释放连接").arg(sourceLabel(owner));
}
