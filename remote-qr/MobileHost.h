#pragma once

#include "MobileCommandGuardHooks.h"
#include "MobileConfig.h"
#include "MobileWebServer.h"
#include "PreviewFrameCache.h"
#include "TokenManager.h"

#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <functional>

// 扫码会话门面：HTTP 服务、token、预览缓存、客户端在线检测
class MobileHost : public QObject
{
    Q_OBJECT

public:
    // 初始化定时器、连接信号、注册默认 HTTP 提供者
    explicit MobileHost(QObject *parent = nullptr);

    // 加载配置、启动 HTTP、签发 token；displayIp 写入二维码 URL
    bool startSession(const QString &displayIp);
    // 停止 HTTP、失效 token、释放命令锁、重置客户端状态
    void stopSession();
    // HTTP 监听中且 token 仍有效
    bool isSessionActive() const;
    // token 过期且仍在监听时终止会话
    void syncExpiredSession();
    // 组装浏览器入口 URL
    QString mobileUrl() const;
    // 最近一次操作错误描述
    QString lastError() const { return m_lastError; }
    // 供采集路径写入预览帧
    PreviewFrameCache &previewCache() { return m_preview; }
    // 视频源停止时清空预览缓存
    void clearPreviewFrame();
    // 是否有客户端已通过 HTTP 访问
    bool isPhoneConnected() const { return m_phoneConnected; }
    // 会话有效且客户端近期在拉 preview 时为真；宿主据此决定是否 updateFrame
    bool wantsPreviewEncoding() const;
    // 当前会话 URL 中的 IPv4
    QString sessionDisplayIp() const { return m_displayIp; }
    // 转发状态 JSON 提供者至 MobileWebServer
    void setStatusProvider(std::function<QJsonObject()> provider);
    // 注入 POST 命令互斥钩子；未注入时不启用互斥
    void setCommandGuardHooks(MobileCommandGuardHooks hooks);
    // 客户端 POST /api/remote/off 时回调宿主关闭远程
    void setRemoteShutdownHandler(std::function<void()> handler);

signals:
    // 转发 MobileWebServer::commandReceived
    void commandReceived(const QString &cmd);
    // startSession 成功；参数为 mobileUrl()
    void sessionStarted(const QString &url);
    // stopSession 完成或 token 过期
    void sessionStopped();
    // 会话启动后首次 authenticated HTTP 访问
    void phoneConnected();
    // 超过空闲阈值无 HTTP 访问
    void phoneDisconnected();

private slots:
    // token  lifetime 到期时 stopSession
    void onTokenExpired();
    // 周期检测客户端空闲断开
    void onPhoneIdleCheck();

private:
    // 重载 mobile.ini 并同步至 TokenManager、PreviewFrameCache
    bool reloadSettings();
    // 自 qrc 加载 mobile.html（进程内仅一次）
    bool loadMobileHtml();
    // 按 tokenLifetimeSec 启动过期定时器
    void scheduleTokenExpiry();
    // 由轮询间隔推算客户端空闲阈值（毫秒）
    int phoneIdleTimeoutMs() const;
    // 预览编码保活窗口：与 mobile.ini preview/poll_ms 对齐
    int previewEncodeIdleMs() const;
    // 组装 GET /api/config 返回的 JSON
    QJsonObject buildClientConfigJson() const;

    MobileConfig m_cfg;
    QString m_displayIp;
    QString m_lastError;
    TokenManager m_token;
    PreviewFrameCache m_preview;
    MobileWebServer m_server;
    MobileCommandGuardHooks m_commandGuardHooks;
    QByteArray m_mobileHtml;
    QTimer m_expiryTimer;
    QTimer m_phoneWatchTimer;
    qint64 m_lastPhoneActivityMs = 0;
    bool m_phoneConnected = false;
};
