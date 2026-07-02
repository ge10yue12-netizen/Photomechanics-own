#pragma once

#include <QJsonObject>
#include <QMutex>
#include <QString>

// 遥控命令占用来源标识。
// 状态查询与预览读取不受互斥限制；POST 命令在同一时刻仅允许一组客户端持有。
enum class RemoteControlSource
{
    None = 0,
    QrBrowser,
    MiniProgramHttp,
    MiniProgramBle
};

// 跨客户端命令通道互斥。
// MiniProgramHttp 与 MiniProgramBle 视为同一客户端组；QrBrowser 为独立组。
// guard 为 nullptr 时不启用互斥。
class RemoteControlGuard
{
public:
    struct Decision
    {
        bool blocked = false;
        QString message;
    };

    // 尝试占用命令通道；已被其他组占用时返回 blocked=true。
    static Decision tryCommand(RemoteControlGuard *guard, RemoteControlSource source);

    // 在 base 状态 JSON 上附加占用方与 blocked 字段。
    static QJsonObject statusWithGuard(RemoteControlGuard *guard,
                                       RemoteControlSource source,
                                       const QJsonObject &base);

    // 释放指定来源占用的命令通道。
    void release(RemoteControlSource source);

    // 释放全部命令占用。
    void releaseAll();

    // 返回来源的协议键名。
    static QString sourceKey(RemoteControlSource source);

    // 返回来源的中文展示标签。
    static QString sourceLabel(RemoteControlSource source);

private:
    // 生成命令被占用时的中文提示。
    QString blockedMessage(RemoteControlSource owner) const;

    mutable QMutex m_mutex;
    RemoteControlSource m_cmdOwner = RemoteControlSource::None;
};
