#pragma once

#include "NetConfigHelper.h"
#include "RemoteControlServer.h"
#include "ble/BleControlServer.h"

#include <QJsonObject>
#include <QObject>
#include <functional>

class RemoteKit : public QObject
{
    Q_OBJECT

public:
    // 构造对象，并将 HTTP/BLE 的 commandReceived 转发到本对象。
    explicit RemoteKit(QObject *parent = nullptr);

    // 定位并读取 config/netconfig.ini，结果缓存在内部 m_cfg。
    bool loadConfig();

    // 使用已加载的 m_cfg 启动 HTTP 与 BLE 服务；两者均失败时返回 false。
    bool start();

    // 停止 HTTP 与 BLE 服务。
    void stop();

    // 返回 loadConfig() 时定位到的 netconfig.ini 绝对路径。
    QString configFilePath() const { return m_configPath; }

    // 返回最近一次 loadConfig() 或 start() 失败的原因。
    QString lastError() const { return m_lastError; }

    // 返回已加载的遥控配置。
    const RemoteConfig &config() const { return m_cfg; }

    // 返回 HTTP 对外地址，格式为 bind:port。
    QString httpEndpoint() const { return m_cfg.httpEndpoint(); }

    // 返回 HTTP 服务实例，供查询监听状态或 lastError。
    RemoteControlServer &http() { return m_http; }
    const RemoteControlServer &http() const { return m_http; }

    // 返回 BLE 服务实例，供查询运行状态或推送状态。
    BleControlServer &ble() { return m_ble; }
    const BleControlServer &ble() const { return m_ble; }

    // 注册状态 JSON 提供函数，HTTP 与 BLE 共用。
    void setStatusProvider(std::function<QJsonObject()> provider);

    // 若 BLE 已运行，立即向已连接客户端推送一次状态。
    void pushBleStatus();

signals:
    // HTTP 与 BLE 收到合法遥控命令时发出，cmd 为命令名。
    void commandReceived(const QString &cmd);

private:
    QString m_configPath;
    QString m_lastError;
    RemoteConfig m_cfg;
    RemoteControlServer m_http;
    BleControlServer m_ble;
};