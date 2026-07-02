#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QTcpServer>
#include <functional>

#include "MobileCommandGuardHooks.h"
#include "PreviewFrameCache.h"

class QTcpSocket;

// HTTP/1.1 遥控服务：解析请求、校验 token、路由至注入的提供者；POST 命令经宿主注入的互斥钩子
class MobileWebServer : public QObject
{
    Q_OBJECT

public:
    // 构造并连接 QTcpServer::newConnection
    explicit MobileWebServer(QObject *parent = nullptr);

    // 于 0.0.0.0 监听 port；失败时写入 lastError
    bool start(quint16 port);
    // 关闭监听套接字
    void stop();
    // 是否处于监听状态
    bool isListening() const;
    // 最近一次 start 失败原因
    QString lastError() const { return m_lastError; }
    // 距上次 preview 拉取（jpg/seq）未超过 idleMs 时为真；供宿主决定是否编码 JPEG
    bool isPreviewConsumerActive(int idleMs) const;
    // 手机端 POST /api/remote/off 时调用；由宿主注入关闭远程服务逻辑
    void setRemoteShutdownHandler(std::function<void()> handler);

    // 注册 token 校验回调；未注册时所有受保护路由拒绝访问
    void setTokenVerifier(std::function<bool(const QString &)> verifier);
    // 注册 /api/status 及命令响应使用的 JSON 状态提供者
    void setStatusProvider(std::function<QJsonObject()> provider);
    // 注册仅返回 JPEG 的预览提供者（兼容；优先使用 Snapshot 提供者）
    void setPreviewProvider(std::function<QByteArray()> provider);
    // 注册 JPEG 与帧序号一并返回的预览提供者
    void setPreviewSnapshotProvider(std::function<PreviewFrameCache::Snapshot()> provider);
    // 注册 /api/preview/seq 使用的帧序号提供者
    void setPreviewSeqProvider(std::function<quint64()> provider);
    // 注册 /api/config 使用的客户端轮询配置提供者
    void setConfigProvider(std::function<QJsonObject()> provider);
    // 设置 GET /mobile 返回的 HTML
    void setMobileHtml(const QByteArray &html);
    // 绑定 POST 命令互斥钩子；未注入时不启用互斥
    void setCommandGuardHooks(MobileCommandGuardHooks hooks);

signals:
    // POST 命令校验通过后发出
    void commandReceived(const QString &cmd);
    // token 校验通过后发出，用于检测客户端在线
    void clientAccessed(const QString &path);

private slots:
    // 接受 pending 连接并绑定 readyRead
    void onNewConnection();

private:
    // 增量读取并组装 HTTP 请求，支持 Keep-Alive 连续处理
    void onSocketReadyRead(QTcpSocket *socket);
    // 解析单条请求并写回响应；返回 true 表示保持 Keep-Alive 连接
    bool handleRequest(QTcpSocket *socket, const QByteArray &request, bool clientWantsKeepAlive);
    // 发出 commandReceived 并以最新状态 JSON 响应
    void replyCommand(QTcpSocket *socket, const QString &cmd);
    // 将 QJsonObject 序列化为 application/json 响应
    void writeJson(QTcpSocket *socket,
                   const QJsonObject &obj,
                   int statusCode = 200,
                   const QByteArray &statusText = QByteArrayLiteral("OK")) const;
    // 写入完整 HTTP/1.1 响应头与 body
    void writeResponse(QTcpSocket *socket,
                       int statusCode,
                       const QByteArray &statusText,
                       const QByteArray &contentType,
                       const QByteArray &body,
                       const QByteArray &extraHeaders = QByteArray(),
                       bool closeConnection = true) const;
    // 校验 URL query 中的 token 参数
    bool verifyToken(const QString &query) const;
    // 调用已安装的状态提供者；未安装时返回占位 JSON
    QJsonObject currentStatus() const;
    // 记录客户端正在拉取预览流
    void touchPreviewConsumer();

    QTcpServer m_server;
    MobileCommandGuardHooks m_commandGuardHooks;
    QString m_lastError;
    QByteArray m_mobileHtml;
    std::function<bool(const QString &)> m_tokenVerifier;
    std::function<QJsonObject()> m_statusProvider;
    std::function<QByteArray()> m_previewProvider;
    std::function<PreviewFrameCache::Snapshot()> m_previewSnapshotProvider;
    std::function<quint64()> m_previewSeqProvider;
    std::function<QJsonObject()> m_configProvider;
    std::function<void()> m_remoteShutdownHandler;
    qint64 m_lastPreviewPullMs = 0;
};
