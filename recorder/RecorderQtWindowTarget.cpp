#include "RecorderQtWindowTarget.h"

#include <QImage>
#include <cstring>

namespace
{

bool captureFrameFromImage(const QImage &image, recorder::CaptureFrame *out)
{
    if (!out || image.isNull())
        return false;

    QImage src = image.convertToFormat(QImage::Format_ARGB32);
    if (src.isNull())
        return false;

    const int width = src.width();
    const int height = src.height();
    if (width < 1 || height < 1)
        return false;

    const int stride = width * 4;
    out->width = width;
    out->height = height;
    out->stride = stride;
    out->hasNewPixels = true;
    out->bgra.resize(static_cast<size_t>(stride) * static_cast<size_t>(height));

    if (src.bytesPerLine() == stride)
    {
        std::memcpy(out->bgra.data(), src.constBits(), out->bgra.size());
        return true;
    }

    for (int y = 0; y < height; ++y)
        std::memcpy(out->bgra.data() + static_cast<size_t>(y) * static_cast<size_t>(stride),
                    src.constScanLine(y),
                    static_cast<size_t>(stride));
    return true;
}

int countNonBackgroundPixels(const QImage &img, int bgR, int bgG, int bgB, int tolerance = 8)
{
    if (img.isNull())
        return 0;
    int count = 0;
    for (int y = 0; y < img.height(); ++y)
    {
        const QRgb *row = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        for (int x = 0; x < img.width(); ++x)
        {
            const QRgb px = row[x];
            if (qAbs(qRed(px) - bgR) > tolerance || qAbs(qGreen(px) - bgG) > tolerance ||
                qAbs(qBlue(px) - bgB) > tolerance)
                ++count;
        }
    }
    return count;
}

} // namespace

QWidgetRecorderTarget::QWidgetRecorderTarget(QWidget *widget)
{
    setWidget(widget);
}

void QWidgetRecorderTarget::setWidget(QWidget *widget)
{
    m_widget = widget;
    bindVisualSource(widget);
}

void QWidgetRecorderTarget::bindVisualSource(QWidget *widget)
{
    Q_UNUSED(widget);
    if (m_widget)
        m_widget->setAttribute(Qt::WA_NativeWindow, true);
}

QWidget *QWidgetRecorderTarget::widget() const
{
    return m_widget;
}

std::uintptr_t QWidgetRecorderTarget::windowHandle() const
{
    if (!m_widget || !m_widget->isVisible())
        return 0;
    return static_cast<std::uintptr_t>(m_widget->winId());
}

bool QWidgetRecorderTarget::isAvailable() const
{
    if (!m_widget || !m_widget->isVisible())
        return false;
    if (m_widget->width() < 100 || m_widget->height() < 100)
        return false;

    if (const auto *visualSource = dynamic_cast<const IRecorderQtVisualSource *>(m_widget.data()))
    {
        const QImage snap = visualSource->recorderVisualSnapshot();
        if (snap.isNull())
            return false;
        return countNonBackgroundPixels(snap, 26, 26, 26) > 32;
    }

    return m_widget->winId() != 0;
}

recorder::VisualFrameCache *QWidgetRecorderTarget::visualCache()
{
    return &m_cache;
}

void QWidgetRecorderTarget::refreshVisualCache(bool force)
{
    if (!m_widget || !m_widget->isVisible())
        return;
    if (!force && !hasVisualConsumers())
        return;

    QImage snap;
    if (const auto *visualSource = dynamic_cast<const IRecorderQtVisualSource *>(m_widget.data()))
        snap = visualSource->recorderVisualSnapshot();
    else
        snap = m_widget->grab().toImage();

    recorder::CaptureFrame frame;
    if (!captureFrameFromImage(snap, &frame))
        return;
    m_cache.publish(std::move(frame));
}

void QWidgetRecorderTarget::setVisualConsumerFlags(int flags)
{
    const int prev = visualConsumerFlags();
    recorder::IRecorderWindowTarget::setVisualConsumerFlags(flags);
    if (prev == 0 && flags != 0)
        refreshVisualCache(true);
}
