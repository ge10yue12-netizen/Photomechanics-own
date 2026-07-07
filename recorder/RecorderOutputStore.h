#pragma once

#include <QStringList>
#include <QList>

// 单次录制输出条目。
struct RecorderOutputEntry
{
    QString filePath;
    qint64 sizeBytes = 0;
    int durationSeconds = 0;
    QString savedAt;
};

// 输出历史持久化（Record/output_history.json）。
class RecorderOutputStore
{
public:
    bool load(QString *errorMessage);
    bool save(QString *errorMessage) const;

    const QList<RecorderOutputEntry> &entries() const { return m_entries; }

    void addEntry(const RecorderOutputEntry &entry);
    bool removeEntryAt(int index, QString *errorMessage);
    bool renameEntryAt(int index, const QString &newPath, QString *errorMessage);

private:
    QList<RecorderOutputEntry> m_entries;
};
