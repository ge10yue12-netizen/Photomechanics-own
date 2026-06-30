#pragma once

#include <QByteArray>
#include <QHostAddress>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTcpServer>
#include <functional>

#include "NetConfigHelper.h"

class QTcpSocket;

class RemoteControlServer : public QObject
{
    Q_OBJECT

public:
    explicit RemoteControlServer(QObject *parent = nullptr);

    bool start(const RemoteConfig &cfg);
    void stop();
    bool isListening() const;
    quint16 serverPort() const;
    QString lastError() const { return m_lastError; }
    void setStatusProvider(std::function<QJsonObject()> provider);

signals:
    void commandReceived(const QString &cmd);

private slots:
    void onNewConnection();

private:
    void onSocketReadyRead(QTcpSocket *socket);
    void handleRequest(QTcpSocket *socket, const QByteArray &request);
    void writeJson(QTcpSocket *socket,
                   const QJsonObject &obj,
                   int statusCode = 200,
                   const QByteArray &statusText = QByteArrayLiteral("OK")) const;
    void writeResponse(QTcpSocket *socket,
                       int statusCode,
                       const QByteArray &statusText,
                       const QByteArray &contentType,
                       const QByteArray &body) const;
    bool hasValidToken(const QString &query, const QJsonObject &body) const;
    QJsonObject currentStatus() const;

    QTcpServer m_server;
    QString m_token = QStringLiteral("1234");
    QString m_lastError;
    std::function<QJsonObject()> m_statusProvider;
};
