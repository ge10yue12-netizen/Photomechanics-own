#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QTcpServer>
#include <functional>

#include "../remote/RemoteControlGuard.h"

class QTcpSocket;

// 扫码 HTTP 遥控服务（QTcpServer 短连接）。
// 路由：GET /mobile、GET /api/status、GET /api/preview.jpg、POST /api/camera/*、POST /api/release。
// 除 /favicon.ico 外均须校验会话 token。
class MobileWebServer : public QObject
{
    Q_OBJECT

public:
    // 构造并连接 QTcpServer::newConnection。
    explicit MobileWebServer(QObject *parent = nullptr);

    // 在指定端口绑定全接口监听。
    bool start(quint16 port);

    // 关闭 TCP 监听。
    void stop();

    // 返回当前是否处于监听状态。
    bool isListening() const;

    // 返回最近一次 start 失败原因。
    QString lastError() const { return m_lastError; }

    // 注册 token 校验函数。
    void setTokenVerifier(std::function<bool(const QString &)> verifier);

    // 注册设备状态 JSON 提供函数。
    void setStatusProvider(std::function<QJsonObject()> provider);

    // 注册 GET /api/preview.jpg 的 JPEG 提供函数。
    void setPreviewProvider(std::function<QByteArray()> provider);

    // 注册 GET /api/config 的配置 JSON 提供函数。
    void setConfigProvider(std::function<QJsonObject()> provider);

    // 设置 GET /mobile 返回的 HTML 页面内容。
    void setMobileHtml(const QByteArray &html);

    // 注入命令通道互斥及本服务来源标识。
    void setControlGuard(RemoteControlGuard *guard, RemoteControlSource source);

    // 返回最近一次 preview 请求的毫秒时间戳。
    qint64 lastPreviewRequestMs() const;

    // 判断 ttlMs 内是否存在 preview 请求。
    bool hadRecentPreviewRequest(qint64 nowMs, int ttlMs) const;

signals:
    // 收到合法 POST 遥控命令后发出。
    void commandReceived(const QString &cmd);

    // 任意合法请求通过 token 校验后发出。
    void clientAccessed(const QString &path);

private slots:
    // 接受新 TCP 连接并绑定读取处理。
    void onNewConnection();

private:
    // 累积读取完整 HTTP 请求后分发。
    void onSocketReadyRead(QTcpSocket *socket);

    // 解析 HTTP 请求，校验 token 并按路径分发 API。
    void handleRequest(QTcpSocket *socket, const QByteArray &request);

    // 发出 commandReceived 并写回命令响应 JSON。
    void replyCommand(QTcpSocket *socket, const QString &cmd);

    // 将 JSON 对象作为 HTTP 响应体写回客户端。
    void writeJson(QTcpSocket *socket,
                   const QJsonObject &obj,
                   int statusCode = 200,
                   const QByteArray &statusText = QByteArrayLiteral("OK")) const;

    // 组装 HTTP/1.1 响应并发送。
    void writeResponse(QTcpSocket *socket,
                       int statusCode,
                       const QByteArray &statusText,
                       const QByteArray &contentType,
                       const QByteArray &body) const;

    // 校验 query 中的 token。
    bool verifyToken(const QString &query) const;

    // 调用 statusProvider 获取当前状态 JSON。
    QJsonObject currentStatus() const;

    QTcpServer m_server;
    RemoteControlGuard *m_controlGuard = nullptr;
    RemoteControlSource m_controlSource = RemoteControlSource::QrBrowser;
    QString m_lastError;
    QByteArray m_mobileHtml;
    std::function<bool(const QString &)> m_tokenVerifier;
    std::function<QJsonObject()> m_statusProvider;
    std::function<QByteArray()> m_previewProvider;
    std::function<QJsonObject()> m_configProvider;
    qint64 m_lastPreviewRequestMs = 0;
};

