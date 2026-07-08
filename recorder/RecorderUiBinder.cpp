#include "RecorderUiBinder.h"

#include "include/RecorderPresets.h"
#include "src/Capture/CaptureCommon.h"

#include <QComboBox>
#include <QGuiApplication>
#include <QLineEdit>
#include <QScreen>
#include <QSize>
#include <Qt>

recorder::RecorderConfig RecorderUiBinder::configFromWidgets(QComboBox *modeCombo,
                                                             QComboBox *qualityCombo,
                                                             QComboBox *encodeCombo,
                                                             QComboBox *fpsCombo,
                                                             QComboBox *formatCombo,
                                                             const QLineEdit *pathEdit,
                                                             const recorder::Rect &region,
                                                             int captureWidth,
                                                             int captureHeight)
{
    recorder::RecorderConfig cfg;
    if (modeCombo)
        cfg.mode = static_cast<recorder::CaptureMode>(modeCombo->currentData().toInt());

    const auto quality = qualityCombo
                             ? static_cast<recorder::QualityLevel>(qualityCombo->currentData().toInt())
                             : recorder::defaultQualityLevel();
    const auto encode = encodeCombo
                            ? static_cast<recorder::EncodeLevel>(encodeCombo->currentData().toInt())
                            : recorder::defaultEncodeLevel();
    const auto fpsPreset = fpsCombo
                               ? static_cast<recorder::FrameRatePreset>(fpsCombo->currentData().toInt())
                               : recorder::defaultFrameRatePreset();

    const auto format = formatCombo
                            ? static_cast<recorder::VideoFormat>(formatCombo->currentData().toInt())
                            : recorder::VideoFormat::Mp4;

    const recorder::ResolvedVideoParams resolved = recorder::resolveVideoPresets(
        quality, encode, fpsPreset, captureWidth, captureHeight, format);
    cfg.video.fps = resolved.fps;
    cfg.video.bitrateKbps = resolved.bitrateKbps;
    cfg.video.encodeLevel = resolved.encodeLevel;
    cfg.video.outputWidth = resolved.outputWidth;
    cfg.video.outputHeight = resolved.outputHeight;

    if (formatCombo)
        cfg.output.format = format;
    if (pathEdit)
        cfg.output.filePath = pathEdit->text().trimmed().toStdString();
    cfg.region = region;
    return cfg;
}

void RecorderUiBinder::fillCaptureModeCombo(QComboBox *combo)
{
    if (!combo)
        return;
    combo->clear();
    combo->addItem(QString::fromUtf8(recorder::captureModeLabel(recorder::CaptureMode::FullScreen)),
                   static_cast<int>(recorder::CaptureMode::FullScreen));
    combo->addItem(QString::fromUtf8(recorder::captureModeLabel(recorder::CaptureMode::Region)),
                   static_cast<int>(recorder::CaptureMode::Region));
}

void RecorderUiBinder::fillQualityLevelCombo(QComboBox *combo)
{
    if (!combo)
        return;
    combo->clear();
    const recorder::QualityLevel levels[] = {
        recorder::QualityLevel::Original,
        recorder::QualityLevel::UltraClear,
        recorder::QualityLevel::HD,
        recorder::QualityLevel::Clear,
        recorder::QualityLevel::Normal,
        recorder::QualityLevel::Low,
    };
    for (recorder::QualityLevel level : levels)
    {
        combo->addItem(QString::fromUtf8(recorder::qualityLevelLabel(level)),
                       static_cast<int>(level));
        const int idx = combo->count() - 1;
        combo->setItemData(idx,
                           QString::fromUtf8(recorder::qualityLevelDescription(level)),
                           Qt::ToolTipRole);
    }
}

void RecorderUiBinder::fillEncodeLevelCombo(QComboBox *combo)
{
    if (!combo)
        return;
    combo->clear();
    const recorder::EncodeLevel levels[] = {
        recorder::EncodeLevel::General,
        recorder::EncodeLevel::Default,
        recorder::EncodeLevel::Fine,
    };
    for (recorder::EncodeLevel level : levels)
    {
        combo->addItem(QString::fromUtf8(recorder::encodeLevelLabel(level)),
                       static_cast<int>(level));
        const int idx = combo->count() - 1;
        combo->setItemData(idx,
                           QString::fromUtf8(recorder::encodeLevelDescription(level)),
                           Qt::ToolTipRole);
    }
}

void RecorderUiBinder::fillFrameRateCombo(QComboBox *combo)
{
    if (!combo)
        return;
    combo->clear();
    const recorder::FrameRatePreset presets[] = {
        recorder::FrameRatePreset::Smooth,
        recorder::FrameRatePreset::Standard,
        recorder::FrameRatePreset::High,
    };
    for (recorder::FrameRatePreset preset : presets)
    {
        combo->addItem(QString::fromUtf8(recorder::frameRatePresetLabel(preset)),
                       static_cast<int>(preset));
    }
}

void RecorderUiBinder::fillVideoFormatCombo(QComboBox *combo)
{
    if (!combo)
        return;
    combo->clear();
    combo->addItem(QStringLiteral(".mp4"), static_cast<int>(recorder::VideoFormat::Mp4));
    combo->addItem(QStringLiteral(".avi"), static_cast<int>(recorder::VideoFormat::Avi));
}

QString RecorderUiBinder::validateForUi(const recorder::RecorderConfig &config)
{
    recorder::RecorderErrorCode code = recorder::RecorderErrorCode::None;
    std::string message;
    if (!recorder::validateConfig(config, &code, &message))
        return QString::fromStdString(message);
    return QString();
}

void RecorderUiBinder::setCaptureMode(QComboBox *combo, recorder::CaptureMode mode)
{
    if (!combo)
        return;
    const int idx = combo->findData(static_cast<int>(mode));
    if (idx >= 0)
        combo->setCurrentIndex(idx);
}

void RecorderUiBinder::setVideoFormat(QComboBox *combo, recorder::VideoFormat format)
{
    if (!combo)
        return;
    const int idx = combo->findData(static_cast<int>(format));
    if (idx >= 0)
        combo->setCurrentIndex(idx);
}

void RecorderUiBinder::setQualityLevel(QComboBox *combo, recorder::QualityLevel level)
{
    if (!combo)
        return;
    const int idx = combo->findData(static_cast<int>(level));
    if (idx >= 0)
        combo->setCurrentIndex(idx);
}

void RecorderUiBinder::setEncodeLevel(QComboBox *combo, recorder::EncodeLevel level)
{
    if (!combo)
        return;
    const int idx = combo->findData(static_cast<int>(level));
    if (idx >= 0)
        combo->setCurrentIndex(idx);
}

void RecorderUiBinder::setFrameRatePreset(QComboBox *combo, recorder::FrameRatePreset preset)
{
    if (!combo)
        return;
    const int idx = combo->findData(static_cast<int>(preset));
    if (idx >= 0)
        combo->setCurrentIndex(idx);
}

recorder::CaptureMode RecorderUiBinder::previewCaptureMode(recorder::CaptureMode selected, bool regionValid)
{
    if (selected == recorder::CaptureMode::Region && regionValid)
        return recorder::CaptureMode::Region;
    if (selected == recorder::CaptureMode::Window)
        return recorder::CaptureMode::Window;
    return recorder::CaptureMode::FullScreen;
}

namespace
{

int evenDim(int value)
{
    return value > 0 ? (value & ~1) : value;
}

} // namespace

QSize RecorderUiBinder::captureSourceSize(recorder::CaptureMode mode,
                                          const recorder::Rect &region,
                                          bool regionValid,
                                          std::uintptr_t windowHandle)
{
    if (mode == recorder::CaptureMode::Region && regionValid && region.width >= 100 && region.height >= 100)
        return QSize(evenDim(region.width), evenDim(region.height));

    if (mode == recorder::CaptureMode::Window && windowHandle != 0)
    {
        int w = 0;
        int h = 0;
        if (recorder::capture::windowClientSize(windowHandle, &w, &h, nullptr))
            return QSize(evenDim(w), evenDim(h));
    }

    const recorder::Rect screen = recorder::capture::primaryScreenRect();
    if (screen.width >= 100 && screen.height >= 100)
        return QSize(evenDim(screen.width), evenDim(screen.height));

    if (QGuiApplication::instance())
    {
        if (QScreen *screen = QGuiApplication::primaryScreen())
        {
            const QSize sz = screen->geometry().size();
            return QSize(evenDim(sz.width()), evenDim(sz.height()));
        }
    }
    return QSize(1920, 1080);
}

QString RecorderUiBinder::presetSummaryText(QComboBox *qualityCombo,
                                            QComboBox *encodeCombo,
                                            QComboBox *fpsCombo,
                                            QComboBox *formatCombo,
                                            int captureWidth,
                                            int captureHeight)
{
    const auto quality = qualityCombo
                             ? static_cast<recorder::QualityLevel>(qualityCombo->currentData().toInt())
                             : recorder::defaultQualityLevel();
    const auto encode = encodeCombo
                            ? static_cast<recorder::EncodeLevel>(encodeCombo->currentData().toInt())
                            : recorder::defaultEncodeLevel();
    const auto fpsPreset = fpsCombo
                               ? static_cast<recorder::FrameRatePreset>(fpsCombo->currentData().toInt())
                               : recorder::defaultFrameRatePreset();
    const auto format = formatCombo
                            ? static_cast<recorder::VideoFormat>(formatCombo->currentData().toInt())
                            : recorder::VideoFormat::Mp4;

    const recorder::ResolvedVideoParams resolved =
        recorder::resolveVideoPresets(quality, encode, fpsPreset, captureWidth, captureHeight, format);

    const QString sizePart =
        QStringLiteral("%1 × %2").arg(resolved.outputWidth).arg(resolved.outputHeight);

    const QString formatExt =
        format == recorder::VideoFormat::Avi ? QStringLiteral("AVI") : QStringLiteral("MP4");

    QString encodePart;
    if (format == recorder::VideoFormat::Avi)
    {
        encodePart = QStringLiteral("JPEG 质量 %1")
                         .arg(recorder::encodeMjpegQuality(encode, resolved.bitrateKbps));
    }
    else
    {
        encodePart = QStringLiteral("GOP %1 · 峰值 %2 kbps")
                         .arg(recorder::encodeGopSize(encode, resolved.fps))
                         .arg(recorder::encodeMaxBitrateKbps(encode, resolved.bitrateKbps));
    }

    return QStringLiteral("实际输出：%1 · %2 fps · 约 %3 kbps · %4 · %5")
        .arg(sizePart)
        .arg(resolved.fps)
        .arg(resolved.bitrateKbps)
        .arg(formatExt)
        .arg(encodePart);
}
