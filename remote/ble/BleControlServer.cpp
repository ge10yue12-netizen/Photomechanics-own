#include "BleControlServer.h"

#include "../RemoteStatusText.h"

#include "BleCommandProtocol.h"
#include "BleWinRtWorker.h"
#include <QJsonDocument>
#include <QMetaType>

// 构造：注册元类型并启动 2s 状态定时推送。
BleControlServer::BleControlServer(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<BleAdapterInfo>("BleAdapterInfo");
    m_statusTimer = new QTimer(this);
    m_statusTimer->setInterval(2000);
    connect(m_statusTimer, &QTimer::timeout, this, &BleControlServer::onStatusTimer);
}

// 析构：停止服务并等待 WinRT 线程退出。
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

// 检测 BLE 适配器并在 WinRT 线程启动 GATT Server。
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

// 返回 BLE 通道状态摘要文案。
QString BleControlServer::statusSummary() const
{
    return RemoteStatusText::bleSummary(m_running, m_lastError);
}

// 停止 GATT Server；关停前推送 ok=false 离线 Notify。
void BleControlServer::stop()
{
    if (m_statusTimer)
        m_statusTimer->stop();

    // 关停前推送 ok=false，使 Central 立即进入断开态。
    if (m_running && m_worker && m_winRtThread.isRunning())
    {
        QJsonObject offline;
        offline.insert(QStringLiteral("ok"), false);
        offline.insert(QStringLiteral("message"), QStringLiteral("主机服务已停止"));
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

// 返回 GATT Server 是否处于运行状态。
bool BleControlServer::isRunning() const
{
    return m_running;
}

// 注册状态 JSON 提供函数。
void BleControlServer::setStatusProvider(std::function<QJsonObject()> provider)
{
    m_statusProvider = std::move(provider);
}

// 注册命令互斥 guard 与本服务来源标识。
void BleControlServer::setControlGuard(RemoteControlGuard *guard, RemoteControlSource source)
{
    m_controlGuard = guard;
    m_controlSource = source;
}

// 主线程组装 JSON 并经 QueuedConnection 投递至 WinRT 线程 Notify。
void BleControlServer::pushStatus()
{
    if (!m_running || !m_worker)
        return;

    const QJsonObject status = RemoteControlGuard::statusWithGuard(m_controlGuard, m_controlSource, currentStatus());
    const QByteArray json = QJsonDocument(status).toJson(QJsonDocument::Compact);
    const QByteArray payload = compactStatusJson(json);
    QMetaObject::invokeMethod(m_worker, "notifyStatus", Qt::QueuedConnection, Q_ARG(QByteArray, payload));
}

// 定时器回调：周期性推送状态 Notify。
void BleControlServer::onStatusTimer()
{
    pushStatus();
}

// WinRT 线程 QueuedConnection 入口：转发至 handleRawCommand。
void BleControlServer::ingestBleCommand(QByteArray raw)
{
    handleRawCommand(raw);
}

// 记录 GATT 错误并发出 serverError 信号。
void BleControlServer::reportGattError(QString message)
{
    m_lastError = message;
    emit serverError(message);
}

// 解析 cmd:token；status/release 仅推送状态，其余转发 commandReceived。
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

    if (parsed.cmd == QStringLiteral("release"))
    {
        if (m_controlGuard)
            m_controlGuard->release(m_controlSource);
        pushStatus();
        return;
    }

    const RemoteControlGuard::Decision access =
        RemoteControlGuard::tryCommand(m_controlGuard, m_controlSource);
    if (access.blocked)
    {
        emit serverError(access.message);
        return;
    }

    emit commandReceived(parsed.cmd);
    pushStatus();
}

// 调用 statusProvider；未注册时返回默认空闲状态 JSON。
QJsonObject BleControlServer::currentStatus() const
{
    if (m_statusProvider)
        return m_statusProvider();
    QJsonObject obj;
    obj.insert(QStringLiteral("ok"), true);
    obj.insert(QStringLiteral("message"), QStringLiteral("空闲"));
    return obj;
}