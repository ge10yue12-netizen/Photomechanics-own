#include "RecorderPathHelper.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>

namespace RecorderPathHelper
{

QString recordRootDir()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    if (!appDir.isEmpty())
    {
        const QString candidate = QDir(appDir).filePath(QStringLiteral("Record"));
        QDir dir;
        if (dir.mkpath(candidate))
            return QDir(candidate).absolutePath();
    }

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
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    return QDir(recordRootDir()).filePath(QStringLiteral("record_") + stamp + extensionFor(format));
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

QString historyFilePath()
{
    return QDir(recordRootDir()).filePath(QStringLiteral("output_history.json"));
}

} // namespace RecorderPathHelper
