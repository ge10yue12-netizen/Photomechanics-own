// ImageSaveThread.cpp：有界存图任务队列与 BMP 写盘线程实现

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
    requestStopAndWait(10000);
}

// 将任务放入存图队列；队列满时触发 queueFull 并返回 false
bool ImageSaveThread::trySubmit(SaveTask &task)
{
    QMutexLocker lock(&m_queueMutex);
    const int szBefore = m_taskQueue.size();
    if (m_taskQueue.size() >= kMaxQueueSize)
    {
        lock.unlock();
        emit queueFull(szBefore);
        return false;
    }
    m_taskQueue.enqueue(std::move(task));
    m_queueNotEmpty.wakeOne();
    lock.unlock();

    const int sz = queueSize();
    if (sz >= kBacklogWarnSize)
        emit queueBacklog(sz);
    return true;
}

int ImageSaveThread::queueSize() const
{
    QMutexLocker lock(&m_queueMutex);
    return m_taskQueue.size();
}

int ImageSaveThread::capacity() const
{
    return kMaxQueueSize;
}

// 阻塞等待队列排空或超时
void ImageSaveThread::waitUntilEmpty(int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    QMutexLocker lock(&m_queueMutex);
    while (!m_taskQueue.isEmpty())
    {
        const int remaining = timeoutMs - static_cast<int>(timer.elapsed());
        if (remaining <= 0)
            break;
        m_queueEmpty.wait(&m_queueMutex, remaining);
    }
}

void ImageSaveThread::requestStopAndWait(int timeoutMs)
{
    {
        QMutexLocker lock(&m_queueMutex);
        m_stop = true;
        m_queueNotEmpty.wakeAll();
        m_queueEmpty.wakeAll();
    }
    if (isRunning() && !wait(timeoutMs))
        qWarning("ImageSaveThread: 等待存图线程结束超时 (%d ms)。", timeoutMs);
}

// 写盘线程主循环：出队、保存 BMP、上报结果
void ImageSaveThread::run()
{
    while (true)
    {
        SaveTask task;
        {
            QMutexLocker lock(&m_queueMutex);
            while (m_taskQueue.isEmpty() && !m_stop)
                m_queueNotEmpty.wait(&m_queueMutex);

            if (m_taskQueue.isEmpty())
                break;

            task = m_taskQueue.dequeue();
            if (m_taskQueue.isEmpty())
                m_queueEmpty.wakeAll();
        }

        const bool ok = task.image.save(task.filePath, "BMP");
        const QString err = ok ? QString() : QStringLiteral("写入 BMP 失败");

        bool stopping = false;
        {
            QMutexLocker lock(&m_queueMutex);
            stopping = m_stop;
        }
        // 退出过程中不再通知 UI，避免窗口关闭后仍更新界面
        if (!stopping)
            emit saveFinished(task.filePath, ok, err);

        const int sz = queueSize();
        if (sz >= kBacklogWarnSize)
            emit queueBacklog(sz);

        {
            QMutexLocker lock(&m_queueMutex);
            if (m_stop && m_taskQueue.isEmpty())
                break;
        }
    }
}
