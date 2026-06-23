#pragma once

#include <QHostAddress>
#include <QString>
#include <QStringList>

class QTcpServer;

struct HttpNetConfig
{
    quint16 port = 18765;
    QString token = QStringLiteral("1234");
    QHostAddress bindAddress = QHostAddress::Any;
};

class NetConfigHelper
{
public:
    static QString configDirPath();
    static QString configFilePath();
    static QString loadSharedToken();
    static bool ensureDefaultConfigFile();
    static HttpNetConfig loadHttpConfig();
    static bool isLocalOnlyBind(const QHostAddress &bindAddress);
    static QString bindAddressText(const QHostAddress &bindAddress);
    static QString preferredLanIpv4();
    static QStringList listLanIpv4();
    static QString httpEndpointHint(quint16 port);
    static bool tryListen(QTcpServer &server,
                          const QHostAddress &bindAddress,
                          quint16 port,
                          QString *lastError = nullptr);

private:
    static QHostAddress parseBindAddress(const QString &bind);
};