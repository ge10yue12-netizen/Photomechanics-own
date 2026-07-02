#include "MobileWebServer.h"
#include "MobileCommands.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QDateTime>
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>
#include <utility>

namespace
{

const char *kRequestDataProperty = "mobileRequestData";

// 规范化 HTTP 路径，保证以 / 开头。
QString normalizePath(QString path)
{
    if (path.isEmpty())
        return QStringLiteral("/");
    if (!path.startsWith(QLatin1Char('/')))
        path.prepend(QLatin1Char('/'));
    return path;
}

// 从请求行 target 解析路径与查询串。
void parseRequestTarget(const QString &target, QString *outPath, QString *outQuery)
{
    if (target.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)
        || target.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive))
    {
        const QUrl url(target);
        if (url.isValid())
        {
            *outPath = normalizePath(url.path());
            *outQuery = url.query(QUrl::FullyEncoded);
            return;
        }
    }
    const int queryPos = target.indexOf(QLatin1Char('?'));
    *outPath = normalizePath(queryPos >= 0 ? target.left(queryPos) : target);
    *outQuery = queryPos >= 0 ? target.mid(queryPos + 1) : QString();
}

} // namespace

// 构造：连接 newConnection 至连接处理槽。
MobileWebServer::MobileWebServer(QObject *parent)
    : QObject(parent)
{
    connect(&m_server, &QTcpServer::newConnection, this, &MobileWebServer::onNewConnection);
}

// 在指定端口绑定 QHostAddress::Any 监听。
bool MobileWebServer::start(quint16 port)
{
    if (m_server.isListening())
        stop();
    m_lastError.clear();

    if (m_server.listen(QHostAddress::Any, port))
        return true;

    m_lastError = QStringLiteral("%1（port=%2）").arg(m_server.errorString()).arg(port);
    m_server.close();
    return false;
}

// 关闭 TCP 监听。
void MobileWebServer::stop()
{
    m_server.close();
}

// 返回 m_server 是否正在监听。
bool MobileWebServer::isListening() const
{
    return m_server.isListening();
}

// 注册 token 校验函数。
void MobileWebServer::setTokenVerifier(std::function<bool(const QString &)> verifier)
{
    m_tokenVerifier = std::move(verifier);
}

// 注册状态 JSON 提供函数。
void MobileWebServer::setStatusProvider(std::function<QJsonObject()> provider)
{
    m_statusProvider = std::move(provider);
}

// 注册预览 JPEG 提供函数。
void MobileWebServer::setPreviewProvider(std::function<QByteArray()> provider)
{
    m_previewProvider = std::move(provider);
}

// 注册客户端配置 JSON 提供函数。
void MobileWebServer::setConfigProvider(std::function<QJsonObject()> provider)
{
    m_configProvider = std::move(provider);
}

// 设置 GET /mobile 返回的 HTML 内容。
void MobileWebServer::setMobileHtml(const QByteArray &html)
{
    m_mobileHtml = html;
}

// 注册命令互斥 guard 与本服务来源标识。
void MobileWebServer::setControlGuard(RemoteControlGuard *guard, RemoteControlSource source)
{
    m_controlGuard = guard;
    m_controlSource = source;
}

// 接受 pending 连接并为每个 socket 绑定 readyRead。
void MobileWebServer::onNewConnection()
{
    while (auto *socket = m_server.nextPendingConnection())
    {
        socket->setParent(this);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { onSocketReadyRead(socket); });
        connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
    }
}

// 按 Content-Length 累积完整 HTTP 请求后调用 handleRequest。
void MobileWebServer::onSocketReadyRead(QTcpSocket *socket)
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
    socket->setProperty(kRequestDataProperty, QByteArray());
}

// 解析请求行，校验 token 并按路径分发 API。
void MobileWebServer::handleRequest(QTcpSocket *socket, const QByteArray &request)
{
    const int headerEnd = request.indexOf("\r\n\r\n");
    const QByteArray header = request.left(headerEnd);
    const QList<QByteArray> lines = header.split('\n');
    if (lines.isEmpty())
    {
        writeJson(socket, {{QStringLiteral("ok"), false}, {QStringLiteral("message"), QStringLiteral("请求格式错误")}},
                  400, QByteArrayLiteral("Bad Request"));
        return;
    }

    const QList<QByteArray> firstLine = lines.first().trimmed().split(' ');
    if (firstLine.size() < 2)
    {
        writeJson(socket, {{QStringLiteral("ok"), false}, {QStringLiteral("message"), QStringLiteral("请求行错误")}},
                  400, QByteArrayLiteral("Bad Request"));
        return;
    }

    const QString method = QString::fromLatin1(firstLine.at(0)).toUpper();
    const QString target = QString::fromUtf8(firstLine.at(1));
    QString path;
    QString query;
    parseRequestTarget(target, &path, &query);

    if (path == QStringLiteral("/favicon.ico"))
    {
        writeResponse(socket, 204, QByteArrayLiteral("No Content"), QByteArrayLiteral("text/plain"), QByteArray());
        return;
    }

    if (!verifyToken(query))
    {
        writeJson(socket, {{QStringLiteral("ok"), false}, {QStringLiteral("message"), QStringLiteral("token 无效或已过期")}},
                  403, QByteArrayLiteral("Forbidden"));
        return;
    }

    emit clientAccessed(path);

    if (method == QStringLiteral("GET") && path == QStringLiteral("/mobile"))
    {
        if (m_mobileHtml.isEmpty())
        {
            writeJson(socket, {{QStringLiteral("ok"), false}, {QStringLiteral("message"), QStringLiteral("mobile.html 未加载")}},
                      500, QByteArrayLiteral("Internal Server Error"));
            return;
        }
        writeResponse(socket, 200, QByteArrayLiteral("OK"),
                      QByteArrayLiteral("text/html; charset=utf-8"), m_mobileHtml);
        return;
    }

    if (method == QStringLiteral("GET") && path == QStringLiteral("/api/config"))
    {
        QJsonObject cfg = m_configProvider ? m_configProvider() : QJsonObject();
        cfg.insert(QStringLiteral("ok"), true);
        writeJson(socket, cfg);
        return;
    }

    if (method == QStringLiteral("GET") && path == QStringLiteral("/api/status"))
    {
        writeJson(socket, RemoteControlGuard::statusWithGuard(m_controlGuard, m_controlSource, currentStatus()));
        return;
    }

    if (method == QStringLiteral("GET") && path == QStringLiteral("/api/preview.jpg"))
    {
        m_lastPreviewRequestMs = QDateTime::currentMSecsSinceEpoch();
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

    if (method == QStringLiteral("POST"))
    {
        const QString cmd = MobileCommands::commandForApiPath(path);
        if (!cmd.isEmpty())
        {
            const RemoteControlGuard::Decision access =
                RemoteControlGuard::tryCommand(m_controlGuard, m_controlSource);
            if (access.blocked)
            {
                writeJson(socket, {{QStringLiteral("ok"), false}, {QStringLiteral("message"), access.message}}, 409,
                          QByteArrayLiteral("Conflict"));
                return;
            }
            replyCommand(socket, cmd);
            return;
        }
    }

    writeJson(socket, {{QStringLiteral("ok"), false}, {QStringLiteral("message"), QStringLiteral("接口不存在")}},
              404, QByteArrayLiteral("Not Found"));
}

// 发出 commandReceived 并写回带占用信息的状态 JSON。
void MobileWebServer::replyCommand(QTcpSocket *socket, const QString &cmd)
{
    emit commandReceived(cmd);
    QJsonObject response = RemoteControlGuard::statusWithGuard(m_controlGuard, m_controlSource, currentStatus());
    response.insert(QStringLiteral("ok"), true);
    writeJson(socket, response);
}

// 将 JSON 对象序列化并写入 HTTP 响应。
void MobileWebServer::writeJson(QTcpSocket *socket, const QJsonObject &obj, int statusCode, const QByteArray &statusText) const
{
    const QByteArray body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    writeResponse(socket, statusCode, statusText, QByteArrayLiteral("application/json; charset=utf-8"), body);
}

// 组装 HTTP/1.1 响应头与正文并关闭连接。
void MobileWebServer::writeResponse(QTcpSocket *socket,
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

// 校验 query 中的 token；未注册 verifier 时拒绝。
bool MobileWebServer::verifyToken(const QString &query) const
{
    if (!m_tokenVerifier)
        return false;
    const QUrlQuery urlQuery(query);
    return m_tokenVerifier(urlQuery.queryItemValue(QStringLiteral("token")));
}

// 调用 statusProvider；未注册时返回占位 JSON。
QJsonObject MobileWebServer::currentStatus() const
{
    if (m_statusProvider)
        return m_statusProvider();
    return {{QStringLiteral("ok"), true}, {QStringLiteral("message"), QStringLiteral("状态提供者未接入")}};
}

// 返回最近一次 preview 请求时间戳。
qint64 MobileWebServer::lastPreviewRequestMs() const
{
    return m_lastPreviewRequestMs;
}

// 判断 nowMs 向前 ttlMs 内是否存在 preview 请求。
bool MobileWebServer::hadRecentPreviewRequest(qint64 nowMs, int ttlMs) const
{
    if (m_lastPreviewRequestMs <= 0 || ttlMs <= 0)
        return false;
    return nowMs - m_lastPreviewRequestMs <= static_cast<qint64>(ttlMs);
}

