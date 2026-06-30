#include "RemoteHost.h"

#include "RemoteStatusText.h"

#include <utility>

RemoteHost::RemoteHost(QObject *parent)
    : QObject(parent)
{
    connect(&m_kit, &RemoteKit::commandReceived, this, &RemoteHost::commandReceived);
    connect(&m_kit.ble(), &BleControlServer::serverError, this, &RemoteHost::onBleServerError);
}

void RemoteHost::setStatusLabels(QLabel *bleLabel, QLabel *httpLabel)
{
    m_bleLabel = bleLabel;
    m_httpLabel = httpLabel;
}

void RemoteHost::setStatusProvider(std::function<QJsonObject()> provider)
{
    m_kit.setStatusProvider(std::move(provider));
}

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

void RemoteHost::shutdown()
{
    m_kit.stop();
    refreshStatusLabels();
}

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

void RemoteHost::pushBleStatus()
{
    m_kit.pushBleStatus();
}

QStringList RemoteHost::startupWarnings() const
{
    QStringList lines;
    if (m_kit.configFilePath().isEmpty())
    {
        if (!m_kit.lastError().isEmpty())
            lines << QStringLiteral("遥控：%1").arg(m_kit.lastError());
        return lines;
    }
    if (!m_kit.ble().isRunning() && !m_kit.ble().lastError().isEmpty())
        lines << QStringLiteral("蓝牙遥控：%1").arg(m_kit.ble().lastError());
    if (!m_kit.http().isListening() && !m_kit.http().lastError().isEmpty())
    {
        lines << QStringLiteral("HTTP 遥控：%1")
                     .arg(RemoteStatusText::humanizeHttpListenError(m_kit.http().lastError()));
    }
    return lines;
}

void RemoteHost::onBleServerError(const QString &message)
{
    if (!message.isEmpty())
        emit notify(QStringLiteral("蓝牙遥控：%1").arg(message));
    refreshStatusLabels();
}
