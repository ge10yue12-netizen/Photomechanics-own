#pragma once

#include "GuideTheme.h"

#include <QFrame>

class QLabel;
class QPushButton;

// 引导气泡：显示步骤文案、操作提示、进度与导航按钮。
class GuideBubbleWidget : public QFrame
{
    Q_OBJECT

public:
    explicit GuideBubbleWidget(QWidget *parent = nullptr);

    // 设置并应用主题
    void setTheme(const GuideTheme &theme);
    // 设置标题、说明与进度（index 从 0 起）
    void setContent(const QString &title, const QString &description, int index, int total);
    // 设置操作提示；空字符串时隐藏提示行
    void setActionHint(const QString &hint);
    // 设置导航按钮：first 首步禁用上一步，last 末步显示「完成」
    void setNavigationState(bool first, bool last, bool allowPrevious, bool allowSkip);
    // 设置「下一步」或「完成」是否可点
    void setNextEnabled(bool enabled);
    // 限制气泡最大宽度
    void setMaximumContentWidth(int width);

signals:
    void previousRequested();
    void nextRequested();
    void skipRequested();
    void finishRequested();

private:
    // 将 m_theme 应用到字体、颜色与边距
    void applyTheme();

    QLabel *m_title = nullptr;
    QLabel *m_description = nullptr;
    QLabel *m_actionHint = nullptr;
    QLabel *m_progress = nullptr;
    QPushButton *m_previous = nullptr;
    QPushButton *m_next = nullptr;
    QPushButton *m_skip = nullptr;
    GuideTheme m_theme;
};
