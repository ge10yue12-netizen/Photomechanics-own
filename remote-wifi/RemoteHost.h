#pragma once

#include "RemoteKit.h"

#include <QJsonObject>
#include <QLabel>
#include <QObject>
#include <QStringList>
#include <functional>

/**
 * WiFi 遥控集成门面：配置加载、HTTP 启动、状态行刷新、命令转发。
 * 复制 remote-wifi/ + config/netconfig.ini 后，宿主工程只需构造 RemoteHost 并 bootstrap。
 */
class RemoteHost : public QObject
{
    Q_OBJECT

public:
    explicit RemoteHost(QObject *parent = nullptr);

    RemoteKit &kit() { return m_kit; }
    const RemoteKit &kit() const { return m_kit; }

    void setStatusLabel(QLabel *httpLabel);
    void setStatusProvider(std::function<QJsonObject()> provider);

    bool bootstrap();
    void shutdown();
    void refreshStatusLabel();

    QStringList startupWarnings() const;

signals:
    void commandReceived(const QString &cmd);
    void notify(const QString &message);

private:
    RemoteKit m_kit;
    QLabel *m_httpLabel = nullptr;
};
