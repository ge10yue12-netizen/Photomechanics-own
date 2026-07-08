#include "ScreenRecorderSettingsDialog.h"
#include "ui_ScreenRecorderSettingsDialog.h"

#include "../RecorderUiBinder.h"
#include "../RecorderComboStyle.h"
#include "../include/RecorderPresets.h"

#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <Qt>

namespace
{

void restoreComboByData(QComboBox *combo, const char *settingsKey, int fallback)
{
    if (!combo)
        return;
    const int stored = QSettings().value(QString::fromLatin1(settingsKey), fallback).toInt();
    const int idx = combo->findData(stored);
    if (idx >= 0)
        combo->setCurrentIndex(idx);
}

void saveComboData(QComboBox *combo, const char *settingsKey)
{
    if (!combo)
        return;
    QSettings().setValue(QString::fromLatin1(settingsKey), combo->currentData());
}

} // namespace

ScreenRecorderSettingsDialog::ScreenRecorderSettingsDialog(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ScreenRecorderSettingsDialog)
{
    ui->setupUi(this);

    RecorderUiBinder::fillFrameRateCombo(ui->fpsCombo);
    RecorderUiBinder::fillQualityLevelCombo(ui->qualityCombo);
    RecorderUiBinder::fillEncodeLevelCombo(ui->encodeCombo);
    RecorderUiBinder::fillVideoFormatCombo(ui->formatCombo);
    applyComboTooltips();
    applyComboAppearance();
    applyFormAlignment();

    loadPresetSettings();

    const auto onPreset = [this]() {
        savePresetSettings();
        updatePresetSummary(m_summaryCaptureW, m_summaryCaptureH);
        emit presetChanged();
    };
    connect(ui->fpsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, onPreset);
    connect(ui->qualityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, onPreset);
    connect(ui->encodeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, onPreset);
    connect(ui->formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, onPreset);
}

ScreenRecorderSettingsDialog::~ScreenRecorderSettingsDialog()
{
    delete ui;
}

QComboBox *ScreenRecorderSettingsDialog::qualityCombo() const
{
    return ui->qualityCombo;
}

QComboBox *ScreenRecorderSettingsDialog::encodeCombo() const
{
    return ui->encodeCombo;
}

QComboBox *ScreenRecorderSettingsDialog::fpsCombo() const
{
    return ui->fpsCombo;
}

QComboBox *ScreenRecorderSettingsDialog::formatCombo() const
{
    return ui->formatCombo;
}

QLineEdit *ScreenRecorderSettingsDialog::pathEdit() const
{
    return ui->pathEdit;
}

QPushButton *ScreenRecorderSettingsDialog::browseBtn() const
{
    return ui->browseBtn;
}

void ScreenRecorderSettingsDialog::updatePresetSummary(int captureWidth, int captureHeight)
{
    m_summaryCaptureW = captureWidth;
    m_summaryCaptureH = captureHeight;
    if (!ui->presetSummaryLabel)
        return;

    ui->presetSummaryLabel->setText(
        RecorderUiBinder::presetSummaryText(ui->qualityCombo,
                                            ui->encodeCombo,
                                            ui->fpsCombo,
                                            ui->formatCombo,
                                            captureWidth,
                                            captureHeight));
}

void ScreenRecorderSettingsDialog::setControlsEnabled(bool enabled)
{
    ui->fpsCombo->setEnabled(enabled);
    ui->qualityCombo->setEnabled(enabled);
    ui->encodeCombo->setEnabled(enabled);
    ui->formatCombo->setEnabled(enabled);
    ui->pathEdit->setEnabled(enabled);
    ui->browseBtn->setEnabled(enabled);
}

void ScreenRecorderSettingsDialog::loadPresetSettings()
{
    restoreComboByData(ui->qualityCombo,
                       "recorder/qualityLevel",
                       static_cast<int>(recorder::defaultQualityLevel()));
    restoreComboByData(ui->encodeCombo,
                       "recorder/encodeLevel",
                       static_cast<int>(recorder::defaultEncodeLevel()));
    restoreComboByData(ui->fpsCombo,
                       "recorder/frameRatePreset",
                       static_cast<int>(recorder::defaultFrameRatePreset()));
    restoreComboByData(ui->formatCombo,
                       "recorder/videoFormat",
                       static_cast<int>(recorder::VideoFormat::Mp4));
}

void ScreenRecorderSettingsDialog::savePresetSettings()
{
    saveComboData(ui->qualityCombo, "recorder/qualityLevel");
    saveComboData(ui->encodeCombo, "recorder/encodeLevel");
    saveComboData(ui->fpsCombo, "recorder/frameRatePreset");
    saveComboData(ui->formatCombo, "recorder/videoFormat");
}

void ScreenRecorderSettingsDialog::applyFormAlignment()
{
    if (ui->settingsGrid)
    {
        ui->settingsGrid->setColumnStretch(1, 1);
        ui->settingsGrid->setColumnStretch(3, 1);
        ui->settingsGrid->setColumnMinimumWidth(0, 100);
        ui->settingsGrid->setColumnMinimumWidth(2, 100);
    }

    const char *labelStyle = "color: #212121; font-size: 13px;";
    for (QLabel *label : {ui->fpsLabel,
                          ui->qualityLabel,
                          ui->encodeLabel,
                          ui->formatLabel,
                          ui->pathLabel})
    {
        if (label)
            label->setStyleSheet(QString::fromUtf8(labelStyle));
    }

    if (ui->pathEdit)
    {
        ui->pathEdit->setStyleSheet(
            QStringLiteral("padding: 2px 6px; border: 1px solid #c8c8c8; border-radius: 2px; min-height: 24px;"));
    }
    if (ui->browseBtn)
    {
        ui->browseBtn->setStyleSheet(
            QStringLiteral("padding: 2px 10px; border: 1px solid #c8c8c8; border-radius: 2px; background: #fafafa;"));
    }
}

void ScreenRecorderSettingsDialog::applyComboAppearance()
{
    RecorderComboStyle::applyTo(ui->fpsCombo);
    RecorderComboStyle::applyTo(ui->qualityCombo);
    RecorderComboStyle::applyTo(ui->encodeCombo);
    RecorderComboStyle::applyTo(ui->formatCombo);
}

void ScreenRecorderSettingsDialog::applyComboTooltips()
{
    if (ui->qualityCombo)
    {
        ui->qualityCombo->setToolTip(
            QStringLiteral("选择画质档位；鼠标悬停各项可查看说明。低档会缩小输出尺寸以减小文件。"));
    }
    if (ui->encodeCombo)
        ui->encodeCombo->setToolTip(QStringLiteral("编码精细度：一般更快，精细更清晰。"));
    if (ui->fpsCombo)
        ui->fpsCombo->setToolTip(QStringLiteral("录制帧率；文档演示可用 15 fps，游戏演示可用 30 fps。"));
    if (ui->formatCombo)
        ui->formatCombo->setToolTip(QStringLiteral("MP4 兼容性最好；AVI 为 MJPEG，编辑友好但体积较大。"));
}
