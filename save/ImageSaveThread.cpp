// ImageSaveThread.cpp：有界存图任务队列与 BMP 写盘线程实现

#include "ImageSaveThread.h"
#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QMutexLocker>

namespace
{
void writeLe32(char *p, quint32 v)
{
    p[0] = static_cast<char>(v & 0xff);
    p[1] = static_cast<char>((v >> 8) & 0xff);
    p[2] = static_cast<char>((v >> 16) & 0xff);
    p[3] = static_cast<char>((v >> 24) & 0xff);
}

void writeLe16(char *p, quint16 v)
{
    p[0] = static_cast<char>(v & 0xff);
    p[1] = static_cast<char>((v >> 8) & 0xff);
}

// 24 位未压缩 BMP 直写磁盘（RGB888→BGR、自下而上），避免 QImage::save 通用编码开销
bool writeBmpFile(const QString &path, const QImage &image)
{
    if (image.isNull())
        return false;

    const QImage *rgbPtr = &image;
    QImage converted;
    if (image.format() != QImage::Format_RGB888)
    {
        converted = image.convertToFormat(QImage::Format_RGB888);
        if (converted.isNull())
            return false;
        rgbPtr = &converted;
    }

    const QImage &rgb = *rgbPtr;
    const int w = rgb.width();
    const int h = rgb.height();
    if (w <= 0 || h <= 0)
        return false;

    const int dstRowBytes = ((w * 3 + 3) / 4) * 4; // BMP 行须 4 字节对齐
    const quint32 pixelBytes = static_cast<quint32>(dstRowBytes * h);
    const quint32 fileSize = 54u + pixelBytes;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    char fileHeader[14] = {};
    fileHeader[0] = 'B';
    fileHeader[1] = 'M';
    writeLe32(fileHeader + 2, fileSize);
    writeLe32(fileHeader + 10, 54u);
    if (file.write(fileHeader, 14) != 14)
        return false;

    char dibHeader[40] = {};
    writeLe32(dibHeader + 0, 40u);
    writeLe32(dibHeader + 4, static_cast<quint32>(w));
    writeLe32(dibHeader + 8, static_cast<quint32>(h));
    writeLe16(dibHeader + 12, 1);
    writeLe16(dibHeader + 14, 24);
    writeLe32(dibHeader + 20, pixelBytes);
    if (file.write(dibHeader, 40) != 40)
        return false;

    QByteArray rowBuf(dstRowBytes, 0);
    char *const dst = rowBuf.data();

    // BMP 像素区自下而上；源图为 RGB888，写入时交换为 BGR
    for (int y = h - 1; y >= 0; --y)
    {
        const uchar *src = rgb.constScanLine(y);
        for (int x = 0; x < w; ++x)
        {
            const int sx = x * 3;
            const int dx = x * 3;
            dst[dx + 0] = static_cast<char>(src[sx + 2]);
            dst[dx + 1] = static_cast<char>(src[sx + 1]);
            dst[dx + 2] = static_cast<char>(src[sx + 0]);
        }
        if (file.write(rowBuf.constData(), dstRowBytes) != dstRowBytes)
            return false;
    }

    return file.flush();
}
}

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

        const bool ok = writeBmpFile(task.filePath, task.image);
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
