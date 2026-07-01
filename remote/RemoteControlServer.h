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

class RemoteControlServer : public QObject
{
    Q_OBJECT

public:
    // 构造对象并连接 QTcpServer::newConnection。
    explicit RemoteControlServer(QObject *parent = nullptr);

    // 优先按 cfg 中的 bind 监听；地址不适配当前机器时回退到所有网卡。
    bool start(const RemoteConfig &cfg);

    // 关闭 TCP 监听。
    void stop();

    // 返回当前是否处于监听状态。
    bool isListening() const;

    // 返回实际监听端口。
    quint16 serverPort() const;

    // 返回最近一次 start() 失败或相关错误信息。
    QString lastError() const { return m_lastError; }

    // 注册设备状态 JSON 提供函数，供 GET /api/status 与命令响应使用。
    void setStatusProvider(std::function<QJsonObject()> provider);

    // 可选：GET /api/preview.jpg 的 JPEG 提供函数；未注册时返回空图。
    void setPreviewProvider(std::function<QByteArray()> provider);

    // 注入跨通道互斥；source 通常为 MiniProgramHttp。
    void setControlGuard(RemoteControlGuard *guard, RemoteControlSource source);

signals:
    // 收到合法 POST /api/command 后发出，cmd 为命令名。
    void commandReceived(const QString &cmd);

private slots:
    // 接受新 TCP 连接并为每个 socket 绑定读取处理。
    void onNewConnection();

private:
    // 累积读取完整 HTTP 请求后交给 handleRequest。
    void onSocketReadyRead(QTcpSocket *socket);

    // 解析 HTTP 请求，校验 token 与路径，分发 status 查询或遥控命令。
    void handleRequest(QTcpSocket *socket, const QByteArray &request);

    // 将 JSON 对象作为 HTTP 响应体写回客户端。
    void writeJson(QTcpSocket *socket,
                   const QJsonObject &obj,
                   int statusCode = 200,
                   const QByteArray &statusText = QByteArrayLiteral("OK")) const;

    // 按给定状态码、类型与正文组装并发送 HTTP/1.1 响应。
    void writeResponse(QTcpSocket *socket,
                       int statusCode,
                       const QByteArray &statusText,
                       const QByteArray &contentType,
                       const QByteArray &body) const;

    // 校验 query 或 JSON body 中的 token 是否与服务器配置一致。
    bool hasValidToken(const QString &query, const QJsonObject &body) const;

    // 调用 statusProvider 获取当前状态；未注册时返回占位 JSON。
    QJsonObject currentStatus() const;

    QTcpServer m_server;
    RemoteControlGuard *m_controlGuard = nullptr;
    RemoteControlSource m_controlSource = RemoteControlSource::MiniProgramHttp;
    QString m_token = QStringLiteral("1234");
    QString m_lastError;
    std::function<QJsonObject()> m_statusProvider;
    std::function<QByteArray()> m_previewProvider;
};