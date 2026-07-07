#include "RegionSelectorWidget.h"

#include <QApplication>
#include <QEventLoop>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScreen>

namespace
{

QRect virtualDesktopRect()
{
    QRect bounds;
    const QList<QScreen *> screens = QGuiApplication::screens();
    for (QScreen *screen : screens)
    {
        if (screen)
            bounds = bounds.united(screen->geometry());
    }
    if (bounds.isValid())
        return bounds;
    if (QScreen *primary = QGuiApplication::primaryScreen())
        return primary->geometry();
    return QRect(0, 0, 1920, 1080);
}

} // namespace

RegionSelectorWidget::RegionSelectorWidget(QWidget *parent)
    : QWidget(parent,
              Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::CrossCursor);

    m_confirmBtn = new QPushButton(QStringLiteral("✓"), this);
    m_cancelBtn = new QPushButton(QStringLiteral("✗"), this);
    for (QPushButton *btn : {m_confirmBtn, m_cancelBtn})
    {
        btn->setFixedSize(36, 28);
        btn->setStyleSheet(QStringLiteral(
            "QPushButton { background: rgba(0,0,0,180); color: white; border: 1px solid #888; font-weight: bold; }"
            "QPushButton:hover { background: rgba(0,120,212,220); }"));
        btn->hide();
    }
    connect(m_confirmBtn, &QPushButton::clicked, this, [this]() {
        const recorder::Rect r = localRect();
        if (r.width >= 100 && r.height >= 100)
            finish(true);
    });
    connect(m_cancelBtn, &QPushButton::clicked, this, [this]() { finish(false); });
}

bool RegionSelectorWidget::run(recorder::Rect *region)
{
    if (!region)
        return false;

    setGeometry(virtualDesktopRect());

    m_finished = false;
    m_accepted = false;
    m_dragging = false;
    show();
    raise();
    activateWindow();
    setFocus();

    QEventLoop loop;
    connect(this, &RegionSelectorWidget::regionConfirmed, &loop, &QEventLoop::quit);
    connect(this, &RegionSelectorWidget::selectionCancelled, &loop, &QEventLoop::quit);
    loop.exec();

    hide();
    if (m_accepted)
        *region = m_result;
    return m_accepted;
}

void RegionSelectorWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.fillRect(rect(), QColor(0, 0, 0, 120));

    const recorder::Rect r = localRect();
    if (r.width > 0 && r.height > 0)
    {
        const QRect qr(r.x, r.y, r.width, r.height);
        p.setCompositionMode(QPainter::CompositionMode_Clear);
        p.fillRect(qr, Qt::transparent);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        p.setPen(QPen(QColor(0, 120, 212), 2));
        p.drawRect(qr);

        const recorder::Rect global = toGlobalRect(r);
        const QString tip = QStringLiteral("%1 × %2").arg(global.width).arg(global.height);
        p.setPen(Qt::white);
        p.drawText(qr.adjusted(4, 4, -4, -4), Qt::AlignTop | Qt::AlignLeft, tip);
    }
    else
    {
        p.setPen(Qt::white);
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("在屏幕上拖拽选择录制区域"));
    }
}

void RegionSelectorWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        m_dragging = true;
        m_startPoint = event->pos();
        m_currentPoint = event->pos();
        update();
        updateActionButtons();
    }
}

void RegionSelectorWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging)
    {
        m_currentPoint = event->pos();
        update();
        updateActionButtons();
    }
}

void RegionSelectorWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_dragging)
    {
        m_dragging = false;
        m_currentPoint = event->pos();
        update();
        updateActionButtons();
    }
}

void RegionSelectorWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
    {
        finish(false);
        return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    {
        const recorder::Rect r = localRect();
        if (r.width >= 100 && r.height >= 100)
            finish(true);
        return;
    }
    QWidget::keyPressEvent(event);
}

recorder::Rect RegionSelectorWidget::localRect() const
{
    recorder::Rect r;
    const int x1 = qMin(m_startPoint.x(), m_currentPoint.x());
    const int y1 = qMin(m_startPoint.y(), m_currentPoint.y());
    const int x2 = qMax(m_startPoint.x(), m_currentPoint.x());
    const int y2 = qMax(m_startPoint.y(), m_currentPoint.y());
    r.x = x1;
    r.y = y1;
    r.width = x2 - x1;
    r.height = y2 - y1;
    return r;
}

recorder::Rect RegionSelectorWidget::toGlobalRect(const recorder::Rect &local) const
{
    const QPoint origin = frameGeometry().topLeft();
    recorder::Rect g;
    g.x = local.x + origin.x();
    g.y = local.y + origin.y();
    g.width = local.width;
    g.height = local.height;
    return g;
}

void RegionSelectorWidget::updateActionButtons()
{
    const recorder::Rect r = localRect();
    const bool valid = r.width >= 100 && r.height >= 100;
    m_confirmBtn->setVisible(valid);
    m_cancelBtn->setVisible(valid);
    if (!valid)
        return;

    const QRect qr(r.x, r.y, r.width, r.height);
    int bx = qr.right() - 80;
    int by = qr.top();
    if (by < 0)
        by = qr.bottom() + 4;
    m_confirmBtn->move(bx, by);
    m_cancelBtn->move(bx + 40, by);
    m_confirmBtn->raise();
    m_cancelBtn->raise();
}

void RegionSelectorWidget::finish(bool accepted)
{
    if (m_finished)
        return;
    m_finished = true;
    m_accepted = accepted;
    if (accepted)
    {
        m_result = toGlobalRect(localRect());
        emit regionConfirmed(m_result);
    }
    else
    {
        emit selectionCancelled();
    }
}
