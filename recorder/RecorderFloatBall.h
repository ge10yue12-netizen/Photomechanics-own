#pragma once

#include "RecorderKit.h"

#include <QMouseEvent>
#include <QTimer>
#include <QWidget>

class RecorderControlBar;
class QStackedWidget;

// EV 式录屏悬浮球：录制中置顶显示；可拖拽自由定位；展开为迷你控制条。
class RecorderFloatBall : public QWidget
{
    Q_OBJECT

public:
    explicit RecorderFloatBall(QWidget *parent = nullptr);
    ~RecorderFloatBall() override;

    void setRecorderState(recorder::RecorderState state);
    void setDurationSeconds(int seconds);

signals:
    void pauseClicked();
    void resumeClicked();
    void stopClicked();
    void openPanelRequested();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private slots:
    void onPulseTick();
    void onDeferredExpand();

private:
    void setExpanded(bool expanded);
    void cancelDeferredExpand();
    void scheduleDeferredExpand();
    void loadPosition();
    void savePosition();
    void ensureOnScreen();
    QScreen *currentScreen() const;

    QStackedWidget *m_stack = nullptr;
    QWidget *m_ballPage = nullptr;
    QWidget *m_panelPage = nullptr;
    RecorderControlBar *m_controlBar = nullptr;

    QTimer *m_pulseTimer = nullptr;
    QTimer *m_expandClickTimer = nullptr;
    bool m_pulseOn = false;
    bool m_dragging = false;
    bool m_moved = false;
    QPoint m_dragGlobalOffset;
    bool m_expanded = false;

    static constexpr int kEdgeMarginPx = 10;
    static constexpr int kDragThresholdPx = 4;
};
