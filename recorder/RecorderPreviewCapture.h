#pragma once

#include "RecorderKit.h"

#include <QImage>

class QString;

// 预览抓帧（GDI 长连接 + 降采样，独立于录制线程；录制走 DXGI）。
namespace RecorderPreviewCapture
{

bool grabFrame(recorder::CaptureMode mode,
               const recorder::Rect &region,
               bool regionValid,
               QImage *image,
               QString *errorMessage);

// 对话框隐藏或模式切换时可释放采集句柄。
void releaseSessions();

} // namespace RecorderPreviewCapture
