#include "RemoteControlServer.h"
#include "RemoteCommands.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QTcpSocket>
#include <QUrlQuery>
#include <utility>

namespace
{
const char *kRequestDataProperty = "remoteRequestData";

// 从请求行 target 中截取路径部分（去掉 ? 后查询串）。
QString requestPath(const QString &target)
{
    const int queryPos = target.indexOf(QLatin1Char('?'));
    return queryPos >= 0 ? target.left(queryPos) : target;
}

// 从请求行 target 中截取查询串（不含前导 ?）。
QString requestQuery(const QString &target)
{
    const int queryPos = target.indexOf(QLatin1Char('?'));
    return queryPos >= 0 ? target.mid(queryPos + 1) : QString();
}
}

RemoteControlServer::RemoteControlServer(QObject *parent)
    : QObject(parent)
{
    connect(&m_server, &QTcpServer::newConnection, this, &RemoteControlServer::onNewConnection);
}

// 优先按 ini 的 bind/port 监听；移植到新电脑时 IP 不匹配则回退到所有网卡。
bool RemoteControlServer::start(const RemoteConfig &cfg)
{
    if (m_server.isListening())
        stop();
    m_token = cfg.token;
    m_lastError.clear();

    const QHostAddress bindAddr(cfg.httpBind);
    if (m_server.listen(bindAddr, cfg.httpPort))
        return true;

    const QString bindError = QStringLiteral("%1（bind=%2 port=%3）")
                                  .arg(m_server.errorString(), cfg.httpBind)
                                  .arg(cfg.httpPort);
    m_server.close();

    if (!m_server.listen(QHostAddress::Any, cfg.httpPort))
    {
        m_lastError = QStringLiteral("%1；回退至全接口监听仍失败：%2（port=%3）")
                          .arg(bindError, m_server.errorString())
                          .arg(cfg.httpPort);
        return false;
    }

    m_lastError = QStringLiteral("%1；已回退至全接口监听").arg(bindError);
    return true;
}

void RemoteControlServer::stop()
{
    m_server.close();
}

bool RemoteControlServer::isListening() const
{
    return m_server.isListening();
}

quint16 RemoteControlServer::serverPort() const
{
    return m_server.serverPort();
}

void RemoteControlServer::setStatusProvider(std::function<QJsonObject()> provider)
{
    m_statusProvider = std::move(provider);
}

void RemoteControlServer::setControlGuard(RemoteControlGuard *guard, RemoteControlSource source)
{
    m_controlGuard = guard;
    m_controlSource = source;
}

void RemoteControlServer::setPreviewProvider(std::function<QByteArray()> provider)
{
    m_previewProvider = std::move(provider);
}

void RemoteControlServer::onNewConnection()
{
    while (auto *socket = m_server.nextPendingConnection())
    {
        socket->setParent(this);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { onSocketReadyRead(socket); });
        connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
    }
}

void RemoteControlServer::onSocketReadyRead(QTcpSocket *socket)
{
    QByteArray request = socket->property(kRequestDataProperty).toByteArray();
    request += socket->readAll();
    socket->setProperty(kRequestDataProperty, request);

    const int headerEnd = request.indexOf("\r\n\r\n");
    if (headerEnd < 0)
        return;

    int contentLength = 0;
    const QList<QByteArray> headerLines = request.left(headerEnd).split('\n');
    for (QByteArray line : headerLines)
    {
        line = line.trimmed();
        if (line.toLower().startsWith("content-length:"))
            contentLength = line.mid(line.indexOf(':') + 1).trimmed().toInt();
    }

    if (request.size() < headerEnd + 4 + contentLength)
        return;

    handleRequest(socket, request.left(headerEnd + 4 + contentLength));
}

void RemoteControlServer::handleRequest(QTcpSocket *socket, const QByteArray &request)
{
    const int headerEnd = request.indexOf("\r\n\r\n");
    const QByteArray header = request.left(headerEnd);
    const QByteArray bodyBytes = request.mid(headerEnd + 4);
    const QList<QByteArray> lines = header.split('\n');
    if (lines.isEmpty())
    {
        writeJson(socket, {{QStringLiteral("ok"), false}, {QStringLiteral("message"), QStringLiteral("请求格式错误")}}, 400, QByteArrayLiteral("Bad Request"));
        return;
    }

    const QList<QByteArray> firstLine = lines.first().trimmed().split(' ');
    if (firstLine.size() < 2)
    {
        writeJson(socket, {{QStringLiteral("ok"), false}, {QStringLiteral("message"), QStringLiteral("请求行错误")}}, 400, QByteArrayLiteral("Bad Request"));
        return;
    }

    const QString method = QString::fromLatin1(firstLine.at(0)).toUpper();
    const QString target = QString::fromUtf8(firstLine.at(1));
    const QString path = requestPath(target);
    const QString query = requestQuery(target);

    QJsonObject body;
    if (!bodyBytes.trimmed().isEmpty())
    {
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(bodyBytes, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject())
        {
            writeJson(socket, {{QStringLiteral("ok"), false}, {QStringLiteral("message"), QStringLiteral("JSON 格式错误")}}, 400, QByteArrayLiteral("Bad Request"));
            return;
        }
        body = doc.object();
    }

    if (method == QStringLiteral("GET") && path == QStringLiteral("/"))
    {
        writeJson(socket, {{QStringLiteral("ok"), true},
                           {QStringLiteral("message"), QStringLiteral("远程控制 API：GET /api/status")}});
        return;
    }

    if (path == QStringLiteral("/favicon.ico"))
    {
        writeResponse(socket, 204, QByteArrayLiteral("No Content"), QByteArrayLiteral("text/plain"), QByteArray());
        return;
    }

    if (!hasValidToken(query, body))
    {
        writeJson(socket, {{QStringLiteral("ok"), false}, {QStringLiteral("message"), QStringLiteral("认证 token 无效")}}, 403, QByteArrayLiteral("Forbidden"));
        return;
    }

    if (method == QStringLiteral("GET") && path == QStringLiteral("/api/status"))
    {
        writeJson(socket, RemoteControlGuard::statusWithGuard(m_controlGuard, m_controlSource, currentStatus()));
        return;
    }

    if (method == QStringLiteral("GET") && path == QStringLiteral("/api/preview.jpg"))
    {
        const QByteArray jpeg = m_previewProvider ? m_previewProvider() : QByteArray();
        writeResponse(socket, 200, QByteArrayLiteral("OK"),
                      QByteArrayLiteral("image/jpeg"), jpeg);
        return;
    }

    if (method == QStringLiteral("POST") && path == QStringLiteral("/api/release"))
    {
        if (m_controlGuard)
            m_controlGuard->release(m_controlSource);
        writeJson(socket, RemoteControlGuard::statusWithGuard(m_controlGuard, m_controlSource, currentStatus()));
        return;
    }

    if (method == QStringLiteral("POST") && path == QStringLiteral("/api/command"))
    {
        const QString cmd = body.value(QStringLiteral("cmd")).toString().trimmed();
        if (!isKnownRemoteCommand(cmd))
        {
            writeJson(socket, {{QStringLiteral("ok"), false}, {QStringLiteral("message"), QStringLiteral("未知命令")}}, 400, QByteArrayLiteral("Bad Request"));
            return;
        }

        const RemoteControlGuard::Decision access =
            RemoteControlGuard::tryCommand(m_controlGuard, m_controlSource);
        if (access.blocked)
        {
            writeJson(socket, {{QStringLiteral("ok"), false}, {QStringLiteral("message"), access.message}}, 409,
                      QByteArrayLiteral("Conflict"));
            return;
        }

        emit commandReceived(cmd);
        QJsonObject response = RemoteControlGuard::statusWithGuard(m_controlGuard, m_controlSource, currentStatus());
        response.insert(QStringLiteral("ok"), true);
        response.insert(QStringLiteral("cmd"), cmd);
        response.insert(QStringLiteral("message"), QStringLiteral("命令已受理"));
        writeJson(socket, response);
        return;
    }

    writeJson(socket, {{QStringLiteral("ok"), false}, {QStringLiteral("message"), QStringLiteral("接口不存在")}}, 404, QByteArrayLiteral("Not Found"));
}

void RemoteControlServer::writeJson(QTcpSocket *socket, const QJsonObject &obj, int statusCode, const QByteArray &statusText) const
{
    const QByteArray body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    writeResponse(socket, statusCode, statusText, QByteArrayLiteral("application/json; charset=utf-8"), body);
}

void RemoteControlServer::writeResponse(QTcpSocket *socket,
                                        int statusCode,
                                        const QByteArray &statusText,
                                        const QByteArray &contentType,
                                        const QByteArray &body) const
{
    QByteArray response;
    response += "HTTP/1.1 " + QByteArray::number(statusCode) + " " + statusText + "\r\n";
    response += "Content-Type: " + contentType + "\r\n";
    response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    response += "Connection: close\r\n";
    response += "Cache-Control: no-store\r\n";
    response += "\r\n";
    response += body;
    socket->write(response);
    socket->disconnectFromHost();
}

bool RemoteControlServer::hasValidToken(const QString &query, const QJsonObject &body) const
{
    if (m_token.isEmpty())
        return true;
    const QUrlQuery urlQuery(query);
    const QString queryToken = urlQuery.queryItemValue(QStringLiteral("token"));
    const QString bodyToken = body.value(QStringLiteral("token")).toString();
    return queryToken == m_token || bodyToken == m_token;
}

QJsonObject RemoteControlServer::currentStatus() const
{
    if (m_statusProvider)
        return m_statusProvider();
    return {{QStringLiteral("ok"), true}, {QStringLiteral("message"), QStringLiteral("状态提供者未接入")}};
}