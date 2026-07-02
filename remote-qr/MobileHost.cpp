#include "MobileHost.h"

#include <QDateTime>
#include <QFile>
#include <climits>

// 初始化定时器、转发信号、注册 token/预览/配置提供者
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
    m_server.setPreviewSnapshotProvider([this]() { return m_preview.snapshot(); });
    m_server.setPreviewSeqProvider([this]() { return m_preview.lastFrameSeq(); });
    m_server.setConfigProvider([this]() { return buildClientConfigJson(); });
}

// 读取 mobile.ini 并应用到 token 与预览编码参数
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

// 启动 HTTP 会话：配置、HTML、token、监听
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

// 停止定时器、HTTP、失效 token、释放命令锁
void MobileHost::stopSession()
{
    m_phoneWatchTimer.stop();
    m_expiryTimer.stop();
    m_server.stop();
    m_token.invalidate();
    m_displayIp.clear();
    m_phoneConnected = false;
    m_lastPhoneActivityMs = 0;
    if (m_commandGuardHooks.isActive())
        m_commandGuardHooks.release();
    emit sessionStopped();
}

// 监听中且 token 有效
bool MobileHost::isSessionActive() const
{
    return m_server.isListening() && m_token.hasValidToken();
}

// 监听中但 token 已失效时清理会话
void MobileHost::syncExpiredSession()
{
    if (m_server.isListening() && !m_token.hasValidToken())
    {
        m_lastError = QStringLiteral("会话 token 已过期");
        stopSession();
    }
}

// 拼接 http://{ip}:{port}/mobile?token={token}
QString MobileHost::mobileUrl() const
{
    if (m_displayIp.isEmpty() || !m_token.hasValidToken())
        return QString();
    return QStringLiteral("http://%1:%2/mobile?token=%3")
        .arg(m_displayIp)
        .arg(m_cfg.httpPort)
        .arg(m_token.currentToken());
}

// 清空预览 JPEG 缓存
void MobileHost::clearPreviewFrame()
{
    m_preview.clear();
}

bool MobileHost::wantsPreviewEncoding() const
{
    if (!isSessionActive())
        return false;
    return m_server.isPreviewConsumerActive(previewEncodeIdleMs());
}

// 转发至 MobileWebServer
void MobileHost::setStatusProvider(std::function<QJsonObject()> provider)
{
    m_server.setStatusProvider(std::move(provider));
}

// 保存钩子并转发至 MobileWebServer
void MobileHost::setCommandGuardHooks(MobileCommandGuardHooks hooks)
{
    m_commandGuardHooks = std::move(hooks);
    m_server.setCommandGuardHooks(m_commandGuardHooks);
}

void MobileHost::setRemoteShutdownHandler(std::function<void()> handler)
{
    m_server.setRemoteShutdownHandler(std::move(handler));
}

// token 过期回调
void MobileHost::onTokenExpired()
{
    if (!m_server.isListening())
        return;
    m_lastError = QStringLiteral("会话 token 已过期");
    stopSession();
}

// 无 HTTP 活动超过阈值时标记断开并释放命令锁
void MobileHost::onPhoneIdleCheck()
{
    if (!m_phoneConnected || !isSessionActive() || m_lastPhoneActivityMs <= 0)
        return;

    if (QDateTime::currentMSecsSinceEpoch() - m_lastPhoneActivityMs < phoneIdleTimeoutMs())
        return;

    m_phoneConnected = false;
    m_lastPhoneActivityMs = 0;
    if (m_commandGuardHooks.isActive())
        m_commandGuardHooks.release();
    emit phoneDisconnected();
}

// 启动单次 token 过期定时器
void MobileHost::scheduleTokenExpiry()
{
    m_expiryTimer.stop();
    const qint64 ms = static_cast<qint64>(m_cfg.tokenLifetimeSec) * 1000;
    if (ms > 0)
        m_expiryTimer.start(static_cast<int>(qMin(ms, static_cast<qint64>(INT_MAX))));
}

// 空闲阈值：轮询间隔的 4 倍加 1s，限制在 [2s, 30s]
int MobileHost::phoneIdleTimeoutMs() const
{
    const int pollMax = qMax(m_cfg.previewPollMs, m_cfg.statusPollMs);
    return qBound(2000, pollMax * 4 + 1000, 30000);
}

int MobileHost::previewEncodeIdleMs() const
{
    return qBound(2000, m_cfg.previewPollMs * 4 + 1000, 30000);
}

// 自 qrc 读取 mobile.html 并注入 HTTP 服务
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

// 返回浏览器轮询间隔配置
QJsonObject MobileHost::buildClientConfigJson() const
{
    return {
        {QStringLiteral("previewPollMs"), m_cfg.previewPollMs},
        {QStringLiteral("statusPollMs"), m_cfg.statusPollMs},
    };
}
