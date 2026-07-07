#pragma once

#include <QWidget>

namespace Ui {
class RecorderNavSidebar;
}

// 录屏面板左侧导航（RecorderNavSidebar.ui 可在 Qt Designer 编辑）。
class RecorderNavSidebar : public QWidget
{
    Q_OBJECT

public:
    enum Page
    {
        General = 0,
        Settings = 1,
        List = 2
    };

    explicit RecorderNavSidebar(QWidget *parent = nullptr);
    ~RecorderNavSidebar() override;

    Page currentPage() const { return m_current; }
    void setCurrentPage(Page page);
    void setSettingsEnabled(bool enabled);

signals:
    void pageChanged(RecorderNavSidebar::Page page);

private:
    void selectPage(Page page);

    Ui::RecorderNavSidebar *ui = nullptr;
    Page m_current = General;
};
