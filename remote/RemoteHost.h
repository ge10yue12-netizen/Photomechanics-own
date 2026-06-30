#pragma once

#include "RemoteKit.h"

#include <QJsonObject>
#include <QLabel>
#include <QObject>
#include <QStringList>
#include <functional>

/**
 * 遥控集成门面：配置加载、HTTP/BLE 启动、状态行刷新、命令转发。
 * 复制 remote/ + config/netconfig.ini 后，宿主工程只需构造 RemoteHost 并 bootstrap。
 */
class RemoteHost : public QObject
{
    Q_OBJECT

public:
    explicit RemoteHost(QObject *parent = nullptr);

    RemoteKit &kit() { return m_kit; }
    const RemoteKit &kit() const { return m_kit; }

    // 绑定日志区 BLE/HTTP 状态标签；任一可为 nullptr。
    void setStatusLabels(QLabel *bleLabel, QLabel *httpLabel);

    // 注册状态 JSON，须在 bootstrap() 前调用。
    void setStatusProvider(std::function<QJsonObject()> provider);

    // loadConfig + start + 刷新状态行；至少一个通道成功时返回 true。
    bool bootstrap();

    void shutdown();
    void refreshStatusLabels();
    void pushBleStatus();

    // 启动失败的中文说明，供宿主写日志（成功通道不生成项）。
    QStringList startupWarnings() const;

signals:
    void commandReceived(const QString &cmd);
    void notify(const QString &message);

private:
    void onBleServerError(const QString &message);

    RemoteKit m_kit;
    QLabel *m_bleLabel = nullptr;
    QLabel *m_httpLabel = nullptr;
};
