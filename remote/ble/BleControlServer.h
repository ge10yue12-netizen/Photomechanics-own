#pragma once

#include "BleAdapterChecker.h"

#include <QJsonObject>
#include <QObject>
#include <QThread>
#include <QTimer>
#include <functional>

class BleWinRtWorker;

class BleControlServer : public QObject
{
    Q_OBJECT

public:
    explicit BleControlServer(QObject *parent = nullptr);
    ~BleControlServer() override;

    bool start();
    void stop();
    bool isRunning() const;
    QString lastError() const { return m_lastError; }
    BleAdapterInfo adapterInfo() const { return m_adapterInfo; }
    QString statusSummary() const;
    void setStatusProvider(std::function<QJsonObject()> provider);
    void pushStatus();

signals:
    void commandReceived(const QString &cmd);
    void serverError(const QString &message);

private slots:
    void onStatusTimer();
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
    std::function<QJsonObject()> m_statusProvider;
    QTimer *m_statusTimer = nullptr;
};
