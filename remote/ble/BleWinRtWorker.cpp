#include "BleWinRtWorker.h"

#include "WinRtBootstrap.h"

BleWinRtWorker::BleWinRtWorker(QObject *parent)
    : QObject(parent)
{
}

void BleWinRtWorker::initApartment()
{
#ifdef _WIN32
    initWinRtMtaOnWorkerThread();
#endif
}

BleAdapterInfo BleWinRtWorker::queryAdapter()
{
    return queryBleAdapter();
}

bool BleWinRtWorker::startServer(const QString &deviceName)
{
    return m_gatt.start(
        deviceName,
        [this](const QByteArray &raw) { emit commandReceived(raw); },
        [this](const QString &msg) { emit serverError(msg); });
}

void BleWinRtWorker::stopServer()
{
    m_gatt.stop();
}

bool BleWinRtWorker::notifyStatus(QByteArray payload)
{
    return m_gatt.notifyStatus(payload);
}

bool BleWinRtWorker::isServerRunning() const
{
    return m_gatt.isRunning();
}

QString BleWinRtWorker::serverLastError() const
{
    return m_gatt.lastError();
}
