#include "MobileHost.h"

#include <QDateTime>
#include <QFile>
#include <climits>

// 构造：初始化定时器、HTTP 回调与 token/预览/配置提供函数。
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

// 读取 mobile.ini 并更新 token 有效期与预览编码参数。
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

// 在指定 IP 上启动 HTTP 会话并刷新 token。
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

// 停止 HTTP 服务、失效 token 并释放命令占用。
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

// 判断 HTTP 监听中且 token 有效。
bool MobileHost::isSessionActive() const
{
    return m_server.isListening() && m_token.hasValidToken();
}

// token 过期且 HTTP 仍在监听时停止会话。
void MobileHost::syncExpiredSession()
{
    if (m_server.isListening() && !m_token.hasValidToken())
    {
        m_lastError = QStringLiteral("会话 token 已过期");
        stopSession();
    }
}

// 拼接当前会话的 mobile 页面 URL。
QString MobileHost::mobileUrl() const
{
    if (m_displayIp.isEmpty() || !m_token.hasValidToken())
        return QString();
    return QStringLiteral("http://%1:%2/mobile?token=%3")
        .arg(m_displayIp)
        .arg(m_cfg.httpPort)
        .arg(m_token.currentToken());
}

// 清空预览 JPEG 缓存。
void MobileHost::clearPreviewFrame()
{
    m_preview.clear();
}

// 向 HTTP 服务注册状态 JSON 提供函数。
void MobileHost::setStatusProvider(std::function<QJsonObject()> provider)
{
    m_server.setStatusProvider(std::move(provider));
}

// 注入命令互斥 guard 并绑定 QrBrowser 来源。
void MobileHost::setControlGuard(RemoteControlGuard *guard)
{
    m_controlGuard = guard;
    m_server.setControlGuard(guard, RemoteControlSource::QrBrowser);
}

// 查询 HTTP 预览请求是否在 TTL 窗口内。
bool MobileHost::hadRecentPreviewRequest(qint64 nowMs, int ttlMs) const
{
    if (!m_server.isListening())
        return false;
    return m_server.hadRecentPreviewRequest(nowMs, ttlMs);
}

// token 过期定时器回调：停止会话。
void MobileHost::onTokenExpired()
{
    if (!m_server.isListening())
        return;
    m_lastError = QStringLiteral("会话 token 已过期");
    stopSession();
}

// 检测浏览器客户端空闲超时并发出 phoneDisconnected。
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

// 按 tokenLifetimeSec 启动单次过期定时器。
void MobileHost::scheduleTokenExpiry()
{
    m_expiryTimer.stop();
    const qint64 ms = static_cast<qint64>(m_cfg.tokenLifetimeSec) * 1000;
    if (ms > 0)
        m_expiryTimer.start(static_cast<int>(qMin(ms, static_cast<qint64>(INT_MAX))));
}

// 根据状态/预览轮询间隔计算空闲判定阈值。
int MobileHost::phoneIdleTimeoutMs() const
{
    const int pollMax = qMax(m_cfg.previewPollMs, m_cfg.statusPollMs);
    return qBound(2000, pollMax * 4 + 1000, 30000);
}

// 从 Qt 资源加载 mobile.html。
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

// 构建客户端轮询间隔配置 JSON。
QJsonObject MobileHost::buildClientConfigJson() const
{
    return {
        {QStringLiteral("previewPollMs"), m_cfg.previewPollMs},
        {QStringLiteral("statusPollMs"), m_cfg.statusPollMs},
    };
}

