#include "ScreenRecorderSettingsDialog.h"
#include "ui_ScreenRecorderSettingsDialog.h"

#include "../RecorderUiBinder.h"

#include <QComboBox>
#include <QSize>
#include <QSpinBox>

ScreenRecorderSettingsDialog::ScreenRecorderSettingsDialog(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ScreenRecorderSettingsDialog)
{
    ui->setupUi(this);

    const QSize defSize = RecorderUiBinder::captureSourceSize(recorder::CaptureMode::FullScreen, {}, false);
    ui->widthSpin->setValue(defSize.width());
    ui->heightSpin->setValue(defSize.height());

    RecorderUiBinder::fillVideoFormatCombo(ui->formatCombo);

    const auto notify = [this]() { emit settingsChanged(); };
    connect(ui->fpsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, notify);
    connect(ui->widthSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, notify);
    connect(ui->heightSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, notify);
    connect(ui->bitrateSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, notify);
    connect(ui->formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, notify);
}

ScreenRecorderSettingsDialog::~ScreenRecorderSettingsDialog()
{
    delete ui;
}

QSpinBox *ScreenRecorderSettingsDialog::fpsSpin() const
{
    return ui->fpsSpin;
}

QSpinBox *ScreenRecorderSettingsDialog::widthSpin() const
{
    return ui->widthSpin;
}

QSpinBox *ScreenRecorderSettingsDialog::heightSpin() const
{
    return ui->heightSpin;
}

QSpinBox *ScreenRecorderSettingsDialog::bitrateSpin() const
{
    return ui->bitrateSpin;
}

QComboBox *ScreenRecorderSettingsDialog::formatCombo() const
{
    return ui->formatCombo;
}

void ScreenRecorderSettingsDialog::setControlsEnabled(bool enabled)
{
    ui->fpsSpin->setEnabled(enabled);
    ui->widthSpin->setEnabled(enabled);
    ui->heightSpin->setEnabled(enabled);
    ui->bitrateSpin->setEnabled(enabled);
    ui->formatCombo->setEnabled(enabled);
}
