#pragma once

#include <QImage>

// Qt 控件视觉快照：由控件实现，供录屏模块在主线程调用（比 PrintWindow 可靠且可控）。
class IRecorderQtVisualSource
{
public:
    virtual ~IRecorderQtVisualSource() = default;
    virtual QImage recorderVisualSnapshot() const = 0;
};
