#include "GuideOverlay.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

// 构造函数：铺满父控件，启用半透明绘制，鼠标事件穿透至下层。
GuideOverlay::GuideOverlay(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    if (parentWidget())
        setGeometry(parentWidget()->rect());
}

// 设置绘制主题并触发重绘。
void GuideOverlay::setTheme(const GuideTheme &theme)
{
    m_theme = theme;
    update();
}

// 设置高亮参考控件 target 与外扩边距 margin，并重新计算洞区矩形。
void GuideOverlay::setTarget(QWidget *target, int margin)
{
    m_target = target;
    m_margin = margin;
    updateTargetRect();
    update();
}

// 父控件尺寸变化后，将遮罩调整为父控件大小并更新洞区。
void GuideOverlay::refreshGeometry()
{
    if (parentWidget())
        setGeometry(parentWidget()->rect());
    updateTargetRect();
    update();
}

// 绘制全屏半透明遮罩，并在 target 区域挖出圆角洞，绘制高亮边框。
void GuideOverlay::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QPainterPath fullPath;
    fullPath.addRect(rect());

    QPainterPath highlightPath;
    if (m_targetRect.isValid())
        highlightPath.addRoundedRect(m_targetRect, m_theme.highlightRadius, m_theme.highlightRadius);

    painter.fillPath(fullPath.subtracted(highlightPath), m_theme.maskColor);

    if (m_targetRect.isValid())
    {
        painter.setPen(QPen(m_theme.highlightColor, m_theme.highlightPenWidth));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(m_targetRect, m_theme.highlightRadius, m_theme.highlightRadius);
    }
}

// 根据 m_target 在父控件坐标系中计算高亮洞区矩形 m_targetRect。
void GuideOverlay::updateTargetRect()
{
    m_targetRect = QRect();
    if (!m_target || !parentWidget() || !m_target->isVisible())
        return;

    const QPoint topLeft = m_target->mapTo(parentWidget(), QPoint(0, 0));
    QRect rect(topLeft, m_target->size());
    rect = rect.adjusted(-m_margin, -m_margin, m_margin, m_margin);
    m_targetRect = rect.intersected(parentWidget()->rect());
}
