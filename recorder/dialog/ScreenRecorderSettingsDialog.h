#pragma once

#include <QWidget>

class QComboBox;
class QSpinBox;

namespace Ui {
class ScreenRecorderSettingsDialog;
}

// 录屏参数面板（ScreenRecorderSettingsDialog.ui 可在 Qt Designer 编辑，内嵌于主面板）。
class ScreenRecorderSettingsDialog : public QWidget
{
    Q_OBJECT

public:
    explicit ScreenRecorderSettingsDialog(QWidget *parent = nullptr);
    ~ScreenRecorderSettingsDialog() override;

    QSpinBox *fpsSpin() const;
    QSpinBox *widthSpin() const;
    QSpinBox *heightSpin() const;
    QSpinBox *bitrateSpin() const;
    QComboBox *formatCombo() const;

    void setControlsEnabled(bool enabled);

signals:
    void settingsChanged();

private:
    Ui::ScreenRecorderSettingsDialog *ui = nullptr;
};
