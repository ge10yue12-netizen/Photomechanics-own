// PreviewWidget.cpp：预览区缩放、平移与像素灰度读数

#include "PreviewWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QtMath>
#include <QWheelEvent>

namespace
{
constexpr double kMinViewScale = 0.02;
constexpr double kMaxViewScale = 32.0;
constexpr double kWheelZoomFactor = 1.15;
}

PreviewWidget::PreviewWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(640, 512);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setStyleSheet(QStringLiteral("background-color: rgb(26, 26, 26); color: rgb(136, 136, 136);"));
}

void PreviewWidget::setImage(const QImage &image)
{
    if (image.isNull())
        return;

    const bool sizeChanged = !m_hasImage || m_image.size() != image.size();
    m_image = image;
    m_hasImage = true;

    // 全画幅模式下每帧保持适应窗口；Manual/OneToOne 保持用户当前视图
    if (sizeChanged || m_viewMode == ViewMode::Fit)
        applyFitView();

    update();
}

void PreviewWidget::clearImage()
{
    m_image = QImage();
    m_hasImage = false;
    m_viewMode = ViewMode::Fit;
    m_viewScale = 1.0;
    m_viewTopLeft = QPointF();
    m_dragging = false;
    unsetCursor();
    emit pixelInfoChanged(0, 0, 0, false);
    update();
}

void PreviewWidget::setPlaceholderText(const QString &text)
{
    m_placeholder = text;
    if (!m_hasImage)
        update();
}

double PreviewWidget::fitScale() const
{
    if (!m_hasImage || width() <= 0 || height() <= 0)
        return 1.0;

    const double sx = static_cast<double>(width()) / static_cast<double>(m_image.width());
    const double sy = static_cast<double>(height()) / static_cast<double>(m_image.height());
    return qMin(sx, sy);
}

void PreviewWidget::applyFitView()
{
    if (!m_hasImage)
        return;

    m_viewMode = ViewMode::Fit;
    m_viewScale = fitScale();
    const QSizeF shown = displayedImageSize();
    m_viewTopLeft = QPointF((width() - shown.width()) * 0.5, (height() - shown.height()) * 0.5);
    unsetCursor();
}

void PreviewWidget::applyOneToOneView()
{
    if (!m_hasImage)
        return;

    m_viewMode = ViewMode::OneToOne;
    m_viewScale = 1.0;
    const QSizeF shown = displayedImageSize();
    m_viewTopLeft = QPointF((width() - shown.width()) * 0.5, (height() - shown.height()) * 0.5);
    setCursor(Qt::OpenHandCursor);
}

QPointF PreviewWidget::imageTopLeft() const
{
    return m_viewTopLeft;
}

QSizeF PreviewWidget::displayedImageSize() const
{
    if (!m_hasImage)
        return QSizeF();
    return QSizeF(m_image.width() * m_viewScale, m_image.height() * m_viewScale);
}

bool PreviewWidget::mapToImage(const QPoint &widgetPos, int *outX, int *outY) const
{
    if (!m_hasImage || !outX || !outY)
        return false;

    const double ix = (widgetPos.x() - m_viewTopLeft.x()) / m_viewScale;
    const double iy = (widgetPos.y() - m_viewTopLeft.y()) / m_viewScale;
    const int x = static_cast<int>(ix);
    const int y = static_cast<int>(iy);
    if (x < 0 || y < 0 || x >= m_image.width() || y >= m_image.height())
        return false;

    *outX = x;
    *outY = y;
    return true;
}

int PreviewWidget::sampleGrayAt(int x, int y) const
{
    if (!m_hasImage || x < 0 || y < 0 || x >= m_image.width() || y >= m_image.height())
        return 0;

    switch (m_image.format())
    {
    case QImage::Format_Grayscale8:
        return static_cast<int>(m_image.constScanLine(y)[x]);
    case QImage::Format_RGB888:
    case QImage::Format_ARGB32:
        return qGray(m_image.pixel(x, y));
    default:
    {
        const QImage gray = m_image.convertToFormat(QImage::Format_Grayscale8);
        if (gray.isNull())
            return 0;
        return static_cast<int>(gray.constScanLine(y)[x]);
    }
    }
}

void PreviewWidget::emitPixelInfoAt(const QPoint &widgetPos)
{
    int x = 0;
    int y = 0;
    if (!mapToImage(widgetPos, &x, &y))
    {
        emit pixelInfoChanged(0, 0, 0, false);
        return;
    }
    emit pixelInfoChanged(x, y, sampleGrayAt(x, y), true);
}

void PreviewWidget::clampScale()
{
    m_viewScale = qBound(kMinViewScale, m_viewScale, kMaxViewScale);
}

void PreviewWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.fillRect(rect(), QColor(26, 26, 26));

    if (!m_hasImage)
    {
        if (!m_placeholder.isEmpty())
        {
            painter.setPen(QColor(136, 136, 136));
            painter.drawText(rect(), Qt::AlignCenter, m_placeholder);
        }
        return;
    }

    const QRectF target(m_viewTopLeft, displayedImageSize());
    painter.setRenderHint(QPainter::SmoothPixmapTransform, m_viewScale < 1.0);
    painter.drawImage(target, m_image, QRectF(0, 0, m_image.width(), m_image.height()));
}

void PreviewWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (!m_hasImage)
        return;

    if (m_viewMode == ViewMode::Fit)
        applyFitView();
    else if (m_viewMode == ViewMode::OneToOne)
        applyOneToOneView();
    else
    {
        // Manual 模式窗口尺寸变化时保持比例，仅重新居中
        const QSizeF shown = displayedImageSize();
        m_viewTopLeft = QPointF((width() - shown.width()) * 0.5, (height() - shown.height()) * 0.5);
    }
}

void PreviewWidget::wheelEvent(QWheelEvent *event)
{
    if (!m_hasImage)
    {
        event->ignore();
        return;
    }

    const QPoint pos = event->pos();
    int ix = 0;
    int iy = 0;
    if (!mapToImage(pos, &ix, &iy))
    {
        ix = m_image.width() / 2;
        iy = m_image.height() / 2;
    }

    const double factor = event->angleDelta().y() > 0 ? kWheelZoomFactor : 1.0 / kWheelZoomFactor;
    m_viewScale *= factor;
    clampScale();
    m_viewMode = ViewMode::Manual;

    const double wx = pos.x();
    const double wy = pos.y();
    m_viewTopLeft = QPointF(wx - ix * m_viewScale, wy - iy * m_viewScale);
    setCursor(Qt::OpenHandCursor);
    emitPixelInfoAt(pos);
    update();
    event->accept();
}

void PreviewWidget::mousePressEvent(QMouseEvent *event)
{
    if (!m_hasImage || event->button() != Qt::LeftButton)
    {
        QWidget::mousePressEvent(event);
        return;
    }

    m_dragging = true;
    m_lastDragPos = event->pos();
    setCursor(Qt::ClosedHandCursor);
    emitPixelInfoAt(event->pos());
}

void PreviewWidget::mouseMoveEvent(QMouseEvent *event)
{
    emitPixelInfoAt(event->pos());

    if (!m_dragging || !m_hasImage)
    {
        QWidget::mouseMoveEvent(event);
        return;
    }

    const QPoint delta = event->pos() - m_lastDragPos;
    m_lastDragPos = event->pos();
    m_viewTopLeft += delta;
    m_viewMode = ViewMode::Manual;
    update();
}

void PreviewWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_dragging)
    {
        m_dragging = false;
        if (m_hasImage)
            setCursor(Qt::OpenHandCursor);
        else
            unsetCursor();
    }
    QWidget::mouseReleaseEvent(event);
}

void PreviewWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (!m_hasImage)
        return;

    // 双击在「适应窗口」与「1:1 原尺寸」之间切换
    if (m_viewMode == ViewMode::OneToOne)
        applyFitView();
    else
        applyOneToOneView();

    emitPixelInfoAt(event->pos());
    update();
}

void PreviewWidget::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    emit pixelInfoChanged(0, 0, 0, false);
}
