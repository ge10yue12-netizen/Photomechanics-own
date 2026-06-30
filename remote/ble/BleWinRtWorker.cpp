#include "BleWinRtWorker.h"

#include "WinRtBootstrap.h"

BleWinRtWorker::BleWinRtWorker(QObject *parent)
    : QObject(parent)
{
}

// 线程入口：初始化 WinRT 多线程公寓，供后续 GATT API 调用。
void BleWinRtWorker::initApartment()
{
#ifdef _WIN32
    initWinRtMtaOnWorkerThread();
#endif
}

// 在同一线程同步查询蓝牙适配器能力。
BleAdapterInfo BleWinRtWorker::queryAdapter()
{
    return queryBleAdapter();
}

// 启动 GATT Server；命令与错误通过信号转发至主线程。
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

// 推送状态 Notify；payload 须已在主线程序列化为紧凑 JSON。
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