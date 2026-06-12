// AppLogger.cpp：运行日志目录、文件创建与按行写入

#include "AppLogger.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>

QString AppLogger::defaultLogDirectory()
{
    // 与 SavePathHelper::defaultRootPath 相同规则：exe 上两级为项目根
    QDir projDir(QCoreApplication::applicationDirPath());
    projDir.cdUp();
    projDir.cdUp();
    return projDir.absoluteFilePath(QStringLiteral("Log"));
}

bool AppLogger::openRunLog(const QString &logDir)
{
    if (m_file.isOpen())
        return true;

    const QString logDirPath = logDir.trimmed().isEmpty() ? defaultLogDirectory() : logDir.trimmed();
    if (!QDir().mkpath(logDirPath))
        return false;

    const QString fileName = QStringLiteral("run_%1.log")
                                 .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    m_file.setFileName(QDir(logDirPath).filePath(fileName));
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return false;

    // 会话首行记录日志文件完整路径，供离线分析使用
    const QString header = formatLine(QStringLiteral("日志文件: %1").arg(m_file.fileName()));
    m_file.write(header.toUtf8());
    m_file.write("\n");
    m_file.flush();
    return true;
}

void AppLogger::closeRunLog(const QString &footer)
{
    if (!m_file.isOpen())
        return;

    if (!footer.trimmed().isEmpty())
        log(footer.trimmed());

    m_file.flush();
    m_file.close();
}

QString AppLogger::formatLine(const QString &msg) const
{
    return QStringLiteral("[%1] %2")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss")), msg);
}

void AppLogger::writeLine(const QString &formattedLine)
{
    if (!m_file.isOpen())
        return;

    m_file.write(formattedLine.toUtf8());
    m_file.write("\n");
    m_file.flush();
}

void AppLogger::log(const QString &msg)
{
    writeLine(formatLine(msg));
}
