#pragma once

#include "IScreenCapture.h"

#include <memory>
#include <string>

namespace recorder
{
namespace capture
{

// 录制：DXGI 优先，GDI 回退。
bool openScreenCapture(CaptureMode mode,
                       const Rect &region,
                       std::unique_ptr<IScreenCapture> *out,
                       std::string *error,
                       std::string *backendHint);

// 预览：GDI 长连接，避免 DXGI 静止桌面超时导致预览空白（录制仍走 DXGI）。
bool openPreviewCapture(CaptureMode mode,
                          const Rect &region,
                          std::unique_ptr<IScreenCapture> *out,
                          std::string *error);

} // namespace capture
} // namespace recorder
