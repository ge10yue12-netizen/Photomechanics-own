#pragma once

#include <QJsonObject>
#include <QMutex>
#include <QString>

/**
 * 遥控命令占用源；状态/预览各通道可同时访问，仅 POST 命令互斥。
 * MiniProgramHttp 与 MiniProgramBle 视为同一客户端组（小程序 WiFi/BLE 切换不互斥）。
 */
enum class RemoteControlSource
{
    None = 0,
    QrBrowser,
    MiniProgramHttp,
    MiniProgramBle
};

/** 跨通道命令锁：复制 remote/ 后与 HTTP/BLE/remote-qr 共用同一 guard 实例。guard 为 nullptr 时不启用。 */
class RemoteControlGuard
{
public:
    struct Decision
    {
        bool blocked = false;
        QString message;
    };

    static Decision tryCommand(RemoteControlGuard *guard, RemoteControlSource source);
    static QJsonObject statusWithGuard(RemoteControlGuard *guard,
                                       RemoteControlSource source,
                                       const QJsonObject &base);

    void release(RemoteControlSource source);
    void releaseAll();

    static QString sourceKey(RemoteControlSource source);
    static QString sourceLabel(RemoteControlSource source);

private:
    QString blockedMessage(RemoteControlSource owner) const;

    mutable QMutex m_mutex;
    RemoteControlSource m_cmdOwner = RemoteControlSource::None;
};
