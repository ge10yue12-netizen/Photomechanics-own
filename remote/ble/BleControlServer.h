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

// BLE 遥控主线程门面：管理 WinRT 工作线程、解析命令、定时推送状态 Notify。
// 小程序 Central 写入 CMD 特征 → WinRT 回调 → commandReceived 信号 → 主窗口 onRemoteCommand。
class BleControlServer : public QObject
{
    Q_OBJECT

public:
    explicit BleControlServer(QObject *parent = nullptr);
    ~BleControlServer() override;

    // 检测适配器并启动 GATT Server；cfg.token 用于校验 cmd:token。
    bool start(const RemoteConfig &cfg);
    void stop();
    bool isRunning() const;
    QString lastError() const { return m_lastError; }
    BleAdapterInfo adapterInfo() const { return m_adapterInfo; }
    // 供主界面状态标签：已启动 / 未启动 / 失败原因。
    QString statusSummary() const;
    // 注册状态 JSON 提供函数，与 HTTP 遥控共用同一数据源。
    void setStatusProvider(std::function<QJsonObject()> provider);
    void setControlGuard(RemoteControlGuard *guard, RemoteControlSource source);
    // 立即向已连接 Central 推送一次紧凑状态 JSON。
    void pushStatus();

signals:
    // 解析通过的遥控命令（不含 status 查询本身）。
    void commandReceived(const QString &cmd);
    void serverError(const QString &message);

private slots:
    void onStatusTimer();
    // WinRT 线程 QueuedConnection：原始写入字节进入主线程解析。
    void ingestBleCommand(QByteArray raw);
    void reportGattError(QString message);

private:
    void ensureWorkerThread();
    void handleRawCommand(const QByteArray &raw);
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