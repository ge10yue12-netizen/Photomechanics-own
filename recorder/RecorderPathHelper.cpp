#include "RecorderPathHelper.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace RecorderPathHelper
{
namespace
{

QString s_recordRootOverride;

QString projectRootDir()
{
    QDir root(QCoreApplication::applicationDirPath());
    root.cdUp();
    root.cdUp();
    return root.absolutePath();
}

} // namespace

QString recordRootDir()
{
    if (!s_recordRootOverride.isEmpty())
        return QDir(s_recordRootOverride).absolutePath();

    const QString candidate = QDir(projectRootDir()).filePath(QStringLiteral("Record"));
    QDir dir;
    if (dir.mkpath(candidate))
        return QDir(candidate).absolutePath();

    const QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString fallback = QDir(docs).filePath(QStringLiteral("Record"));
    QDir().mkpath(fallback);
    return QDir(fallback).absolutePath();
}

QString extensionFor(recorder::VideoFormat format)
{
    switch (format)
    {
    case recorder::VideoFormat::Avi:
        return QStringLiteral(".avi");
    case recorder::VideoFormat::Mp4:
    default:
        return QStringLiteral(".mp4");
    }
}

QString defaultOutputFile(recorder::VideoFormat format)
{
    return uniqueOutputFileInDir(recordRootDir(), format);
}

QString normalizeOutputDirectory(const QString &pathOrDir)
{
    const QString trimmed = pathOrDir.trimmed();
    if (trimmed.isEmpty())
        return recordRootDir();

    QFileInfo fi(trimmed);
    if (fi.exists() && fi.isFile())
        return fi.absolutePath();

    const QString lower = trimmed.toLower();
    if (lower.endsWith(QStringLiteral(".mp4")) || lower.endsWith(QStringLiteral(".avi")))
        return QFileInfo(trimmed).absolutePath();

    return QDir(trimmed).absolutePath();
}

QString uniqueOutputFileInDir(const QString &outputDirectory, recorder::VideoFormat format)
{
    const QString dir = normalizeOutputDirectory(outputDirectory);
    QDir().mkpath(dir);

    const QString stamp =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
    const QString ext = extensionFor(format);
    QString base = QDir(dir).filePath(QStringLiteral("record_") + stamp);
    QString candidate = base + ext;

    int suffix = 1;
    while (QFileInfo::exists(candidate))
    {
        candidate = base + QStringLiteral("_%1").arg(suffix++) + ext;
    }
    return QDir(candidate).absolutePath();
}

bool ensureRecordRootExists(QString *errorMessage)
{
    const QString root = recordRootDir();
    if (QDir(root).exists())
        return true;
    if (QDir().mkpath(root))
        return true;
    if (errorMessage)
        *errorMessage = QStringLiteral("无法创建录制目录：%1").arg(root);
    return false;
}

QString legacyMetadataDirectory()
{
    return QDir(projectRootDir()).filePath(QStringLiteral("Log"));
}

void setRecordRootDirOverride(const QString &absoluteDir)
{
    s_recordRootOverride = absoluteDir.trimmed();
}

void clearRecordRootDirOverride()
{
    s_recordRootOverride.clear();
}

} // namespace RecorderPathHelper
