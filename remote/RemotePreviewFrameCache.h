#pragma once

#include <QByteArray>
#include <QImage>
#include <QMutex>

/**
 * remote/ 模块 WiFi 预览 JPEG 缓存（类名与 remote-qr/PreviewFrameCache 区分，避免同 TU 重定义与 .obj 冲突）。
 */
class RemotePreviewFrameCache
{
public:
    void setEncodeOptions(int maxWidth, int maxHeight, int jpegQuality);

    void updateFrame(const QImage &frame, quint64 frameSeq = 0);

    void clear();

    QByteArray getLatestJpeg() const;

private:
    int m_maxWidth = 480;
    int m_maxHeight = 360;
    int m_jpegQuality = 55;
    quint64 m_lastFrameSeq = 0;
    mutable QMutex m_mutex;
    QByteArray m_jpeg;
};
