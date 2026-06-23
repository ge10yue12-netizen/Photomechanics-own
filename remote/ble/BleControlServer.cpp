#include "BleControlServer.h"

#include "BleCommandProtocol.h"
#include "BleConfigHelper.h"
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

bool BleControlServer::start()
{
    stop();
    ensureWorkerThread();

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

    const BleNetConfig cfg = BleConfigHelper::load();
    m_token = cfg.token;

    bool ok = false;
    QMetaObject::invokeMethod(m_worker, "startServer", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, ok),
                              Q_ARG(QString, cfg.deviceName));
    if (!ok)
    {
        QString err;
        QMetaObject::invokeMethod(m_worker, "serverLastError", Qt::BlockingQueuedConnection, Q_RETURN_ARG(QString, err));
        m_lastError = err;
        if (m_lastError.isEmpty())
            m_lastError = QStringLiteral("BLE \u670d\u52a1\u542f\u52a8\u5931\u8d25");
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
    if (m_running)
    {
        const QString addr = m_adapterInfo.address.isEmpty() ? QStringLiteral("?") : m_adapterInfo.address;
        if (!m_adapterInfo.friendlyName.isEmpty())
        {
            return QStringLiteral("\u5df2\u5e7f\u64ad | \u624b\u673a\u641c\u300c%1\u300d| %2")
                .arg(m_adapterInfo.friendlyName, addr);
        }
        return QStringLiteral("\u5df2\u5e7f\u64ad\uff0c\u7b49\u5f85\u624b\u673a\u8fde\u63a5 | %1").arg(addr);
    }
    if (!m_lastError.isEmpty())
        return m_lastError;
    return QStringLiteral("\u672a\u542f\u52a8");
}

void BleControlServer::stop()
{
    if (m_statusTimer)
        m_statusTimer->stop();
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
