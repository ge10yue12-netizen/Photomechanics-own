#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QTcpServer>
#include <functional>

class QTcpSocket;

/**
 * @brief 扫码遥控 HTTP 服务（QTcpServer，C++14）。
 *
 * 路由见 remote-qr/README.md；所有 /api/* 与 /mobile 均校验 token。
 */
class MobileWebServer : public QObject
{
    Q_OBJECT

public:
    explicit MobileWebServer(QObject *parent = nullptr);

    bool start(quint16 port);
    void stop();
    bool isListening() const;
    QString lastError() const { return m_lastError; }

    void setTokenVerifier(std::function<bool(const QString &)> verifier);
    void setStatusProvider(std::function<QJsonObject()> provider);
    void setPreviewProvider(std::function<QByteArray()> provider);
    void setConfigProvider(std::function<QJsonObject()> provider);
    void setMobileHtml(const QByteArray &html);

signals:
    void commandReceived(const QString &cmd);
    void clientAccessed(const QString &path);

private slots:
    void onNewConnection();

private:
    void onSocketReadyRead(QTcpSocket *socket);
    void handleRequest(QTcpSocket *socket, const QByteArray &request);
    void replyCommand(QTcpSocket *socket, const QString &cmd);
    void writeJson(QTcpSocket *socket,
                   const QJsonObject &obj,
                   int statusCode = 200,
                   const QByteArray &statusText = QByteArrayLiteral("OK")) const;
    void writeResponse(QTcpSocket *socket,
                       int statusCode,
                       const QByteArray &statusText,
                       const QByteArray &contentType,
                       const QByteArray &body) const;
    bool verifyToken(const QString &query) const;
    QJsonObject currentStatus() const;

    QTcpServer m_server;
    QString m_lastError;
    QByteArray m_mobileHtml;
    std::function<bool(const QString &)> m_tokenVerifier;
    std::function<QJsonObject()> m_statusProvider;
    std::function<QByteArray()> m_previewProvider;
    std::function<QJsonObject()> m_configProvider;
};
