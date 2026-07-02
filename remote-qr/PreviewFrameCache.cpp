#include "PreviewFrameCache.h"

#include <QBuffer>
#include <QMutexLocker>

// 写入缩放与质量参数，非法入参回退默认值
void PreviewFrameCache::setEncodeOptions(int maxWidth, int maxHeight, int jpegQuality)
{
    QMutexLocker lock(&m_mutex);
    m_maxWidth = maxWidth > 0 ? maxWidth : 480;
    m_maxHeight = maxHeight > 0 ? maxHeight : 360;
    m_jpegQuality = jpegQuality >= 30 && jpegQuality <= 90 ? jpegQuality : 55;
}

// 锁外编码、锁内写入；同 frameSeq 跳过重复编码
void PreviewFrameCache::updateFrame(const QImage &frame, quint64 frameSeq)
{
    if (frame.isNull())
        return;

    int maxWidth = 480;
    int maxHeight = 360;
    int jpegQuality = 55;
    {
        QMutexLocker lock(&m_mutex);
        if (frameSeq > 0 && frameSeq == m_lastFrameSeq)
            return;
        maxWidth = m_maxWidth;
        maxHeight = m_maxHeight;
        jpegQuality = m_jpegQuality;
    }

    QImage scaled = frame;
    if (scaled.width() > maxWidth || scaled.height() > maxHeight)
    {
        scaled = scaled.scaled(maxWidth, maxHeight, Qt::KeepAspectRatio, Qt::FastTransformation);
    }
    // Grayscale8 须转 RGB888，否则 JPEG 编码静默失败
    if (scaled.format() == QImage::Format_Grayscale8)
        scaled = scaled.convertToFormat(QImage::Format_RGB888);

    QByteArray jpeg;
    QBuffer buffer(&jpeg);
    if (!buffer.open(QIODevice::WriteOnly))
        return;
    if (!scaled.save(&buffer, "JPEG", jpegQuality))
        return;

    QMutexLocker lock(&m_mutex);
    if (frameSeq > 0)
        m_lastFrameSeq = frameSeq;
    m_jpeg = jpeg;
}

// 清空缓存 payload 与序号
void PreviewFrameCache::clear()
{
    QMutexLocker lock(&m_mutex);
    m_jpeg.clear();
    m_lastFrameSeq = 0;
}

// 持锁拷贝最新 JPEG
QByteArray PreviewFrameCache::getLatestJpeg() const
{
    QMutexLocker lock(&m_mutex);
    return m_jpeg;
}

// 持锁读取帧序号
quint64 PreviewFrameCache::lastFrameSeq() const
{
    QMutexLocker lock(&m_mutex);
    return m_lastFrameSeq;
}

// 持锁组装 Snapshot，供 HTTP 单次响应
PreviewFrameCache::Snapshot PreviewFrameCache::snapshot() const
{
    QMutexLocker lock(&m_mutex);
    return {m_jpeg, m_lastFrameSeq};
}
