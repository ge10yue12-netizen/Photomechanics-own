#include "BleControlServer.h"

#include "../RemoteStatusText.h"

#include "BleCommandProtocol.h"
#include "BleWinRtWorker.h"
#include <QJsonDocument>
#include <QMetaType>
BleControlServer::BleControlServer(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<BleAdapterInfo>("BleAdapterInfo");
    m_statusTimer = new QTimer(this);
    m_statusTimer->setInterval(2000);
    connect(m_statusTimer, &QTimer::timeout, this, &BleControlServer::onStatusTimer);
}

BleControlServer::~BleControlServer()
{
    stop();
    if (m_winRtThread.isRunning())
    {
        m_winRtThread.quit();
        m_winRtThread.wait(3000);
    }
    delete m_worker;
    m_worker = nullptr;
}

// 懒创建 WinRT 工作线程；GATT 操作均在 m_winRtThread 执行。
void BleControlServer::ensureWorkerThread()
{
    if (m_worker)
        return;

    m_worker = new BleWinRtWorker();
    m_worker->moveToThread(&m_winRtThread);
    connect(&m_winRtThread, &QThread::started, m_worker, &BleWinRtWorker::initApartment);
    connect(m_worker, &BleWinRtWorker::commandReceived, this, &BleControlServer::ingestBleCommand,
            Qt::QueuedConnection);
    connect(m_worker, &BleWinRtWorker::serverError, this, &BleControlServer::reportGattError, Qt::QueuedConnection);
    m_winRtThread.start();
}

bool BleControlServer::start(const RemoteConfig &cfg)
{
    stop();
    ensureWorkerThread();

    // 阻塞跨线程查询适配器；须在 WinRT 线程执行。
    BleAdapterInfo adapter;
    QMetaObject::invokeMethod(m_worker, "queryAdapter", Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(BleAdapterInfo, adapter));
    m_adapterInfo = adapter;
    if (!adapter.available)
    {
        m_lastError = adapter.message;
        emit serverError(m_lastError);
        return false;
    }

    m_token = cfg.token;

    bool ok = false;
    QMetaObject::invokeMethod(m_worker, "startServer", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, ok),
                              Q_ARG(QString, cfg.bleDeviceName));
    if (!ok)
    {
        QString err;
        QMetaObject::invokeMethod(m_worker, "serverLastError", Qt::BlockingQueuedConnection, Q_RETURN_ARG(QString, err));
        m_lastError = err;
        if (m_lastError.isEmpty())
            m_lastError = QStringLiteral("BLE 服务启动失败");
        emit serverError(m_lastError);
        return false;
    }

    m_lastError.clear();
    m_running = true;
    m_statusTimer->start();
    pushStatus();
    return true;
}

QString BleControlServer::statusSummary() const
{
    return RemoteStatusText::bleSummary(m_running, m_lastError);
}

void BleControlServer::stop()
{
    if (m_statusTimer)
        m_statusTimer->stop();

    // 关停前推送 ok=0，让手机 Central 立即进入断开态，避免长时间假连接。
    if (m_running && m_worker && m_winRtThread.isRunning())
    {
        QJsonObject offline;
        offline.insert(QStringLiteral("ok"), false);
        offline.insert(QStringLiteral("message"), QStringLiteral("PC已关闭"));
        const QByteArray payload =
            compactStatusJson(QJsonDocument(offline).toJson(QJsonDocument::Compact));
        QMetaObject::invokeMethod(m_worker, "notifyStatus", Qt::BlockingQueuedConnection,
                                  Q_ARG(QByteArray, payload));
    }

    m_running = false;

    if (m_worker && m_winRtThread.isRunning())
    {
        QMetaObject::invokeMethod(m_worker, "stopServer", Qt::BlockingQueuedConnection);
    }
}

bool BleControlServer::isRunning() const
{
    return m_running;
}

void BleControlServer::setStatusProvider(std::function<QJsonObject()> provider)
{
    m_statusProvider = std::move(provider);
}

// 主线程组装 JSON → compactStatusJson → QueuedConnection 投递至 WinRT 线程 Notify。
void BleControlServer::pushStatus()
{
    if (!m_running || !m_worker)
        return;

    const QJsonObject status = currentStatus();
    const QByteArray json = QJsonDocument(status).toJson(QJsonDocument::Compact);
    const QByteArray payload = compactStatusJson(json);
    QMetaObject::invokeMethod(m_worker, "notifyStatus", Qt::QueuedConnection, Q_ARG(QByteArray, payload));
}

void BleControlServer::onStatusTimer()
{
    pushStatus();
}

void BleControlServer::ingestBleCommand(QByteArray raw)
{
    handleRawCommand(raw);
}

void BleControlServer::reportGattError(QString message)
{
    m_lastError = message;
    emit serverError(message);
}

// 解析 cmd:token；status 命令仅推送状态，其余转发 commandReceived。
void BleControlServer::handleRawCommand(const QByteArray &raw)
{
    const BleCommandParseResult parsed = parseBleCommand(raw, m_token);
    if (!parsed.valid)
    {
        if (!parsed.error.isEmpty())
            emit serverError(parsed.error);
        return;
    }

    if (parsed.cmd == QStringLiteral("status"))
    {
        pushStatus();
        return;
    }

    emit commandReceived(parsed.cmd);
    pushStatus();
}

QJsonObject BleControlServer::currentStatus() const
{
    if (m_statusProvider)
        return m_statusProvider();

    QJsonObject obj;
    obj.insert(QStringLiteral("ok"), true);
    obj.insert(QStringLiteral("message"), QStringLiteral("idle"));
    return obj;
}