#pragma once

#include "IScreenCapture.h"

#include <cstdint>
#include <memory>
#include <string>

namespace recorder
{

class VisualFrameCache;

namespace capture
{

bool openScreenCapture(CaptureMode mode,
                       const Rect &region,
                       const WindowCaptureTarget &windowTarget,
                       std::unique_ptr<IScreenCapture> *out,
                       std::string *error,
                       std::string *backendHint);

bool openPreviewCapture(CaptureMode mode,
                        const Rect &region,
                        const WindowCaptureTarget &windowTarget,
                        std::unique_ptr<IScreenCapture> *out,
                        std::string *error);

bool copyVisualCacheToPreviewImage(const VisualFrameCache *cache,
                                   int maxLongEdge,
                                   std::vector<std::uint8_t> *bgraOut,
                                   int *widthOut,
                                   int *heightOut,
                                   int *strideOut);

} // namespace capture
} // namespace recorder
