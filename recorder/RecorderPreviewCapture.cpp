#include "RecorderPreviewCapture.h"
#include "include/IRecorderWindowTarget.h"
#include "include/VisualFrameCache.h"
#include "src/Capture/CaptureOpen.h"
#include <QImage>
#include <cstring>
#include <memory>

namespace
{

constexpr int kWindowPreviewMaxLongEdge = 640;

QImage frameToPreviewImage(const recorder::CaptureFrame &frame)
{
    if (frame.width <= 0 || frame.height <= 0 || frame.bgra.empty())
        return QImage();
    QImage img(frame.width, frame.height, QImage::Format_ARGB32);
    const int copyBytes = frame.width * 4;
    if (frame.stride == copyBytes && img.bytesPerLine() == copyBytes)
    {
        std::memcpy(img.bits(), frame.bgra.data(), static_cast<size_t>(copyBytes) * static_cast<size_t>(frame.height));
    }
    else
    {
        for (int y = 0; y < frame.height; ++y)
        {
            const std::uint8_t *srcRow =
                frame.bgra.data() + static_cast<size_t>(y) * static_cast<size_t>(frame.stride);
            std::memcpy(img.scanLine(y), srcRow, static_cast<size_t>(copyBytes));
        }
    }
    return img;
}

bool grabWindowVisualPreview(recorder::IRecorderWindowTarget *windowTarget,
                             QImage *image,
                             QString *errorMessage)
{
    if (!windowTarget || !image || !windowTarget->visualCache())
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("窗口视觉目标无效。");
        return false;
    }

    windowTarget->refreshVisualCache(true);

    std::vector<std::uint8_t> bgra;
    int width = 0;
    int height = 0;
    int stride = 0;
    if (!recorder::capture::copyVisualCacheToPreviewImage(windowTarget->visualCache(),
                                                          kWindowPreviewMaxLongEdge,
                                                          &bgra,
                                                          &width,
                                                          &height,
                                                          &stride))
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("读取窗口视觉缓存失败。");
        return false;
    }

    recorder::CaptureFrame frame;
    frame.width = width;
    frame.height = height;
    frame.stride = stride;
    frame.bgra = std::move(bgra);
    frame.hasNewPixels = true;
    *image = frameToPreviewImage(frame);
    return !image->isNull();
}

struct PreviewSession
{
    std::unique_ptr<recorder::IScreenCapture> capture;
    recorder::CaptureFrame frame;
    recorder::CaptureMode mode = recorder::CaptureMode::FullScreen;
    recorder::Rect region;
    bool regionValid = false;
    recorder::IRecorderWindowTarget *windowTarget = nullptr;

    void close()
    {
        if (capture)
            capture->close();
        capture.reset();
        frame.bgra.clear();
    }

    bool matches(recorder::CaptureMode m,
                 const recorder::Rect &r,
                 bool valid,
                 recorder::IRecorderWindowTarget *target) const
    {
        if (!capture)
            return false;
        if (mode != m)
            return false;
        if (m == recorder::CaptureMode::Region)
        {
            if (!valid)
                return false;
            return region.x == r.x && region.y == r.y && region.width == r.width &&
                   region.height == r.height;
        }
        if (m == recorder::CaptureMode::Window)
            return windowTarget == target;
        return true;
    }

    bool ensure(recorder::CaptureMode m,
                const recorder::Rect &r,
                bool valid,
                recorder::IRecorderWindowTarget *target,
                QString *errorMessage)
    {
        if (matches(m, r, valid, target))
            return true;
        close();
        mode = m;
        region = r;
        regionValid = valid;
        windowTarget = target;
        recorder::Rect openRegion = r;
        if (m == recorder::CaptureMode::FullScreen || m == recorder::CaptureMode::Window)
            openRegion = recorder::Rect{};

        recorder::WindowCaptureTarget wt;
        wt.provider = target;
        wt.nativeHandle = target ? target->windowHandle() : 0;

        std::string capErr;
        if (!recorder::capture::openPreviewCapture(m, openRegion, wt, &capture, &capErr))
        {
            if (errorMessage)
                *errorMessage = QString::fromStdString(capErr);
            return false;
        }
        return true;
    }

    bool grab(QImage *image, QString *errorMessage)
    {
        if (!capture || !image)
            return false;
        std::string capErr;
        if (!capture->grab(&frame, &capErr))
        {
            if (errorMessage)
                *errorMessage = QString::fromStdString(capErr);
            return false;
        }
        *image = frameToPreviewImage(frame);
        return !image->isNull();
    }
};

PreviewSession &session()
{
    static PreviewSession s;
    return s;
}

} // namespace

namespace RecorderPreviewCapture
{

bool grabFrame(recorder::CaptureMode mode,
               const recorder::Rect &region,
               bool regionValid,
               recorder::IRecorderWindowTarget *windowTarget,
               QImage *image,
               QString *errorMessage)
{
    if (!image)
        return false;

    if (mode == recorder::CaptureMode::Window && windowTarget && windowTarget->visualCache())
        return grabWindowVisualPreview(windowTarget, image, errorMessage);

    PreviewSession &s = session();
    if (!s.ensure(mode, region, regionValid, windowTarget, errorMessage))
        return false;
    return s.grab(image, errorMessage);
}

void releaseSessions()
{
    session().close();
}

} // namespace RecorderPreviewCapture
