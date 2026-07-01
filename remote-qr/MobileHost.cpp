#include "MobileHost.h"

#include <QDateTime>
#include <QFile>
#include <climits>

MobileHost::MobileHost(QObject *parent)
    : QObject(parent)
{
    m_expiryTimer.setSingleShot(true);
    connect(&m_expiryTimer, &QTimer::timeout, this, &MobileHost::onTokenExpired);

    m_phoneWatchTimer.setInterval(500);
    connect(&m_phoneWatchTimer, &QTimer::timeout, this, &MobileHost::onPhoneIdleCheck);

    connect(&m_server, &MobileWebServer::commandReceived, this, &MobileHost::commandReceived);
    connect(&m_server, &MobileWebServer::clientAccessed, this, [this](const QString &path) {
        Q_UNUSED(path);
        m_lastPhoneActivityMs = QDateTime::currentMSecsSinceEpoch();
        if (!m_phoneConnected)
        {
            m_phoneConnected = true;
            emit phoneConnected();
        }
    });

    m_server.setTokenVerifier([this](const QString &token) { return m_token.verifyToken(token); });
    m_server.setPreviewProvider([this]() { return m_preview.getLatestJpeg(); });
    m_server.setConfigProvider([this]() { return buildClientConfigJson(); });
}

bool MobileHost::reloadSettings()
{
    QString err;
    if (!MobileConfigHelper::load(m_cfg, &err))
    {
        m_lastError = err;
        return false;
    }
    m_token.setLifetimeMs(static_cast<qint64>(m_cfg.tokenLifetimeSec) * 1000);
    m_preview.setEncodeOptions(m_cfg.previewMaxWidth, m_cfg.previewMaxHeight, m_cfg.previewJpegQuality);
    m_lastError.clear();
    return true;
}

bool MobileHost::startSession(const QString &displayIp)
{
    const QString ip = displayIp.trimmed();
    if (ip.isEmpty())
    {
        m_lastError = QStringLiteral("未选择主机地址");
        return false;
    }

    syncExpiredSession();
    if (isSessionActive() && m_displayIp == ip)
        return true;

    if (isSessionActive())
        stopSession();

    if (!reloadSettings() || !loadMobileHtml())
        return false;

    m_displayIp = ip;
    m_phoneConnected = false;
    m_lastPhoneActivityMs = 0;
    m_token.refreshToken();

    if (!m_server.start(m_cfg.httpPort))
    {
        m_lastError = m_server.lastError();
        m_token.invalidate();
        m_displayIp.clear();
        return false;
    }

    scheduleTokenExpiry();
    m_phoneWatchTimer.start();
    emit sessionStarted(mobileUrl());
    return true;
}

void MobileHost::stopSession()
{
    m_phoneWatchTimer.stop();
    m_expiryTimer.stop();
    m_server.stop();
    m_token.invalidate();
    m_displayIp.clear();
    m_phoneConnected = false;
    m_lastPhoneActivityMs = 0;
    if (m_controlGuard)
        m_controlGuard->release(RemoteControlSource::QrBrowser);
    emit sessionStopped();
}

bool MobileHost::isSessionActive() const
{
    return m_server.isListening() && m_token.hasValidToken();
}

void MobileHost::syncExpiredSession()
{
    if (m_server.isListening() && !m_token.hasValidToken())
    {
        m_lastError = QStringLiteral("会话 token 已过期");
        stopSession();
    }
}

QString MobileHost::mobileUrl() const
{
    if (m_displayIp.isEmpty() || !m_token.hasValidToken())
        return QString();
    return QStringLiteral("http://%1:%2/mobile?token=%3")
        .arg(m_displayIp)
        .arg(m_cfg.httpPort)
        .arg(m_token.currentToken());
}

void MobileHost::clearPreviewFrame()
{
    m_preview.clear();
}

void MobileHost::setStatusProvider(std::function<QJsonObject()> provider)
{
    m_server.setStatusProvider(std::move(provider));
}

void MobileHost::setControlGuard(RemoteControlGuard *guard)
{
    m_controlGuard = guard;
    m_server.setControlGuard(guard, RemoteControlSource::QrBrowser);
}

void MobileHost::onTokenExpired()
{
    if (!m_server.isListening())
        return;
    m_lastError = QStringLiteral("会话 token 已过期");
    stopSession();
}

void MobileHost::onPhoneIdleCheck()
{
    if (!m_phoneConnected || !isSessionActive() || m_lastPhoneActivityMs <= 0)
        return;

    if (QDateTime::currentMSecsSinceEpoch() - m_lastPhoneActivityMs < phoneIdleTimeoutMs())
        return;

    m_phoneConnected = false;
    m_lastPhoneActivityMs = 0;
    if (m_controlGuard)
        m_controlGuard->release(RemoteControlSource::QrBrowser);
    emit phoneDisconnected();
}

void MobileHost::scheduleTokenExpiry()
{
    m_expiryTimer.stop();
    const qint64 ms = static_cast<qint64>(m_cfg.tokenLifetimeSec) * 1000;
    if (ms > 0)
        m_expiryTimer.start(static_cast<int>(qMin(ms, static_cast<qint64>(INT_MAX))));
}

int MobileHost::phoneIdleTimeoutMs() const
{
    const int pollMax = qMax(m_cfg.previewPollMs, m_cfg.statusPollMs);
    return qBound(2000, pollMax * 4 + 1000, 30000);
}

bool MobileHost::loadMobileHtml()
{
    if (!m_mobileHtml.isEmpty())
        return true;

    QFile file(QStringLiteral(":/remote-qr/assets/mobile.html"));
    if (!file.open(QIODevice::ReadOnly))
    {
        m_lastError = QStringLiteral("未找到 mobile.html（:/remote-qr/assets/mobile.html）");
        return false;
    }
    m_mobileHtml = file.readAll();
    m_server.setMobileHtml(m_mobileHtml);
    return true;
}

QJsonObject MobileHost::buildClientConfigJson() const
{
    return {
        {QStringLiteral("previewPollMs"), m_cfg.previewPollMs},
        {QStringLiteral("statusPollMs"), m_cfg.statusPollMs},
    };
}
