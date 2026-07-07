#pragma once

#include "RecorderKit.h"

#include <QWidget>

class QLabel;
class QToolButton;

namespace Ui {
class RecorderControlBar;
}

// 录屏控制条（RecorderControlBar.ui 可编辑布局/时长样式；圆形按钮为自绘）。
class RecorderControlBar : public QWidget
{
    Q_OBJECT

public:
    explicit RecorderControlBar(QWidget *parent = nullptr);
    ~RecorderControlBar() override;

    void setRecorderState(recorder::RecorderState state);
    void setDurationSeconds(int seconds);
    void setControlsEnabled(bool enabled);

signals:
    void startClicked();
    void pauseClicked();
    void resumeClicked();
    void stopClicked();

private:
    void installRoundButtons();
    QToolButton *replaceWithRound(QToolButton *placeholder, int iconKind);
    void updateButtonVisibility(recorder::RecorderState state);
    void setStopAccent(const QColor &color);

    Ui::RecorderControlBar *ui = nullptr;
    QToolButton *m_startBtn = nullptr;
    QToolButton *m_pauseBtn = nullptr;
    QToolButton *m_resumeBtn = nullptr;
    QToolButton *m_stopBtn = nullptr;
    recorder::RecorderState m_lastState = recorder::RecorderState::Idle;
    bool m_controlsEnabled = true;
};

