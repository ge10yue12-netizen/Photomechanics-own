#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

#include "BleAdapterChecker.h"
#include "BleGattServerWin.h"

class BleWinRtWorker : public QObject
{
    Q_OBJECT

public:
    explicit BleWinRtWorker(QObject *parent = nullptr);

public slots:
    void initApartment();
    BleAdapterInfo queryAdapter();
    bool startServer(const QString &deviceName);
    void stopServer();
    bool notifyStatus(QByteArray payload);
    bool isServerRunning() const;
    QString serverLastError() const;

signals:
    void commandReceived(QByteArray raw);
    void serverError(QString message);

private:
    BleGattServerWin m_gatt;
};
