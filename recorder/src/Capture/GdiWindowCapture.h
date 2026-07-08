#pragma once

#include "IScreenCapture.h"

#include <cstdint>

namespace recorder
{

class GdiWindowCapture : public IScreenCapture
{
public:
    GdiWindowCapture();
    ~GdiWindowCapture() override;

    void setTargetWindow(std::uintptr_t nativeHandle);

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
