#include "RecorderPreviewWidget.h"

#include <QPainter>

RecorderPreviewWidget::RecorderPreviewWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(200, 120);
}

void RecorderPreviewWidget::setPreviewImage(const QImage &image)
{
    m_image = image;
    update();
}

void RecorderPreviewWidget::clearPreview()
{
    m_image = QImage();
    update();
}

void RecorderPreviewWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.fillRect(rect(), QColor(30, 30, 30));

    if (m_image.isNull())
    {
        p.setPen(QColor(180, 180, 180));
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("预览加载中…"));
        return;
    }

    const QRect viewport = rect().adjusted(4, 4, -4, -4);
    QSize drawSize = m_image.size();

    // 仅当画面大于视口时等比缩小；不放大、不拉伸铺满预览框。
    if (drawSize.width() > viewport.width() || drawSize.height() > viewport.height())
        drawSize.scale(viewport.size(), Qt::KeepAspectRatio);

    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    const QImage scaled = (drawSize == m_image.size())
                              ? m_image
                              : m_image.scaled(drawSize, Qt::KeepAspectRatio, Qt::FastTransformation);

    const int x = viewport.x() + (viewport.width() - scaled.width()) / 2;
    const int y = viewport.y() + (viewport.height() - scaled.height()) / 2;
    p.drawImage(QPoint(x, y), scaled);
}
