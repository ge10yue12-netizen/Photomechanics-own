#pragma once

#include "RecorderOutputStore.h"

#include <QWidget>

class QTableWidget;

namespace Ui {
class RecorderOutputListWidget;
}

// 录制输出历史表格（RecorderOutputListWidget.ui 可在 Qt Designer 编辑）。
class RecorderOutputListWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RecorderOutputListWidget(QWidget *parent = nullptr);
    ~RecorderOutputListWidget() override;

    void setStore(RecorderOutputStore *store);
    void reloadList();
    void setControlsEnabled(bool enabled);

private slots:
    void onContextMenuRequested(const QPoint &pos);
    void onItemDoubleClicked(int row, int column);
    void onPlay();
    void onCopy();
    void onRename();
    void onOpenFolder();
    void onDelete();

private:
    int selectedRow() const;
    QString selectedPath() const;
    void refreshTable();

    Ui::RecorderOutputListWidget *ui = nullptr;
    RecorderOutputStore *m_store = nullptr;
};

