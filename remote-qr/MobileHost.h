#pragma once

#include "MobileConfig.h"
#include "MobileWebServer.h"
#include "PreviewFrameCache.h"
#include "TokenManager.h"

#include "../remote/RemoteControlGuard.h"

#include <QJsonObject>
#include <QObject>
#include <QTimer>

// 扫码 HTTP 遥控集成门面。
// 职责：会话 token 生命周期、HTTP 服务启停、预览缓存、客户端连接状态通知。
class MobileHost : public QObject
{
    Q_OBJECT

public:
    explicit MobileHost(QObject *parent = nullptr);

    // 在 displayIp 上启动 HTTP 会话；同 IP 且会话有效时直接返回 true。
    bool startSession(const QString &displayIp);

    // 停止 HTTP 服务并失效 token。
    void stopSession();

    // 返回 HTTP 正在监听且 token 未过期。
    bool isSessionActive() const;

    // token 过期时自动 stopSession。
    void syncExpiredSession();

    // 返回当前会话访问 URL；无有效会话时为空。
    QString mobileUrl() const;

    // 返回最近一次操作失败原因。
    QString lastError() const { return m_lastError; }

    // 返回预览 JPEG 缓存引用。
    PreviewFrameCache &previewCache() { return m_preview; }

    // 清空预览 JPEG 缓存。
    void clearPreviewFrame();

    // 返回浏览器客户端是否处于活跃连接状态。
    bool isPhoneConnected() const { return m_phoneConnected; }

    // 返回当前会话绑定的展示 IP。
    QString sessionDisplayIp() const { return m_displayIp; }

    // 注册设备状态 JSON 提供函数。
    void setStatusProvider(std::function<QJsonObject()> provider);

    // 注入命令通道互斥实例。
    void setControlGuard(RemoteControlGuard *guard);

    // 判断扫码 HTTP 在 ttlMs 内是否收到 GET /api/preview.jpg 请求。
    bool hadRecentPreviewRequest(qint64 nowMs, int ttlMs) const;

signals:
    // HTTP 收到合法遥控命令时发出。
    void commandReceived(const QString &cmd);

    // HTTP 会话启动成功时发出，参数为 mobileUrl。
    void sessionStarted(const QString &url);

    // HTTP 会话结束时发出。
    void sessionStopped();

    // 检测到浏览器客户端首次访问时发出。
    void phoneConnected();

    // 浏览器客户端空闲超时或断开时发出。
    void phoneDisconnected();

private slots:
    // token 定时器到期时停止会话。
    void onTokenExpired();

    // 周期性检测浏览器客户端是否空闲超时。
    void onPhoneIdleCheck();

private:
    // 重新加载 mobile.ini 并更新 token 与预览参数。
    bool reloadSettings();

    // 从 Qt 资源加载 mobile.html 并注入 HTTP 服务。
    bool loadMobileHtml();

    // 按 tokenLifetimeSec 启动单次过期定时器。
    void scheduleTokenExpiry();

    // 根据轮询间隔计算客户端空闲判定阈值（毫秒）。
    int phoneIdleTimeoutMs() const;

    // 构建 GET /api/config 返回的客户端轮询参数 JSON。
    QJsonObject buildClientConfigJson() const;

    MobileConfig m_cfg;
    QString m_displayIp;
    QString m_lastError;
    TokenManager m_token;
    PreviewFrameCache m_preview;
    MobileWebServer m_server;
    RemoteControlGuard *m_controlGuard = nullptr;
    QByteArray m_mobileHtml;
    QTimer m_expiryTimer;
    QTimer m_phoneWatchTimer;
    qint64 m_lastPhoneActivityMs = 0;
    bool m_phoneConnected = false;
};

