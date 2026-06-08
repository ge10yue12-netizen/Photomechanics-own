#pragma once

#include "core/AppTypes.h"
#include <QThread>
#include <QQueue>
#include <QMutex>

// ImageSaveThread — 后台线程顺序写 BMP，避免磁盘 IO 阻塞 UI 与抓图
class ImageSaveThread : public QThread
{
    Q_OBJECT

public:
    explicit ImageSaveThread(QObject *parent = nullptr);
    ~ImageSaveThread() override;

    void enqueue(const SaveTask &task);       // 主线程入队（线程安全）
    int queueSize() const;
    void waitUntilEmpty(int timeoutMs = 30000); // 阻塞直到队列清空或超时
    void requestStopAndWait(int timeoutMs = 5000);

signals:
    void saveFinished(const QString &path, bool ok, const QString &errorMsg);
    void queueBacklog(int queueSize); // 队列长度 ≥ kBacklogWarnSize 时告警

protected:
    void run() override;

private:
    static const int kBacklogWarnSize = 15; // 积压阈值，超过则 emit queueBacklog
    mutable QMutex m_mutex;
    QQueue<SaveTask> m_queue;
    bool m_stop = false; // 为 true 且队列空时 run() 退出
};
