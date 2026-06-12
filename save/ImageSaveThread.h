#pragma once

#include "core/AppTypes.h"
#include <QMutex>
#include <QQueue>
#include <QThread>
#include <QWaitCondition>

// ImageSaveThread：后台 BMP 写盘线程；run() 内 writeBmpFile 直写 24 位未压缩 BMP
class ImageSaveThread : public QThread
{
    Q_OBJECT

public:
    explicit ImageSaveThread(QObject *parent = nullptr);
    ~ImageSaveThread() override;

    // 非阻塞入队；队列已满时返回 false，且不移动 task 所有权
    bool trySubmit(SaveTask &task);

    int queueSize() const;
    int capacity() const;
    void waitUntilEmpty(int timeoutMs = 60000);
    void requestStopAndWait(int timeoutMs = 10000);

signals:
    void saveFinished(const QString &path, bool ok, const QString &errorMsg);
    void queueBacklog(int queueSize); // 队列长度达到积压告警阈值
    void queueFull(int queueSize);     // 队列已达容量上限

protected:
    void run() override;

private:
    static const int kMaxQueueSize = 48;    // 存图队列最大容量（帧数）
    static const int kBacklogWarnSize = 36; // 积压告警阈值（帧数）

    QQueue<SaveTask> m_taskQueue;   // 存图任务队列
    mutable QMutex m_queueMutex;    // 队列互斥锁
    QWaitCondition m_queueNotEmpty; // 队列非空条件，用于唤醒写盘线程
    QWaitCondition m_queueEmpty;    // 队列为空条件，用于 waitUntilEmpty
    bool m_stop = false;            // 写盘线程退出标志
};
