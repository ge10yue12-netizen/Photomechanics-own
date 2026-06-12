#pragma once

#include <QFile>
#include <QString>

// AppLogger：运行日志写入磁盘（项目 Log/run_yyyyMMdd_HHmmss.log），与 UI 解耦
class AppLogger
{
public:
    static QString defaultLogDirectory(); // 可执行文件上两级目录下的 Log

    bool openRunLog(const QString &logDir = QString()); // 空则使用 defaultLogDirectory()
    void closeRunLog(const QString &footer = QStringLiteral("软件退出。"));
    bool isOpen() const { return m_file.isOpen(); }
    QString logFilePath() const { return m_file.fileName(); }

    QString formatLine(const QString &msg) const; // [yyyy-MM-dd hh:mm:ss] msg
    void writeLine(const QString &formattedLine); // 写一行并 flush（文件未打开时忽略）
    void log(const QString &msg);                 // formatLine + writeLine

private:
    QFile m_file; // 本次运行对应的 run_*.log
};
