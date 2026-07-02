#include "MobileWebServer.h"
#include "MobileCommands.h"

#include <QDateTime>
#include <QHostAddress>
#include <QJsonDocument>
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>
#include <utility>

namespace
{

const char *kRequestDataProperty = "mobileRequestData"; // 未完整 HTTP 请求的 socket 缓存键

// 规范化路径：非空且以 '/' 开头
QString normalizePath(QString path)
{
    if (path.isEmpty())
        return QStringLiteral("/");
    if (!path.startsWith(QLatin1Char('/')))
        path.prepend(QLatin1Char('/'));
    return path;
}

// 从请求行 URI 拆分 path 与 query
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

// 连接 newConnection 信号
MobileWebServer::MobileWebServer(QObject *parent)
    : QObject(parent)
{
    connect(&m_server, &QTcpServer::newConnection, this, &MobileWebServer::onNewConnection);
}

// 若已在监听则先 stop，再 bind 至任意地址
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

// 关闭 QTcpServer
void MobileWebServer::stop()
{
    m_lastPreviewPullMs = 0;
    m_server.close();
}

// 转发 isListening
bool MobileWebServer::isListening() const
{
    return m_server.isListening();
}

bool MobileWebServer::isPreviewConsumerActive(int idleMs) const
{
    if (m_lastPreviewPullMs <= 0 || idleMs <= 0)
        return false;
    return QDateTime::currentMSecsSinceEpoch() - m_lastPreviewPullMs < idleMs;
}

void MobileWebServer::touchPreviewConsumer()
{
    m_lastPreviewPullMs = QDateTime::currentMSecsSinceEpoch();
}

void MobileWebServer::setRemoteShutdownHandler(std::function<void()> handler)
{
    m_remoteShutdownHandler = std::move(handler);
}

// 保存 token 校验回调
void MobileWebServer::setTokenVerifier(std::function<bool(const QString &)> verifier)
{
    m_tokenVerifier = std::move(verifier);
}

// 保存状态 JSON 提供者
void MobileWebServer::setStatusProvider(std::function<QJsonObject()> provider)
{
    m_statusProvider = std::move(provider);
}

// 保存兼容 JPEG 预览提供者
void MobileWebServer::setPreviewProvider(std::function<QByteArray()> provider)
{
    m_previewProvider = std::move(provider);
}

// 保存 Snapshot 预览提供者
void MobileWebServer::setPreviewSnapshotProvider(std::function<PreviewFrameCache::Snapshot()> provider)
{
    m_previewSnapshotProvider = std::move(provider);
}

// 保存帧序号提供者
void MobileWebServer::setPreviewSeqProvider(std::function<quint64()> provider)
{
    m_previewSeqProvider = std::move(provider);
}

// 保存客户端配置提供者
void MobileWebServer::setConfigProvider(std::function<QJsonObject()> provider)
{
    m_configProvider = std::move(provider);
}

// 缓存 /mobile 页面 HTML
void MobileWebServer::setMobileHtml(const QByteArray &html)
{
    m_mobileHtml = html;
}

// 保存 POST 命令互斥钩子
void MobileWebServer::setCommandGuardHooks(MobileCommandGuardHooks hooks)
{
    m_commandGuardHooks = std::move(hooks);
}

// 循环接受连接并绑定 readyRead、disconnected
void MobileWebServer::onNewConnection()
{
    while (auto *socket = m_server.nextPendingConnection())
    {
        socket->setParent(this);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { onSocketReadyRead(socket); });
        connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
    }
}

// 按 Content-Length 组装请求；Keep-Alive 时保留 leftover 并循环 dispatch
void MobileWebServer::onSocketReadyRead(QTcpSocket *socket)
{
    for (;;)
    {
        QByteArray request = socket->property(kRequestDataProperty).toByteArray();
        request += socket->readAll();

        const int headerEnd = request.indexOf("\r\n\r\n");
        if (headerEnd < 0)
        {
            socket->setProperty(kRequestDataProperty, request);
            return;
        }

        int contentLength = 0;
        bool clientWantsKeepAlive = false;
        const QList<QByteArray> headerLines = request.left(headerEnd).split('\n');
        for (QByteArray line : headerLines)
        {
            line = line.trimmed();
            if (line.toLower().startsWith("content-length:"))
                contentLength = line.mid(line.indexOf(':') + 1).trimmed().toInt();
            if (line.toLower().startsWith("connection:"))
            {
                const QByteArray value = line.mid(line.indexOf(':') + 1).trimmed().toLower();
                clientWantsKeepAlive = value.contains("keep-alive");
            }
        }

        const int totalSize = headerEnd + 4 + contentLength;
        if (request.size() < totalSize)
        {
            // body 未收齐，继续缓存
            socket->setProperty(kRequestDataProperty, request);
            return;
        }

        const QByteArray oneRequest = request.left(totalSize);
        const QByteArray leftover = request.mid(totalSize);
        socket->setProperty(kRequestDataProperty, leftover);

        const bool keepAlive = handleRequest(socket, oneRequest, clientWantsKeepAlive);
        if (!keepAlive || leftover.isEmpty())
            break;
    }
}

// 路由分发：token 校验后处理 HTML、JSON、预览、POST 命令
bool MobileWebServer::handleRequest(QTcpSocket *socket, const QByteArray &request, bool clientWantsKeepAlive)
{
    const int headerEnd = request.indexOf("\r\n\r\n");
    const QByteArray header = request.left(headerEnd);
    const QList<QByteArray> lines = header.split('\n');
    if (lines.isEmpty())
    {
        writeJson(socket, {{QStringLiteral("ok"), false}, {QStringLiteral("message"), QStringLiteral("请求格式错误")}},
                  400, QByteArrayLiteral("Bad Request"));
        return false;
    }

    const QList<QByteArray> firstLine = lines.first().trimmed().split(' ');
    if (firstLine.size() < 2)
    {
        writeJson(socket, {{QStringLiteral("ok"), false}, {QStringLiteral("message"), QStringLiteral("请求行错误")}},
                  400, QByteArrayLiteral("Bad Request"));
        return false;
    }

    const QString method = QString::fromLatin1(firstLine.at(0)).toUpper();
    const QString target = QString::fromUtf8(firstLine.at(1));
    QString path;
    QString query;
    parseRequestTarget(target, &path, &query);

    if (path == QStringLiteral("/favicon.ico"))
    {
        writeResponse(socket, 204, QByteArrayLiteral("No Content"), QByteArrayLiteral("text/plain"), QByteArray());
        return false;
    }

    if (!verifyToken(query))
    {
        writeJson(socket, {{QStringLiteral("ok"), false}, {QStringLiteral("message"), QStringLiteral("token 无效或已过期")}},
                  403, QByteArrayLiteral("Forbidden"));
        return false;
    }

    emit clientAccessed(path);

    if (method == QStringLiteral("GET") && path == QStringLiteral("/mobile"))
    {
        if (m_mobileHtml.isEmpty())
        {
            writeJson(socket, {{QStringLiteral("ok"), false}, {QStringLiteral("message"), QStringLiteral("mobile.html 未加载")}},
                      500, QByteArrayLiteral("Internal Server Error"));
            return false;
        }
        writeResponse(socket, 200, QByteArrayLiteral("OK"),
                      QByteArrayLiteral("text/html; charset=utf-8"), m_mobileHtml);
        return false;
    }

    if (method == QStringLiteral("GET") && path == QStringLiteral("/api/config"))
    {
        QJsonObject cfg = m_configProvider ? m_configProvider() : QJsonObject();
        cfg.insert(QStringLiteral("ok"), true);
        writeJson(socket, cfg);
        return false;
    }

    if (method == QStringLiteral("GET") && path == QStringLiteral("/api/status"))
    {
        const QJsonObject status = m_commandGuardHooks.isActive()
            ? m_commandGuardHooks.augmentStatus(currentStatus())
            : currentStatus();
        writeJson(socket, status);
        return false;
    }

    if (method == QStringLiteral("GET") && path == QStringLiteral("/api/preview/seq"))
    {
        touchPreviewConsumer();
        const quint64 seq = m_previewSeqProvider ? m_previewSeqProvider() : 0;
        writeJson(socket, {{QStringLiteral("ok"), true}, {QStringLiteral("seq"), static_cast<qint64>(seq)}});
        return false;
    }

    if ((method == QStringLiteral("GET") || method == QStringLiteral("HEAD"))
        && path == QStringLiteral("/api/preview.jpg"))
    {
        touchPreviewConsumer();
        PreviewFrameCache::Snapshot snap;
        if (m_previewSnapshotProvider)
            snap = m_previewSnapshotProvider();
        else if (m_previewProvider)
            snap.jpeg = m_previewProvider();
        else if (m_previewSeqProvider)
            snap.frameSeq = m_previewSeqProvider();

        const QByteArray seqHeader =
            QByteArray("X-Frame-Seq: ") + QByteArray::number(snap.frameSeq) + "\r\n";
        const bool keepAlive = clientWantsKeepAlive;
        if (method == QStringLiteral("HEAD"))
        {
            writeResponse(socket, 200, QByteArrayLiteral("OK"),
                          QByteArrayLiteral("image/jpeg"), QByteArray(), seqHeader, !keepAlive);
            return keepAlive;
        }
        writeResponse(socket, 200, QByteArrayLiteral("OK"),
                      QByteArrayLiteral("image/jpeg"), snap.jpeg, seqHeader, !keepAlive);
        return keepAlive; // 预览拉流可复用 TCP
    }

    if (method == QStringLiteral("POST") && path == MobileCommands::remoteOffApiPath())
    {
        if (m_remoteShutdownHandler)
            m_remoteShutdownHandler();
        writeJson(socket, {{QStringLiteral("ok"), true},
                           {QStringLiteral("message"), QStringLiteral("远程控制已关闭")}});
        return false;
    }

    if (method == QStringLiteral("POST") && path == QStringLiteral("/api/release"))
    {
        if (m_commandGuardHooks.isActive())
            m_commandGuardHooks.release();
        const QJsonObject status = m_commandGuardHooks.isActive()
            ? m_commandGuardHooks.augmentStatus(currentStatus())
            : currentStatus();
        writeJson(socket, status);
        return false;
    }

    if (method == QStringLiteral("POST"))
    {
        const QString cmd = MobileCommands::commandForApiPath(path);
        if (!cmd.isEmpty())
        {
            if (m_commandGuardHooks.isActive())
            {
                const MobileCommandGuardHooks::Decision access = m_commandGuardHooks.tryCommand();
                if (access.blocked)
                {
                    writeJson(socket, {{QStringLiteral("ok"), false}, {QStringLiteral("message"), access.message}}, 409,
                              QByteArrayLiteral("Conflict"));
                    return false;
                }
            }
            replyCommand(socket, cmd);
            return false;
        }
    }

    writeJson(socket, {{QStringLiteral("ok"), false}, {QStringLiteral("message"), QStringLiteral("接口不存在")}},
              404, QByteArrayLiteral("Not Found"));
    return false;
}

// 发出 commandReceived 并返回带 ok 的状态 JSON
void MobileWebServer::replyCommand(QTcpSocket *socket, const QString &cmd)
{
    emit commandReceived(cmd);
    QJsonObject response = m_commandGuardHooks.isActive()
        ? m_commandGuardHooks.augmentStatus(currentStatus())
        : currentStatus();
    response.insert(QStringLiteral("ok"), true);
    writeJson(socket, response);
}

// JSON 紧凑序列化后写入 HTTP 响应
void MobileWebServer::writeJson(QTcpSocket *socket, const QJsonObject &obj, int statusCode, const QByteArray &statusText) const
{
    const QByteArray body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    writeResponse(socket, statusCode, statusText, QByteArrayLiteral("application/json; charset=utf-8"), body);
}

// 组装 HTTP/1.1 头与 body；closeConnection 为 false 时保持连接
void MobileWebServer::writeResponse(QTcpSocket *socket,
                                    int statusCode,
                                    const QByteArray &statusText,
                                    const QByteArray &contentType,
                                    const QByteArray &body,
                                    const QByteArray &extraHeaders,
                                    bool closeConnection) const
{
    QByteArray response;
    response += "HTTP/1.1 " + QByteArray::number(statusCode) + " " + statusText + "\r\n";
    response += "Content-Type: " + contentType + "\r\n";
    response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    if (!extraHeaders.isEmpty())
        response += extraHeaders;
    if (closeConnection)
        response += "Connection: close\r\n";
    else
        response += "Connection: keep-alive\r\nKeep-Alive: timeout=30, max=1000\r\n";
    response += "Cache-Control: no-store\r\n";
    response += "\r\n";
    response += body;
    socket->write(response);
    if (closeConnection)
        socket->disconnectFromHost();
}

// 从 query 解析 token 并调用校验回调
bool MobileWebServer::verifyToken(const QString &query) const
{
    if (!m_tokenVerifier)
        return false;
    const QUrlQuery urlQuery(query);
    return m_tokenVerifier(urlQuery.queryItemValue(QStringLiteral("token")));
}

// 获取当前状态 JSON
QJsonObject MobileWebServer::currentStatus() const
{
    if (m_statusProvider)
        return m_statusProvider();
    return {{QStringLiteral("ok"), true}, {QStringLiteral("message"), QStringLiteral("状态提供者未接入")}};
}
