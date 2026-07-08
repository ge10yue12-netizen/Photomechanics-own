#pragma once

#include <QList>
#include <QString>

// 单次录制输出条目（扫描保存目录生成）。
struct RecorderOutputEntry
{
    QString filePath;
    qint64 sizeBytes = 0;
    int durationSeconds = 0;
    QString savedAt;
};

// 录制输出：列表 = 保存目录内视频；时长从容器读取，无 sidecar JSON。
class RecorderOutputStore
{
public:
    void setOutputDirectory(const QString &absoluteDir);

    bool load(QString *errorMessage);
    bool rebuildFromDirectory(QString *errorMessage);

    const QList<RecorderOutputEntry> &entries() const { return m_entries; }

    bool removeEntryAt(int index, QString *errorMessage);
    bool renameEntryAt(int index, const QString &newPath, QString *errorMessage);

private:
    static QString pathKey(const QString &absolutePath);

    QString m_outputDirectory;
    QList<RecorderOutputEntry> m_entries;
};
