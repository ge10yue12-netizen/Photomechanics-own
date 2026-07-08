#pragma once

class QComboBox;

// 录屏模块下拉框统一样式（行高、内边距、悬停/选中色，贴近 EV 列表体验）。
class RecorderComboStyle
{
public:
    static void applyTo(QComboBox *combo);
};
