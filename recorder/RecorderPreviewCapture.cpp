#include "RecorderPreviewCapture.h"

#include "src/Capture/CaptureOpen.h"

#include <QImage>
#include <cstring>
#include <memory>

namespace
{

constexpr int kPreviewMaxLongEdge = 960;

QImage frameToPreviewImage(const recorder::CaptureFrame &frame)
{
    if (frame.width <= 0 || frame.height <= 0 || frame.bgra.empty())
        return QImage();

    QImage img(frame.width, frame.height, QImage::Format_ARGB32);
    const int copyBytes = frame.width * 4;
    for (int y = 0; y < frame.height; ++y)
    {
        const std::uint8_t *srcRow =
            frame.bgra.data() + static_cast<size_t>(y) * static_cast<size_t>(frame.stride);
        std::memcpy(img.scanLine(y), srcRow, static_cast<size_t>(copyBytes));
    }

    const int longEdge = qMax(img.width(), img.height());
    if (longEdge <= kPreviewMaxLongEdge)
        return img;

    QSize target = img.size();
    target.scale(kPreviewMaxLongEdge, kPreviewMaxLongEdge, Qt::KeepAspectRatio);
    // 预览仅缩小展示，用 Fast 避免二次模糊；成片不受此影响。
    return img.scaled(target, Qt::KeepAspectRatio, Qt::FastTransformation);
}

struct PreviewSession
{
    std::unique_ptr<recorder::IScreenCapture> capture;
    recorder::CaptureMode mode = recorder::CaptureMode::FullScreen;
    recorder::Rect region;
    bool regionValid = false;

    void close()
    {
        if (capture)
            capture->close();
        capture.reset();
    }

    bool matches(recorder::CaptureMode m, const recorder::Rect &r, bool valid) const
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
        return true;
    }

    bool ensure(recorder::CaptureMode m, const recorder::Rect &r, bool valid, QString *errorMessage)
    {
        if (matches(m, r, valid))
            return true;

        close();
        mode = m;
        region = r;
        regionValid = valid;

        recorder::Rect openRegion = r;
        if (m == recorder::CaptureMode::FullScreen)
            openRegion = recorder::Rect{};

        std::string capErr;
        if (!recorder::capture::openPreviewCapture(m, openRegion, &capture, &capErr))
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

        recorder::CaptureFrame frame;
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
               QImage *image,
               QString *errorMessage)
{
    if (!image)
        return false;

    PreviewSession &s = session();
    if (!s.ensure(mode, region, regionValid, errorMessage))
        return false;

    return s.grab(image, errorMessage);
}

void releaseSessions()
{
    session().close();
}

} // namespace RecorderPreviewCapture
