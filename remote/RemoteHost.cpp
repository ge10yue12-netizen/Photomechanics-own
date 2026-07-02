#include "RemoteHost.h"

#include "RemoteStatusText.h"

#include <utility>

// 构造：绑定预览提供函数，转发 kit 命令与 BLE 错误信号。
RemoteHost::RemoteHost(QObject *parent)
    : QObject(parent)
{
    m_kit.setPreviewProvider([this]() { return m_preview.getLatestJpeg(); });
    connect(&m_kit, &RemoteKit::commandReceived, this, &RemoteHost::commandReceived);
    connect(&m_kit.ble(), &BleControlServer::serverError, this, &RemoteHost::onBleServerError);
}

// 保存 BLE/HTTP 状态标签指针。
void RemoteHost::setStatusLabels(QLabel *bleLabel, QLabel *httpLabel)
{
    m_bleLabel = bleLabel;
    m_httpLabel = httpLabel;
}

// 向 kit 注册状态 JSON 提供函数。
void RemoteHost::setStatusProvider(std::function<QJsonObject()> provider)
{
    m_kit.setStatusProvider(std::move(provider));
}

// 向 kit 注入命令通道互斥实例。
void RemoteHost::setControlGuard(RemoteControlGuard *guard)
{
    m_kit.setControlGuard(guard);
}

// 向 kit 注册自定义预览 JPEG 提供函数。
void RemoteHost::setPreviewProvider(std::function<QByteArray()> provider)
{
    m_kit.setPreviewProvider(std::move(provider));
}

// 加载配置、启动双通道并刷新状态标签。
bool RemoteHost::bootstrap()
{
    if (!m_kit.loadConfig())
    {
        refreshStatusLabels();
        return false;
    }
    const bool ok = m_kit.start();
    refreshStatusLabels();
    return ok;
}

// 停止双通道并刷新状态标签。
void RemoteHost::shutdown()
{
    m_kit.stop();
    refreshStatusLabels();
}

// 按 kit 监听/运行状态更新标签文本。
void RemoteHost::refreshStatusLabels()
{
    if (m_bleLabel)
    {
        m_bleLabel->setText(RemoteStatusText::bleSummary(m_kit.ble().isRunning(), m_kit.ble().lastError()));
        m_bleLabel->setToolTip(QString());
    }
    if (m_httpLabel)
    {
        m_httpLabel->setText(RemoteStatusText::httpSummary(m_kit.http().isListening(),
                                                         m_kit.http().lastError()));
        m_httpLabel->setToolTip(QString());
    }
}

// 触发 BLE 状态 Notify 推送。
void RemoteHost::pushBleStatus()
{
    m_kit.pushBleStatus();
}

// 查询 HTTP 预览请求是否在 TTL 窗口内。
bool RemoteHost::hadRecentPreviewRequest(qint64 nowMs, int ttlMs) const
{
    return m_kit.http().hadRecentPreviewRequest(nowMs, ttlMs);
}

// 汇总配置缺失或通道启动失败的警告文案。
QStringList RemoteHost::startupWarnings() const
{
    QStringList lines;
    if (m_kit.configFilePath().isEmpty())
    {
        if (!m_kit.lastError().isEmpty())
            lines << QStringLiteral("远程控制：%1").arg(m_kit.lastError());
        return lines;
    }
    if (!m_kit.ble().isRunning() && !m_kit.ble().lastError().isEmpty())
        lines << QStringLiteral("远程控制 BLE：%1").arg(m_kit.ble().lastError());
    if (!m_kit.http().isListening() && !m_kit.http().lastError().isEmpty())
    {
        lines << QStringLiteral("远程控制 HTTP：%1")
                     .arg(RemoteStatusText::humanizeHttpListenError(m_kit.http().lastError()));
    }
    return lines;
}

// 转发 BLE 错误并刷新标签。
void RemoteHost::onBleServerError(const QString &message)
{
    if (!message.isEmpty())
        emit notify(QStringLiteral("远程控制 BLE：%1").arg(message));
    refreshStatusLabels();
}

