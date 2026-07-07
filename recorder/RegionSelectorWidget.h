#pragma once

#include "RecorderKit.h"

#include <QWidget>

class QPushButton;

// 桌面级区域选择器；选框旁 ✓ / ✗ 确认或取消。
class RegionSelectorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RegionSelectorWidget(QWidget *parent = nullptr);

    bool run(recorder::Rect *region);

signals:
    void regionConfirmed(const recorder::Rect &region);
    void selectionCancelled();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    recorder::Rect localRect() const;
    recorder::Rect toGlobalRect(const recorder::Rect &local) const;
    void finish(bool accepted);
    void updateActionButtons();

    QPushButton *m_confirmBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;

    bool m_dragging = false;
    bool m_finished = false;
    bool m_accepted = false;
    QPoint m_startPoint;
    QPoint m_currentPoint;
    recorder::Rect m_result;
};
