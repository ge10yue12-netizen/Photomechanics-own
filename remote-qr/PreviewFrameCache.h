#pragma once

#include <QByteArray>
#include <QImage>
#include <QMutex>

/**
 * @brief 手机预览 JPEG 缓存。
 *
 * - updateFrame：取帧路径写入图像
 * - clear：相机关闭时清空（配合 MobileHost::clearPreviewFrame）
 * - getLatestJpeg：HTTP 只读；空表示无图像（预览框仍保留）
 */
class PreviewFrameCache
{
public:
    /** @param maxWidth/maxHeight 缩放上限；@param jpegQuality JPEG 质量 1–100。 */
    void setEncodeOptions(int maxWidth, int maxHeight, int jpegQuality);

    /**
     * @brief 写入最新帧；frameSeq 相同时跳过重复编码。
     * @param frame 源图（通常为 Grayscale8）
     * @param frameSeq 相机帧序号；为 0 时不做去重
     */
    void updateFrame(const QImage &frame, quint64 frameSeq = 0);

    /** @brief 清空缓存（相机关闭时调用，手机端不再返回旧帧）。 */
    void clear();

    /** @brief 返回最新 JPEG；无数据时为空。 */
    QByteArray getLatestJpeg() const;

private:
    int m_maxWidth = 480;
    int m_maxHeight = 360;
    int m_jpegQuality = 55;
    quint64 m_lastFrameSeq = 0;
    mutable QMutex m_mutex;
    QByteArray m_jpeg;
};
