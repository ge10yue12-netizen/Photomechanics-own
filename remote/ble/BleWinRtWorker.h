#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

#include "BleAdapterChecker.h"
#include "BleGattServerWin.h"

// WinRT 专用工作对象：运行于独立 QThread，承载 GATT Server 与适配器查询。
// 主线程通过 QueuedConnection / BlockingQueuedConnection 跨线程调用槽。
class BleWinRtWorker : public QObject
{
    Q_OBJECT

public:
    explicit BleWinRtWorker(QObject *parent = nullptr);

public slots:
    // 在线程 started 时初始化 WinRT MTA 公寓。
    void initApartment();
    BleAdapterInfo queryAdapter();
    bool startServer(const QString &deviceName);
    void stopServer();
    bool notifyStatus(QByteArray payload);
    bool isServerRunning() const;
    QString serverLastError() const;

signals:
    // 命令写入到达时发出，由 BleControlServer 在主线程解析。
    void commandReceived(QByteArray raw);
    void serverError(QString message);

private:
    BleGattServerWin m_gatt;
};