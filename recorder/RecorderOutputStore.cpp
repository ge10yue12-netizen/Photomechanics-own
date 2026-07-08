#include "RecorderOutputStore.h"
#include "RecorderPathHelper.h"
#include "RecorderVideoProbe.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <algorithm>

namespace
{

bool isRecordedVideoFileName(const QString &fileName)
{
    const QString lower = fileName.toLower();
    return lower.endsWith(QStringLiteral(".mp4")) || lower.endsWith(QStringLiteral(".avi"));
}

QString normalizedAbsolutePath(const QString &path)
{
    const QFileInfo fi(path);
    if (!fi.exists())
        return QDir::cleanPath(fi.absoluteFilePath());
    const QString canonical = fi.canonicalFilePath();
    return canonical.isEmpty() ? QDir::cleanPath(fi.absoluteFilePath()) : canonical;
}

void removeLegacySidecarJson(const QString &outputDirectory)
{
    const QStringList legacyPaths = {
        QDir(outputDirectory).filePath(QStringLiteral("output_history.json")),
        QDir(RecorderPathHelper::recordRootDir()).filePath(QStringLiteral("output_history.json")),
    };
    for (const QString &path : legacyPaths)
        QFile::remove(path);

    QDir logDir(RecorderPathHelper::legacyMetadataDirectory());
    QFile::remove(logDir.filePath(QStringLiteral("recorder_output_history.json")));
    QFile::remove(logDir.filePath(QStringLiteral("output_history.json")));
}

} // namespace

QString RecorderOutputStore::pathKey(const QString &absolutePath)
{
    return QDir::fromNativeSeparators(normalizedAbsolutePath(absolutePath)).toLower();
}

void RecorderOutputStore::setOutputDirectory(const QString &absoluteDir)
{
    const QString normalized = RecorderPathHelper::normalizeOutputDirectory(absoluteDir);
    m_outputDirectory = normalized.isEmpty() ? RecorderPathHelper::recordRootDir() : normalized;
}

bool RecorderOutputStore::load(QString *errorMessage)
{
    removeLegacySidecarJson(m_outputDirectory.isEmpty() ? RecorderPathHelper::recordRootDir()
                                                        : m_outputDirectory);
    return rebuildFromDirectory(errorMessage);
}

bool RecorderOutputStore::rebuildFromDirectory(QString *errorMessage)
{
    Q_UNUSED(errorMessage);

    m_entries.clear();
    if (m_outputDirectory.isEmpty())
        setOutputDirectory(QString());

    RecorderPathHelper::ensureRecordRootExists(nullptr);
    QDir outputDir(m_outputDirectory);
    if (!outputDir.exists())
        QDir().mkpath(m_outputDirectory);

    const QFileInfoList files =
        outputDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Time);

    QList<RecorderOutputEntry> rebuilt;
    rebuilt.reserve(files.size());
    QSet<QString> seenKeys;

    for (const QFileInfo &fi : files)
    {
        if (!isRecordedVideoFileName(fi.fileName()))
            continue;

        const QString absolutePath = normalizedAbsolutePath(fi.absoluteFilePath());
        const QString key = pathKey(absolutePath);
        if (key.isEmpty() || seenKeys.contains(key))
            continue;
        seenKeys.insert(key);

        RecorderOutputEntry entry;
        entry.filePath = absolutePath;
        entry.sizeBytes = fi.size();
        entry.savedAt = fi.lastModified().toString(Qt::ISODate);
        entry.durationSeconds = RecorderVideoProbe::durationSeconds(absolutePath);

        rebuilt.append(entry);
    }

    std::sort(rebuilt.begin(), rebuilt.end(), [](const RecorderOutputEntry &a, const RecorderOutputEntry &b) {
        const QDateTime ta = QDateTime::fromString(a.savedAt, Qt::ISODate);
        const QDateTime tb = QDateTime::fromString(b.savedAt, Qt::ISODate);
        if (ta.isValid() && tb.isValid())
            return ta < tb;
        return a.filePath < b.filePath;
    });

    m_entries = rebuilt;
    return true;
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

    return rebuildFromDirectory(nullptr);
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

    return rebuildFromDirectory(nullptr);
}
