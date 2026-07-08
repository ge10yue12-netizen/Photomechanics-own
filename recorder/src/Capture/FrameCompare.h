#pragma once

#include "IScreenCapture.h"

namespace recorder
{
namespace capture
{

// 屏录静止判定：DXGI 新帧但像素几乎不变时仍视为静止（过滤光标闪烁、任务栏时钟等）。
struct FrameChangeParams
{
    int sampleStep = 16;
    int channelTolerance = 3;
    double maxChangedRatio = 0.001;
};

bool framesVisuallyChanged(const CaptureFrame &current,
                           const CaptureFrame &reference,
                           const FrameChangeParams &params = FrameChangeParams{});

void copyFramePixels(const CaptureFrame &src, CaptureFrame *dst);

} // namespace capture
} // namespace recorder
