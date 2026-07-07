#include "RecorderControlBar.h"
#include "ui_RecorderControlBar.h"
#include "RecorderStatusText.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QToolButton>

namespace
{

enum class IconKind
{
    Play = 0,
    Pause = 1,
    Stop = 2
};

class RoundIconButton : public QToolButton
{
public:
    explicit RoundIconButton(IconKind kind, QWidget *parent = nullptr)
        : QToolButton(parent)
        , m_kind(kind)
    {
        setFixedSize(40, 40);
        setCursor(Qt::PointingHandCursor);
        setAutoRaise(true);
    }

    void setAccentColor(const QColor &color)
    {
        m_accent = color;
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const QRectF circleRect = QRectF(rect()).adjusted(2, 2, -2, -2);
        QPen pen(m_accent, 2.0);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(circleRect);

        p.setPen(Qt::NoPen);
        p.setBrush(m_accent);

        const QPointF c = circleRect.center();
        switch (m_kind)
        {
        case IconKind::Play:
        {
            const qreal s = circleRect.width() * 0.22;
            QPolygonF tri;
            tri << QPointF(c.x() - s * 0.4, c.y() - s)
                << QPointF(c.x() - s * 0.4, c.y() + s)
                << QPointF(c.x() + s * 0.9, c.y());
            p.drawPolygon(tri);
            break;
        }
        case IconKind::Pause:
        {
            const qreal w = circleRect.width() * 0.12;
            const qreal h = circleRect.height() * 0.32;
            p.drawRect(QRectF(c.x() - w - 2, c.y() - h / 2, w, h));
            p.drawRect(QRectF(c.x() + 2, c.y() - h / 2, w, h));
            break;
        }
        case IconKind::Stop:
        {
            const qreal s = circleRect.width() * 0.22;
            p.drawRect(QRectF(c.x() - s / 2, c.y() - s / 2, s, s));
            break;
        }
        }
    }

private:
    IconKind m_kind;
    QColor m_accent = QColor(66, 165, 245);
};

RoundIconButton *asRound(QToolButton *btn)
{
    return static_cast<RoundIconButton *>(btn);
}

} // namespace

RecorderControlBar::RecorderControlBar(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::RecorderControlBar)
{
    ui->setupUi(this);
    ui->controlRow->setStretch(0, 1);
    ui->controlRow->setStretch(ui->controlRow->count() - 1, 1);
    installRoundButtons();

    connect(m_startBtn, &QToolButton::clicked, this, &RecorderControlBar::startClicked);
    connect(m_pauseBtn, &QToolButton::clicked, this, &RecorderControlBar::pauseClicked);
    connect(m_resumeBtn, &QToolButton::clicked, this, &RecorderControlBar::resumeClicked);
    connect(m_stopBtn, &QToolButton::clicked, this, &RecorderControlBar::stopClicked);

    setRecorderState(recorder::RecorderState::Idle);
}

RecorderControlBar::~RecorderControlBar()
{
    delete ui;
}

QToolButton *RecorderControlBar::replaceWithRound(QToolButton *placeholder, int iconKind)
{
    auto *layout = ui->controlRow;
    const int idx = layout->indexOf(placeholder);
    auto *btn = new RoundIconButton(static_cast<IconKind>(iconKind), this);
    layout->removeWidget(placeholder);
    placeholder->deleteLater();
    if (idx >= 0)
        layout->insertWidget(idx, btn);
    else
        layout->addWidget(btn);
    return btn;
}

void RecorderControlBar::installRoundButtons()
{
    m_startBtn = replaceWithRound(ui->startBtn, static_cast<int>(IconKind::Play));
    m_pauseBtn = replaceWithRound(ui->pauseBtn, static_cast<int>(IconKind::Pause));
    m_resumeBtn = replaceWithRound(ui->resumeBtn, static_cast<int>(IconKind::Play));
    m_stopBtn = replaceWithRound(ui->stopBtn, static_cast<int>(IconKind::Stop));

    asRound(m_startBtn)->setAccentColor(QColor(66, 165, 245));
    asRound(m_pauseBtn)->setAccentColor(QColor(66, 165, 245));
    asRound(m_resumeBtn)->setAccentColor(QColor(66, 165, 245));
    setStopAccent(QColor(180, 180, 180));
}

void RecorderControlBar::setRecorderState(recorder::RecorderState state)
{
    m_lastState = state;
    if (!m_controlsEnabled)
        return;

    updateButtonVisibility(state);

    const bool idleLike = state == recorder::RecorderState::Idle ||
                          state == recorder::RecorderState::Initialized ||
                          state == recorder::RecorderState::Stopped ||
                          state == recorder::RecorderState::Error;
    const bool recording = state == recorder::RecorderState::Recording;
    const bool paused = state == recorder::RecorderState::Paused;

    m_startBtn->setEnabled(idleLike);
    m_pauseBtn->setEnabled(recording);
    m_resumeBtn->setEnabled(paused);
    m_stopBtn->setEnabled(recording || paused);

    if (recording || paused)
        setStopAccent(QColor(229, 57, 53));
    else
        setStopAccent(QColor(180, 180, 180));
}

void RecorderControlBar::setStopAccent(const QColor &color)
{
    asRound(m_stopBtn)->setAccentColor(color);
}

void RecorderControlBar::setDurationSeconds(int seconds)
{
    ui->durationValue->setText(RecorderStatusText::formatDuration(seconds));
}

void RecorderControlBar::setControlsEnabled(bool enabled)
{
    if (m_controlsEnabled == enabled)
        return;

    m_controlsEnabled = enabled;
    if (enabled)
    {
        setRecorderState(m_lastState);
        return;
    }

    m_startBtn->setEnabled(false);
    m_pauseBtn->setEnabled(false);
    m_resumeBtn->setEnabled(false);
    m_stopBtn->setEnabled(false);
}

void RecorderControlBar::updateButtonVisibility(recorder::RecorderState state)
{
    const bool idleLike = state == recorder::RecorderState::Idle ||
                          state == recorder::RecorderState::Initialized ||
                          state == recorder::RecorderState::Stopped ||
                          state == recorder::RecorderState::Error;
    const bool recording = state == recorder::RecorderState::Recording;
    const bool paused = state == recorder::RecorderState::Paused;

    m_startBtn->setVisible(idleLike);
    m_pauseBtn->setVisible(recording);
    m_resumeBtn->setVisible(paused);
    m_stopBtn->setVisible(true);
}

