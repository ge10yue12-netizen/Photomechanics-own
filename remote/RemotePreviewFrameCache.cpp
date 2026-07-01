#include "RemotePreviewFrameCache.h"

#include <QBuffer>
#include <QMutexLocker>

void RemotePreviewFrameCache::setEncodeOptions(int maxWidth, int maxHeight, int jpegQuality)
{
    QMutexLocker lock(&m_mutex);
    m_maxWidth = maxWidth > 0 ? maxWidth : 480;
    m_maxHeight = maxHeight > 0 ? maxHeight : 360;
    m_jpegQuality = jpegQuality >= 30 && jpegQuality <= 90 ? jpegQuality : 55;
}

void RemotePreviewFrameCache::updateFrame(const QImage &frame, quint64 frameSeq)
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
        scaled = scaled.scaled(maxWidth, maxHeight, Qt::KeepAspectRatio, Qt::FastTransformation);
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

void RemotePreviewFrameCache::clear()
{
    QMutexLocker lock(&m_mutex);
    m_jpeg.clear();
    m_lastFrameSeq = 0;
}

QByteArray RemotePreviewFrameCache::getLatestJpeg() const
{
    QMutexLocker lock(&m_mutex);
    return m_jpeg;
}
