#pragma once

#include <QByteArray>
#include <QHostAddress>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTcpServer>
#include <functional>

#include "NetConfigHelper.h"
#include "RemoteControlGuard.h"

class QTcpSocket;

// WiFi HTTP 遥控服务（QTcpServer 短连接）。
// 路由：GET /api/status、GET /api/preview.jpg、POST /api/command、POST /api/release。
class RemoteControlServer : public QObject
{
    Q_OBJECT

public:
    // 构造并连接 QTcpServer::newConnection。
    explicit RemoteControlServer(QObject *parent = nullptr);

    // 按 cfg 绑定地址与端口监听；bind 不可用时回退至全接口。
    bool start(const RemoteConfig &cfg);

    // 关闭 TCP 监听。
    void stop();

    // 返回当前是否处于监听状态。
    bool isListening() const;

    // 返回实际监听端口。
    quint16 serverPort() const;

    // 返回最近一次 start 失败或回退提示信息。
    QString lastError() const { return m_lastError; }

    // 注册设备状态 JSON 提供函数。
    void setStatusProvider(std::function<QJsonObject()> provider);

    // 注册 GET /api/preview.jpg 的 JPEG 提供函数。
    void setPreviewProvider(std::function<QByteArray()> provider);

    // 返回最近一次 preview 请求的毫秒时间戳。
    qint64 lastPreviewRequestMs() const;

    // 判断 ttlMs 内是否存在 preview 请求。
    bool hadRecentPreviewRequest(qint64 nowMs, int ttlMs) const;

    // 注入命令通道互斥及本服务来源标识。
    void setControlGuard(RemoteControlGuard *guard, RemoteControlSource source);

signals:
    // 收到合法 POST /api/command 后发出。
    void commandReceived(const QString &cmd);

private slots:
    // 接受新 TCP 连接并绑定读取处理。
    void onNewConnection();

private:
    // 累积读取完整 HTTP 请求后分发。
    void onSocketReadyRead(QTcpSocket *socket);

    // 解析 HTTP 请求，校验 token 与路径并分发处理。
    void handleRequest(QTcpSocket *socket, const QByteArray &request);

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

    // 校验 query 或 JSON body 中的 token。
    bool hasValidToken(const QString &query, const QJsonObject &body) const;

    // 调用 statusProvider 获取当前状态 JSON。
    QJsonObject currentStatus() const;

    QTcpServer m_server;
    RemoteControlGuard *m_controlGuard = nullptr;
    RemoteControlSource m_controlSource = RemoteControlSource::MiniProgramHttp;
    QString m_token = QStringLiteral("1234");
    QString m_lastError;
    std::function<QJsonObject()> m_statusProvider;
    std::function<QByteArray()> m_previewProvider;
    qint64 m_lastPreviewRequestMs = 0;
};

