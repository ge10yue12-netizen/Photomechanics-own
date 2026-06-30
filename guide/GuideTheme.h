#pragma once

#include <QColor>
#include <QFont>

// 引导遮罩、高亮边框与气泡的视觉主题参数。
struct GuideTheme
{
    QColor maskColor = QColor(0, 0, 0, 155);       // 非高亮区半透明遮罩
    QColor highlightColor = QColor(64, 158, 255);  // 高亮边框颜色
    QColor bubbleBackground = QColor(255, 255, 255); // 气泡背景色
    QColor bubbleBorder = QColor(210, 210, 210);     // 气泡边框色
    QColor titleColor = QColor(32, 32, 32);            // 标题文字颜色
    QColor textColor = QColor(64, 64, 64);             // 正文与进度文字颜色
    QColor hintColor = QColor(24, 144, 255);       // 操作提示行文字颜色

    int highlightRadius = 8;     // 高亮圆角半径
    int highlightPenWidth = 2;   // 高亮边框线宽
    int bubbleRadius = 8;        // 气泡圆角（样式表）
    int bubblePadding = 14;      // 气泡内边距
    int bubbleSpacing = 12;      // 气泡与高亮区域的间距

    QFont titleFont;  // 标题字体；pointSize<=0 时由 GuideBubbleWidget 使用默认值
    QFont textFont;     // 正文与进度字体
};
