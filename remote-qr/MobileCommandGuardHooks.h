#pragma once

#include <QJsonObject>
#include <QString>
#include <functional>

/** POST 命令互斥钩子；由宿主注入，remote-qr 不依赖 remote/ 头文件。guard 未注入时不启用互斥。 */
struct MobileCommandGuardHooks
{
    struct Decision
    {
        bool blocked = false;
        QString message;
    };

    std::function<Decision()> tryCommand;
    std::function<void()> release;
    std::function<QJsonObject(const QJsonObject &base)> augmentStatus;

    bool isActive() const
    {
        return tryCommand && release && augmentStatus;
    }
};
