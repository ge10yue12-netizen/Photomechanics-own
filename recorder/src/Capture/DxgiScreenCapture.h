#pragma once

#include "IScreenCapture.h"

namespace recorder
{

// DXGI Desktop Duplication 屏幕采集（Windows 8+；失败时由 ScreenRecorder 回退 GDI）。
class DxgiScreenCapture : public IScreenCapture
{
public:
    DxgiScreenCapture();
    ~DxgiScreenCapture() override;

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
