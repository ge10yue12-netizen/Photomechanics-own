#include "QtWidgetVisualCapture.h"

#include "../../include/VisualFrameCache.h"

namespace recorder
{

QtWidgetVisualCapture::QtWidgetVisualCapture(VisualFrameCache *cache)
    : m_cache(cache)
{
}

bool QtWidgetVisualCapture::open(CaptureMode mode, const Rect &region, std::string *error)
{
    (void)region;
    close();
    if (mode != CaptureMode::Window)
    {
        if (error)
            *error = "Qt 视觉缓存采集仅支持 Window 模式。";
        return false;
    }
    if (!m_cache || !m_cache->hasFrame())
    {
        if (error)
            *error = "视觉缓存尚无有效帧，请确认预览区已显示内容。";
        return false;
    }

    CaptureFrame probe;
    if (!m_cache->copyLatest(&probe))
    {
        if (error)
            *error = "读取视觉缓存失败。";
        return false;
    }

    m_width = probe.width;
    m_height = probe.height;
    m_open = true;
    return true;
}

bool QtWidgetVisualCapture::grab(CaptureFrame *frame, std::string *error)
{
    if (!m_open || !frame || !m_cache)
    {
        if (error)
            *error = "Qt 视觉缓存采集未初始化。";
        return false;
    }
    if (!m_cache->copyLatest(frame))
    {
        if (error)
            *error = "视觉缓存无有效帧。";
        return false;
    }
    frame->hasNewPixels = true;
    m_width = frame->width;
    m_height = frame->height;
    return true;
}

void QtWidgetVisualCapture::close()
{
    m_open = false;
    m_width = 0;
    m_height = 0;
}

int QtWidgetVisualCapture::captureWidth() const
{
    return m_width;
}

int QtWidgetVisualCapture::captureHeight() const
{
    return m_height;
}

} // namespace recorder
