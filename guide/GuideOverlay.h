#pragma once

#include "GuideTheme.h"

#include <QPointer>
#include <QWidget>

// 全屏半透明遮罩与高亮洞区；鼠标事件穿透，不拦截下层控件。
class GuideOverlay : public QWidget
{
public:
    explicit GuideOverlay(QWidget *parent = nullptr);

    // 设置绘制主题
    void setTheme(const GuideTheme &theme);
    // 设置高亮参考控件与外扩边距
    void setTarget(QWidget *target, int margin);
    // 父控件尺寸变化后同步几何
    void refreshGeometry();

    // 返回高亮洞区矩形（父控件坐标系）
    QRect targetRect() const { return m_targetRect; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    // 根据 m_target 计算 m_targetRect
    void updateTargetRect();

    QPointer<QWidget> m_target;
    QRect m_targetRect;
    int m_margin = 8;
    GuideTheme m_theme;
};
