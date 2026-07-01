#pragma once

#include "MobileConfig.h"
#include "MobileWebServer.h"
#include "PreviewFrameCache.h"
#include "TokenManager.h"

#include "../remote/RemoteControlGuard.h"

#include <QJsonObject>
#include <QObject>
#include <QTimer>

/**
 * @brief 扫码遥控集成门面（宿主唯一入口）。
 *
 * 构造 → setStatusProvider → connect 信号 → startSession。
 * 预览：取帧路径 updateFrame；关相机 clearPreviewFrame。
 */
class MobileHost : public QObject
{
    Q_OBJECT

public:
    explicit MobileHost(QObject *parent = nullptr);

    bool startSession(const QString &displayIp);
    void stopSession();
    bool isSessionActive() const;
    void syncExpiredSession();

    QString mobileUrl() const;
    QString lastError() const { return m_lastError; }
    PreviewFrameCache &previewCache() { return m_preview; }

    /** @brief 相机关闭时调用：清空 JPEG 缓存，手机端仅清图像、预览框保留。 */
    void clearPreviewFrame();

    bool isPhoneConnected() const { return m_phoneConnected; }
    QString sessionDisplayIp() const { return m_displayIp; }

    void setStatusProvider(std::function<QJsonObject()> provider);
    void setControlGuard(RemoteControlGuard *guard);

signals:
    void commandReceived(const QString &cmd);
    void sessionStarted(const QString &url);
    void sessionStopped();
    void phoneConnected();
    void phoneDisconnected();

private slots:
    void onTokenExpired();
    void onPhoneIdleCheck();

private:
    bool reloadSettings();
    bool loadMobileHtml();
    void scheduleTokenExpiry();
    int phoneIdleTimeoutMs() const;
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
