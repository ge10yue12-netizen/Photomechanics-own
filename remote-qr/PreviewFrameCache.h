#pragma once

#include <QByteArray>
#include <QImage>
#include <QMutex>

// 预览帧 JPEG 缓存；生产者写、HTTP 只读；编码在锁外、写入在锁内
class PreviewFrameCache
{
public:
    // 单次读出的 JPEG 与帧序号
    struct Snapshot
    {
        QByteArray jpeg;
        quint64 frameSeq = 0;
    };

    // 设置缩放上限与 JPEG 质量
    void setEncodeOptions(int maxWidth, int maxHeight, int jpegQuality);
    // 缩放、转格式并编码帧写入缓存
    void updateFrame(const QImage &frame, quint64 frameSeq = 0);
    // 清空 JPEG 与帧序号
    void clear();
    // 返回最新 JPEG 副本
    QByteArray getLatestJpeg() const;
    // 返回当前缓存帧序号
    quint64 lastFrameSeq() const;
    // 一次锁内读出 JPEG 与序号
    Snapshot snapshot() const;

private:
    int m_maxWidth = 480;
    int m_maxHeight = 360;
    int m_jpegQuality = 55;
    quint64 m_lastFrameSeq = 0;
    mutable QMutex m_mutex;
    QByteArray m_jpeg;
};
