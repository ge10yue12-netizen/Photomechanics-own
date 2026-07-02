#include "BleCommandProtocol.h"

#include "../RemoteCommands.h"

#include <QJsonDocument>
#include <QJsonObject>

// 解析格式：cmd 或 cmd:token；token 非空且与 expectedToken 不一致时 tokenOk=false。
BleCommandParseResult parseBleCommand(const QByteArray &raw, const QString &expectedToken)
{
    BleCommandParseResult result;
    const QString text = QString::fromUtf8(raw).trimmed();
    if (text.isEmpty())
    {
        result.error = QStringLiteral("命令为空");
        return result;
    }

    QString cmd = text;
    QString token;
    const int colon = text.indexOf(QLatin1Char(':'));
    if (colon > 0)
    {
        cmd = text.left(colon).trimmed();
        token = text.mid(colon + 1).trimmed();
    }

    if (!isKnownRemoteCommand(cmd))
    {
        result.error = QStringLiteral("未知命令");
        return result;
    }

    if (!expectedToken.isEmpty() && token != expectedToken)
    {
        result.tokenOk = false;
        result.error = QStringLiteral("认证 token 无效");
        return result;
    }

    result.cmd = cmd;
    result.valid = true;
    return result;
}

// 紧凑 JSON 字段映射：cameraOpen→cam, liveViewActive→lv, remotePreviewActive→rp,
// acquisitionActive→grab, calculateActive→calc, stageRunning→stg, remoteEnabled→re, ...
QByteArray compactStatusJson(const QByteArray &json)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject())
        return json;

    const QJsonObject in = doc.object();
    QJsonObject out;
    out.insert(QStringLiteral("ok"), in.value(QStringLiteral("ok")).toBool(true) ? 1 : 0);
    out.insert(QStringLiteral("cam"), in.value(QStringLiteral("cameraOpen")).toBool() ? 1 : 0);
    out.insert(QStringLiteral("lv"), in.value(QStringLiteral("liveViewActive")).toBool() ? 1 : 0);
    out.insert(QStringLiteral("rp"), in.value(QStringLiteral("remotePreviewActive")).toBool() ? 1 : 0);
    out.insert(QStringLiteral("grab"), in.value(QStringLiteral("acquisitionActive")).toBool() ? 1 : 0);
    out.insert(QStringLiteral("calc"), in.value(QStringLiteral("calculateActive")).toBool() ? 1 : 0);
    out.insert(QStringLiteral("stg"), in.value(QStringLiteral("stageRunning")).toBool() ? 1 : 0);
    out.insert(QStringLiteral("re"), in.value(QStringLiteral("remoteEnabled")).toBool() ? 1 : 0);
    out.insert(QStringLiteral("q"), in.value(QStringLiteral("queueSize")).toInt());
    out.insert(QStringLiteral("qc"), in.value(QStringLiteral("queueCapacity")).toInt());
    out.insert(QStringLiteral("tot"), in.value(QStringLiteral("totalSaved")).toInt());
    out.insert(QStringLiteral("msg"), in.value(QStringLiteral("message")).toString());
    return QJsonDocument(out).toJson(QJsonDocument::Compact);
}