#pragma once

#include "BleAdapterChecker.h"
#include "../NetConfigHelper.h"
#include "../RemoteControlGuard.h"

#include <QJsonObject>
#include <QObject>
#include <QThread>
#include <QTimer>
#include <functional>

class BleWinRtWorker;

// BLE 遥控主线程门面。
// 职责：管理 WinRT 工作线程、解析 GATT 写入命令、定时推送状态 Notify。
class BleControlServer : public QObject
{
    Q_OBJECT

public:
    // 构造并启动状态定时推送器。
    explicit BleControlServer(QObject *parent = nullptr);

    // 析构时停止服务并等待 WinRT 线程退出。
    ~BleControlServer() override;

    // 检测适配器并启动 GATT Server；cfg.token 用于校验 cmd:token 格式。
    bool start(const RemoteConfig &cfg);

    // 停止 GATT Server 并推送离线状态 Notify。
    void stop();

    // 返回 GATT Server 是否处于运行状态。
    bool isRunning() const;

    // 返回最近一次启动或运行失败原因。
    QString lastError() const { return m_lastError; }

    // 返回 BLE 适配器检测结果。
    BleAdapterInfo adapterInfo() const { return m_adapterInfo; }

    // 返回 BLE 通道状态摘要文案。
    QString statusSummary() const;

    // 注册设备状态 JSON 提供函数。
    void setStatusProvider(std::function<QJsonObject()> provider);

    // 注入命令通道互斥及本服务来源标识。
    void setControlGuard(RemoteControlGuard *guard, RemoteControlSource source);

    // 向已连接 Central 立即推送一次状态 Notify。
    void pushStatus();

signals:
    // 解析通过的遥控命令时发出（不含 status 查询）。
    void commandReceived(const QString &cmd);

    // GATT 或适配器错误时发出。
    void serverError(const QString &message);

private slots:
    // 定时器触发时推送状态 Notify。
    void onStatusTimer();

    // WinRT 线程 QueuedConnection：接收原始 GATT 写入字节。
    void ingestBleCommand(QByteArray raw);

    // 记录 GATT 错误并转发 serverError 信号。
    void reportGattError(QString message);

private:
    // 懒创建 WinRT 工作线程与 BleWinRtWorker。
    void ensureWorkerThread();

    // 解析 cmd:token 并分发命令或推送状态。
    void handleRawCommand(const QByteArray &raw);

    // 调用 statusProvider 获取当前状态 JSON。
    QJsonObject currentStatus() const;

    QThread m_winRtThread;
    BleWinRtWorker *m_worker = nullptr;
    bool m_running = false;
    BleAdapterInfo m_adapterInfo;
    QString m_token;
    QString m_lastError;
    RemoteControlGuard *m_controlGuard = nullptr;
    RemoteControlSource m_controlSource = RemoteControlSource::MiniProgramBle;
    std::function<QJsonObject()> m_statusProvider;
    QTimer *m_statusTimer = nullptr;
};
