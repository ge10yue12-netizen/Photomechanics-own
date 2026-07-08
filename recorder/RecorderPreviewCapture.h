#pragma once
#include "RecorderKit.h"
#include <QImage>
class QWidget;
namespace recorder
{
struct Rect;
class IRecorderWindowTarget;
}
// 预览帧抓取（GDI / 视觉缓存；与录制线程独立）。
namespace RecorderPreviewCapture
{
bool grabFrame(recorder::CaptureMode mode,
               const recorder::Rect &region,
               bool regionValid,
               recorder::IRecorderWindowTarget *windowTarget,
               QImage *image,
               QString *errorMessage);
void releaseSessions();
} // namespace RecorderPreviewCapture
