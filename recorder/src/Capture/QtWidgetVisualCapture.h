#pragma once

#include "IScreenCapture.h"

namespace recorder
{

class VisualFrameCache;

// 从 VisualFrameCache 读取帧（录制线程安全；无 GDI/Qt 调用）。
class QtWidgetVisualCapture : public IScreenCapture
{
public:
    explicit QtWidgetVisualCapture(VisualFrameCache *cache);

    bool open(CaptureMode mode, const Rect &region, std::string *error) override;
    bool grab(CaptureFrame *frame, std::string *error) override;
    void close() override;
    int captureWidth() const override;
    int captureHeight() const override;

private:
    VisualFrameCache *m_cache = nullptr;
    int m_width = 0;
    int m_height = 0;
    bool m_open = false;
};

} // namespace recorder
