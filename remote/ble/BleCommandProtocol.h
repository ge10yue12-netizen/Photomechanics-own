#pragma once

#include <QByteArray>
#include <QString>

// BLE 命令解析结果：cmd 字段、token 校验与错误原因。
struct BleCommandParseResult
{
    QString cmd;
    bool tokenOk = true;
    bool valid = false;
    QString error;
};

// 解析 BLE 写入的原始字节：格式为 cmd 或 cmd:token，并校验白名单与 token。
BleCommandParseResult parseBleCommand(const QByteArray &raw, const QString &expectedToken);

// 将完整状态 JSON 压缩为短键名，减小 Notify 载荷（适配 BLE MTU）。
QByteArray compactStatusJson(const QByteArray &json);