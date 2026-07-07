#pragma once

#include "IScreenCapture.h"

namespace recorder
{

// GDI BitBlt 屏幕采集（Windows 实现，头文件不含 windows.h）。
class GdiScreenCapture : public IScreenCapture
{
public:
    GdiScreenCapture();
    ~GdiScreenCapture() override;

    bool open(CaptureMode mode, const Rect &region, std::string *error) override;
    bool grab(CaptureFrame *frame, std::string *error) override;
    void close() override;
    int captureWidth() const override;
    int captureHeight() const override;

private:
    class Impl;
    Impl *m_impl;
};

} // namespace recorder
