#include "RemoteHost.h"

#include "RemoteStatusText.h"

#include <utility>

RemoteHost::RemoteHost(QObject *parent)
    : QObject(parent)
{
    connect(&m_kit, &RemoteKit::commandReceived, this, &RemoteHost::commandReceived);
}

void RemoteHost::setStatusLabel(QLabel *httpLabel)
{
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
        refreshStatusLabel();
        return false;
    }
    const bool ok = m_kit.start();
    refreshStatusLabel();
    return ok;
}

void RemoteHost::shutdown()
{
    m_kit.stop();
    refreshStatusLabel();
}

void RemoteHost::refreshStatusLabel()
{
    if (!m_httpLabel)
        return;
    m_httpLabel->setText(RemoteStatusText::httpSummary(m_kit.http().isListening(),
                                                       m_kit.http().lastError()));
    m_httpLabel->setToolTip(QString());
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
    if (!m_kit.http().isListening() && !m_kit.http().lastError().isEmpty())
    {
        lines << QStringLiteral("HTTP 遥控：%1")
                     .arg(RemoteStatusText::humanizeHttpListenError(m_kit.http().lastError()));
    }
    return lines;
}
