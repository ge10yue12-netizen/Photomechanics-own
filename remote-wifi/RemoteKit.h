#pragma once

#include "NetConfigHelper.h"
#include "RemoteControlServer.h"

#include <QJsonObject>
#include <QObject>
#include <functional>

/** WiFi-only 遥控套件：HTTP 服务 + 配置加载，无 BLE 依赖。 */
class RemoteKit : public QObject
{
    Q_OBJECT

public:
    explicit RemoteKit(QObject *parent = nullptr);

    bool loadConfig();
    bool start();
    void stop();

    QString configFilePath() const { return m_configPath; }
    QString lastError() const { return m_lastError; }
    const RemoteConfig &config() const { return m_cfg; }
    QString httpEndpoint() const { return m_cfg.httpEndpoint(); }

    RemoteControlServer &http() { return m_http; }
    const RemoteControlServer &http() const { return m_http; }

    void setStatusProvider(std::function<QJsonObject()> provider);

signals:
    void commandReceived(const QString &cmd);

private:
    QString m_configPath;
    QString m_lastError;
    RemoteConfig m_cfg;
    RemoteControlServer m_http;
};
