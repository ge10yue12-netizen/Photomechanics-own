#pragma once

#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

namespace Ui {
class ScreenRecorderSettingsDialog;
}

// 录屏参数面板（ScreenRecorderSettingsDialog.ui 可在 Qt Designer 编辑）。
class ScreenRecorderSettingsDialog : public QWidget
{
    Q_OBJECT

public:
    explicit ScreenRecorderSettingsDialog(QWidget *parent = nullptr);
    ~ScreenRecorderSettingsDialog() override;

    QComboBox *qualityCombo() const;
    QComboBox *encodeCombo() const;
    QComboBox *fpsCombo() const;
    QComboBox *formatCombo() const;
    QLineEdit *pathEdit() const;
    QPushButton *browseBtn() const;

    void updatePresetSummary(int captureWidth, int captureHeight);
    void setControlsEnabled(bool enabled);

signals:
    void presetChanged();

private:
    void loadPresetSettings();
    void savePresetSettings();
    void applyComboTooltips();
    void applyComboAppearance();
    void applyFormAlignment();

    Ui::ScreenRecorderSettingsDialog *ui = nullptr;
    int m_summaryCaptureW = 0;
    int m_summaryCaptureH = 0;
};
