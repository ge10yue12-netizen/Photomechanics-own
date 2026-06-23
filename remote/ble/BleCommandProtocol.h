#pragma once

#include <QByteArray>
#include <QString>

struct BleCommandParseResult
{
    QString cmd;
    bool tokenOk = true;
    bool valid = false;
    QString error;
};

BleCommandParseResult parseBleCommand(const QByteArray &raw, const QString &expectedToken);
QByteArray compactStatusJson(const QByteArray &json);

