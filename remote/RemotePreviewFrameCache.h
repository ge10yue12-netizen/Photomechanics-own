#pragma once

#include <QByteArray>
#include <QImage>
#include <QMutex>

// WiFi HTTP 预览 JPEG 缓存。
// 职责：按帧序号去重、缩放编码、供 GET /api/preview.jpg 只读访问。
class RemotePreviewFrameCache
{
public:
    // 设置 JPEG 缩放上限与质量系数。
    void setEncodeOptions(int maxWidth, int maxHeight, int jpegQuality);

    // 写入最新帧；frameSeq 相同时跳过重复编码。
    void updateFrame(const QImage &frame, quint64 frameSeq = 0);

    // 清空缓存与帧序号记录。
    void clear();

    // 返回最新 JPEG 字节；无数据时为空。
    QByteArray getLatestJpeg() const;

private:
    int m_maxWidth = 480;
    int m_maxHeight = 360;
    int m_jpegQuality = 55;
    quint64 m_lastFrameSeq = 0;
    mutable QMutex m_mutex;
    QByteArray m_jpeg;
};
