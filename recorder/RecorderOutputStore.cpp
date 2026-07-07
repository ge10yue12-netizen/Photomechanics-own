#include "RecorderOutputStore.h"
#include "RecorderPathHelper.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

bool RecorderOutputStore::load(QString *errorMessage)
{
    m_entries.clear();
    const QString path = RecorderPathHelper::historyFilePath();
    QFile file(path);
    if (!file.exists())
        return true;
    if (!file.open(QIODevice::ReadOnly))
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("无法读取输出历史：%1").arg(path);
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();
    if (parseError.error != QJsonParseError::NoError || !doc.isArray())
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("输出历史格式无效。");
        return false;
    }

    const QJsonArray arr = doc.array();
    for (const QJsonValue &val : arr)
    {
        if (!val.isObject())
            continue;
        const QJsonObject obj = val.toObject();
        RecorderOutputEntry e;
        e.filePath = obj.value(QStringLiteral("path")).toString();
        e.sizeBytes = static_cast<qint64>(obj.value(QStringLiteral("sizeBytes")).toDouble());
        e.durationSeconds = obj.value(QStringLiteral("durationSeconds")).toInt();
        e.savedAt = obj.value(QStringLiteral("savedAt")).toString();
        if (!e.filePath.isEmpty())
            m_entries.append(e);
    }
    return true;
}

bool RecorderOutputStore::save(QString *errorMessage) const
{
    RecorderPathHelper::ensureRecordRootExists(nullptr);
    QJsonArray arr;
    for (const RecorderOutputEntry &e : m_entries)
    {
        QJsonObject obj;
        obj.insert(QStringLiteral("path"), e.filePath);
        obj.insert(QStringLiteral("sizeBytes"), static_cast<double>(e.sizeBytes));
        obj.insert(QStringLiteral("durationSeconds"), e.durationSeconds);
        obj.insert(QStringLiteral("savedAt"), e.savedAt);
        arr.append(obj);
    }

    QFile file(RecorderPathHelper::historyFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("无法写入输出历史。");
        return false;
    }
    file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

void RecorderOutputStore::addEntry(const RecorderOutputEntry &entry)
{
    m_entries.prepend(entry);
    while (m_entries.size() > 50)
        m_entries.removeLast();
}

bool RecorderOutputStore::removeEntryAt(int index, QString *errorMessage)
{
    if (index < 0 || index >= m_entries.size())
        return false;

    const QString path = m_entries.at(index).filePath;
    if (QFile::exists(path) && !QFile::remove(path))
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("无法删除文件：%1").arg(path);
        return false;
    }
    m_entries.removeAt(index);
    return true;
}

bool RecorderOutputStore::renameEntryAt(int index, const QString &newPath, QString *errorMessage)
{
    if (index < 0 || index >= m_entries.size())
        return false;

    const QString oldPath = m_entries.at(index).filePath;
    if (oldPath == newPath)
        return true;

    if (QFile::exists(newPath))
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("目标文件已存在。");
        return false;
    }

    if (QFile::exists(oldPath))
    {
        if (!QFile::rename(oldPath, newPath))
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("重命名失败。");
            return false;
        }
    }

    RecorderOutputEntry e = m_entries.at(index);
    e.filePath = newPath;
    m_entries[index] = e;
    return true;
}
