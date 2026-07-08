#include "RecorderFloatBall.h"
#include "RecorderControlBar.h"

#include <QApplication>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace
{

class FloatBallIcon : public QWidget
{
public:
    explicit FloatBallIcon(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setFixedSize(48, 48);
        setCursor(Qt::PointingHandCursor);
    }

    void setPulseOn(bool on)
    {
        if (m_pulseOn == on)
            return;
        m_pulseOn = on;
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const QRectF rect = QRectF(this->rect()).adjusted(4, 4, -4, -4);
        QColor fill(25, 118, 210, m_pulseOn ? 230 : 190);
        QColor ring(255, 255, 255, 210);
        p.setPen(QPen(ring, 2));
        p.setBrush(fill);
        p.drawEllipse(rect);

        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        const qreal r = rect.width() * 0.16;
        p.drawEllipse(QPointF(rect.center().x(), rect.center().y()), r, r);
    }

private:
    bool m_pulseOn = true;
};

} // namespace

RecorderFloatBall::RecorderFloatBall(QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFixedSize(48, 48);

    m_stack = new QStackedWidget(this);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(m_stack);

    m_ballPage = new QWidget(this);
    auto *ballLayout = new QVBoxLayout(m_ballPage);
    ballLayout->setContentsMargins(0, 0, 0, 0);
    auto *ballIcon = new FloatBallIcon(m_ballPage);
    ballIcon->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    ballLayout->addWidget(ballIcon, 0, Qt::AlignCenter);

    m_panelPage = new QWidget(this);
    m_panelPage->setStyleSheet(
        QStringLiteral("background: rgba(255,255,255,245); border: 1px solid #cfd8dc; border-radius: 8px;"));
    auto *panelLayout = new QHBoxLayout(m_panelPage);
    panelLayout->setContentsMargins(8, 6, 8, 6);
    panelLayout->setSpacing(6);

    m_controlBar = new RecorderControlBar(m_panelPage);
    m_controlBar->setMinimumWidth(300);
    panelLayout->addWidget(m_controlBar, 1);

    auto *collapseBtn = new QPushButton(QStringLiteral("●"), m_panelPage);
    collapseBtn->setFixedSize(28, 28);
    collapseBtn->setToolTip(QStringLiteral("收起为悬浮球"));
    collapseBtn->setStyleSheet(
        QStringLiteral("border: none; color: #1976D2; font-size: 16px; background: transparent;"));
    connect(collapseBtn, &QPushButton::clicked, this, [this]() { setExpanded(false); });
    panelLayout->addWidget(collapseBtn, 0, Qt::AlignVCenter);

    m_stack->addWidget(m_ballPage);
    m_stack->addWidget(m_panelPage);
    m_stack->setCurrentWidget(m_ballPage);

    connect(m_controlBar, &RecorderControlBar::pauseClicked, this, &RecorderFloatBall::pauseClicked);
    connect(m_controlBar, &RecorderControlBar::resumeClicked, this, &RecorderFloatBall::resumeClicked);
    connect(m_controlBar, &RecorderControlBar::stopClicked, this, &RecorderFloatBall::stopClicked);

    m_pulseTimer = new QTimer(this);
    m_pulseTimer->setInterval(600);
    connect(m_pulseTimer, &QTimer::timeout, this, &RecorderFloatBall::onPulseTick);

    m_expandClickTimer = new QTimer(this);
    m_expandClickTimer->setSingleShot(true);
    connect(m_expandClickTimer, &QTimer::timeout, this, &RecorderFloatBall::onDeferredExpand);

    loadPosition();
    hide();
}

RecorderFloatBall::~RecorderFloatBall() = default;

void RecorderFloatBall::setRecorderState(recorder::RecorderState state)
{
    m_controlBar->setRecorderState(state);

    const bool active = state == recorder::RecorderState::Recording ||
                        state == recorder::RecorderState::Paused;
    if (active)
    {
        if (!m_pulseTimer->isActive())
            m_pulseTimer->start();
    }
    else
    {
        m_pulseTimer->stop();
    }

    if (auto *icon = m_ballPage->findChild<FloatBallIcon *>())
        icon->setPulseOn(true);
}

void RecorderFloatBall::setDurationSeconds(int seconds)
{
    m_controlBar->setDurationSeconds(seconds);
}

void RecorderFloatBall::setExpanded(bool expanded)
{
    if (m_expanded == expanded)
        return;

    m_expanded = expanded;
    const QPoint center = frameGeometry().center();

    if (expanded)
    {
        setFixedSize(360, 56);
        m_stack->setCurrentWidget(m_panelPage);
    }
    else
    {
        setFixedSize(48, 48);
        m_stack->setCurrentWidget(m_ballPage);
    }

    move(center.x() - width() / 2, center.y() - height() / 2);
    ensureOnScreen();
    savePosition();
}

void RecorderFloatBall::cancelDeferredExpand()
{
    if (m_expandClickTimer)
        m_expandClickTimer->stop();
}

void RecorderFloatBall::scheduleDeferredExpand()
{
    cancelDeferredExpand();
    m_expandClickTimer->start(QApplication::doubleClickInterval());
}

void RecorderFloatBall::onDeferredExpand()
{
    if (m_stack->currentWidget() == m_ballPage)
        setExpanded(true);
}

void RecorderFloatBall::onPulseTick()
{
    m_pulseOn = !m_pulseOn;
    if (auto *icon = m_ballPage->findChild<FloatBallIcon *>())
        icon->setPulseOn(m_pulseOn);
}

void RecorderFloatBall::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
    {
        QWidget::mousePressEvent(event);
        return;
    }

    m_dragging = true;
    m_moved = false;
    cancelDeferredExpand();
    m_dragGlobalOffset = event->globalPos() - frameGeometry().topLeft();
    event->accept();
}

void RecorderFloatBall::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_dragging)
    {
        QWidget::mouseMoveEvent(event);
        return;
    }

    if ((event->globalPos() - (frameGeometry().topLeft() + m_dragGlobalOffset)).manhattanLength() >
        kDragThresholdPx)
    {
        m_moved = true;
        cancelDeferredExpand();
    }

    move(event->globalPos() - m_dragGlobalOffset);
    event->accept();
}

void RecorderFloatBall::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !m_dragging)
    {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    m_dragging = false;
    if (m_moved)
    {
        ensureOnScreen();
        savePosition();
    }
    else if (m_stack->currentWidget() == m_ballPage)
    {
        scheduleDeferredExpand();
    }

    event->accept();
}

void RecorderFloatBall::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        cancelDeferredExpand();
        emit openPanelRequested();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

QScreen *RecorderFloatBall::currentScreen() const
{
    if (QScreen *screen = QGuiApplication::screenAt(frameGeometry().center()))
        return screen;
    return QGuiApplication::primaryScreen();
}

void RecorderFloatBall::ensureOnScreen()
{
    QScreen *screen = currentScreen();
    if (!screen)
        return;

    const QRect area = screen->availableGeometry();
    QPoint pos = frameGeometry().topLeft();
    if (pos.x() < area.left())
        pos.setX(area.left() + kEdgeMarginPx);
    if (pos.y() < area.top())
        pos.setY(area.top() + kEdgeMarginPx);
    if (pos.x() + width() > area.right())
        pos.setX(area.right() - width() - kEdgeMarginPx);
    if (pos.y() + height() > area.bottom())
        pos.setY(area.bottom() - height() - kEdgeMarginPx);
    move(pos);
}

void RecorderFloatBall::loadPosition()
{
    QSettings settings;
    const bool expanded = settings.value(QStringLiteral("recorder/floatBall/expanded"), false).toBool();
    const int x = settings.value(QStringLiteral("recorder/floatBall/x"), -1).toInt();
    const int y = settings.value(QStringLiteral("recorder/floatBall/y"), -1).toInt();

    setExpanded(expanded);

    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;

    const QRect area = screen->availableGeometry();
    if (x >= 0 && y >= 0)
    {
        move(x, y);
        ensureOnScreen();
        return;
    }

    move(area.right() - width() - kEdgeMarginPx, area.bottom() - height() - 80);
}

void RecorderFloatBall::savePosition()
{
    QSettings settings;
    settings.setValue(QStringLiteral("recorder/floatBall/x"), frameGeometry().x());
    settings.setValue(QStringLiteral("recorder/floatBall/y"), frameGeometry().y());
    settings.setValue(QStringLiteral("recorder/floatBall/expanded"), m_expanded);
}
