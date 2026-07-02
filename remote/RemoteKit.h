#pragma once

#include "NetConfigHelper.h"
#include "RemoteControlGuard.h"
#include "RemoteControlServer.h"
#include "ble/BleControlServer.h"

#include <QJsonObject>
#include <QObject>
#include <functional>

// HTTP/BLE 双通道遥控内核。
// 职责：读取 netconfig.ini、并行启动/停止两路服务、统一转发 commandReceived。
class RemoteKit : public QObject
{
    Q_OBJECT

public:
    // 构造并连接 HTTP/BLE commandReceived 至本对象。
    explicit RemoteKit(QObject *parent = nullptr);

    // 定位并读取 config/netconfig.ini，结果写入 m_cfg。
    bool loadConfig();

    // 按 m_cfg 启动 HTTP 与 BLE；两路均失败时返回 false。
    bool start();

    // 停止 HTTP 与 BLE，并释放本组命令占用。
    void stop();

    // 返回 loadConfig 定位到的 netconfig.ini 绝对路径。
    QString configFilePath() const { return m_configPath; }

    // 返回最近一次 loadConfig 或 start 失败原因。
    QString lastError() const { return m_lastError; }

    // 返回已加载的遥控配置结构体。
    const RemoteConfig &config() const { return m_cfg; }

    // 返回 HTTP 对外地址，格式为 bind:port。
    QString httpEndpoint() const { return m_cfg.httpEndpoint(); }

    // 返回 HTTP 服务实例。
    RemoteControlServer &http() { return m_http; }
    const RemoteControlServer &http() const { return m_http; }

    // 返回 BLE 服务实例。
    BleControlServer &ble() { return m_ble; }
    const BleControlServer &ble() const { return m_ble; }

    // 注册状态 JSON 提供函数；HTTP 与 BLE 共用同一数据源。
    void setStatusProvider(std::function<QJsonObject()> provider);

    // 注册 HTTP GET /api/preview.jpg 的 JPEG 提供函数。
    void setPreviewProvider(std::function<QByteArray()> provider);

    // 注入命令通道互斥；HTTP/BLE 分别绑定对应 RemoteControlSource。
    void setControlGuard(RemoteControlGuard *guard);

    // BLE 已运行时向已连接 Central 推送一次状态。
    void pushBleStatus();

signals:
    // HTTP 或 BLE 收到合法遥控命令时发出。
    void commandReceived(const QString &cmd);

private:
    QString m_configPath;
    QString m_lastError;
    RemoteConfig m_cfg;
    RemoteControlGuard *m_controlGuard = nullptr;
    RemoteControlServer m_http;
    BleControlServer m_ble;
};
