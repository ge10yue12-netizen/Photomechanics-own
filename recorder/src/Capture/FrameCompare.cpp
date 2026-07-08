#include "FrameCompare.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace recorder
{
namespace capture
{
namespace
{

bool pixelDifferent(const std::uint8_t *a, const std::uint8_t *b, int tolerance)
{
    for (int ch = 0; ch < 3; ++ch)
    {
        if (std::abs(static_cast<int>(a[ch]) - static_cast<int>(b[ch])) > tolerance)
            return true;
    }
    return false;
}

} // namespace

bool framesVisuallyChanged(const CaptureFrame &current,
                           const CaptureFrame &reference,
                           const FrameChangeParams &params)
{
    if (current.width != reference.width || current.height != reference.height)
        return true;
    if (current.bgra.empty() || reference.bgra.empty())
        return true;

    const int step = params.sampleStep > 0 ? params.sampleStep : 16;
    const int tolerance = params.channelTolerance >= 0 ? params.channelTolerance : 0;
    const double maxRatio = params.maxChangedRatio > 0.0 ? params.maxChangedRatio : 0.001;

    int sampled = 0;
    int changed = 0;

    for (int y = 0; y < current.height; y += step)
    {
        const std::uint8_t *rowA =
            current.bgra.data() + static_cast<size_t>(y) * static_cast<size_t>(current.stride);
        const std::uint8_t *rowB =
            reference.bgra.data() + static_cast<size_t>(y) * static_cast<size_t>(reference.stride);
        for (int x = 0; x < current.width; x += step)
        {
            ++sampled;
            if (pixelDifferent(rowA + x * 4, rowB + x * 4, tolerance))
                ++changed;
        }
    }

    if (sampled <= 0)
        return true;

    return static_cast<double>(changed) / static_cast<double>(sampled) > maxRatio;
}

void copyFramePixels(const CaptureFrame &src, CaptureFrame *dst)
{
    if (!dst)
        return;

    dst->width = src.width;
    dst->height = src.height;
    dst->stride = src.stride;
    dst->hasNewPixels = src.hasNewPixels;
    dst->bgra = src.bgra;
}

} // namespace capture
} // namespace recorder
