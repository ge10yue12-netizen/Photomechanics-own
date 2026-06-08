// ImageSaveThread.cpp — 独立线程消费 SaveTask 队列并写 BMP

#include "ImageSaveThread.h"
#include <QDebug>
#include <QElapsedTimer>
#include <QMutexLocker>

ImageSaveThread::ImageSaveThread(QObject *parent)
    : QThread(parent)
{
}

ImageSaveThread::~ImageSaveThread()
{
    requestStopAndWait(5000);
}

void ImageSaveThread::enqueue(const SaveTask &task)
{
    QMutexLocker lock(&m_mutex);
    m_queue.enqueue(task);
    const int sz = m_queue.size();
    lock.unlock();
    if (sz >= kBacklogWarnSize)
        emit queueBacklog(sz);
}

int ImageSaveThread::queueSize() const
{
    QMutexLocker lock(&m_mutex);
    return m_queue.size();
}

// 轮询等待队列排空，用于停止采集/退出前尽量写完已入队图片
void ImageSaveThread::waitUntilEmpty(int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (queueSize() > 0 && timer.elapsed() < timeoutMs)
        msleep(20);
}

void ImageSaveThread::requestStopAndWait(int timeoutMs)
{
    {
        QMutexLocker lock(&m_mutex);
        m_stop = true;
    }
    if (isRunning() && !wait(timeoutMs))
        qWarning("ImageSaveThread: 等待存图线程结束超时 (%d ms)。", timeoutMs);
}

void ImageSaveThread::run()
{
    while (true)
    {
        SaveTask task;
        {
            QMutexLocker lock(&m_mutex);
            if (m_queue.isEmpty())
            {
                if (m_stop)
                    break;
                lock.unlock();
                msleep(5);
                continue;
            }
            task = m_queue.dequeue();
        }

        const bool ok = task.image.save(task.filePath, "BMP");
        const QString err = ok ? QString() : QStringLiteral("写入 BMP 失败");

        // 退出过程中不再向 UI 发 saveFinished，避免关闭窗口后仍更新界面
        bool stopping = false;
        {
            QMutexLocker lock(&m_mutex);
            stopping = m_stop;
        }
        if (!stopping)
            emit saveFinished(task.filePath, ok, err);

        const int sz = queueSize();
        if (sz >= kBacklogWarnSize)
            emit queueBacklog(sz);

        {
            QMutexLocker lock(&m_mutex);
            if (m_stop && m_queue.isEmpty())
                break;
        }
    }
}
