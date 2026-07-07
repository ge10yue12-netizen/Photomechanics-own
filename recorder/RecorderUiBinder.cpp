#include "RecorderUiBinder.h"

#include "src/Capture/CaptureCommon.h"

#include <QComboBox>
#include <QGuiApplication>
#include <QLineEdit>
#include <QScreen>
#include <QSize>
#include <QSpinBox>
#include <QStandardItemModel>

recorder::RecorderConfig RecorderUiBinder::configFromWidgets(QComboBox *modeCombo,
                                                             QSpinBox *fpsSpin,
                                                             QSpinBox *widthSpin,
                                                             QSpinBox *heightSpin,
                                                             QSpinBox *bitrateSpin,
                                                             QComboBox *formatCombo,
                                                             const QLineEdit *pathEdit,
                                                             const recorder::Rect &region)
{
    recorder::RecorderConfig cfg;
    if (modeCombo)
        cfg.mode = static_cast<recorder::CaptureMode>(modeCombo->currentData().toInt());
    if (fpsSpin)
        cfg.video.fps = fpsSpin->value();
    if (widthSpin)
        cfg.video.outputWidth = widthSpin->value();
    if (heightSpin)
        cfg.video.outputHeight = heightSpin->value();
    if (bitrateSpin)
        cfg.video.bitrateKbps = bitrateSpin->value();
    if (formatCombo)
        cfg.output.format = static_cast<recorder::VideoFormat>(formatCombo->currentData().toInt());
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

void RecorderUiBinder::fillVideoFormatCombo(QComboBox *combo)
{
    if (!combo)
        return;
    combo->clear();
    const recorder::VideoFormat formats[] = {
        recorder::VideoFormat::Mp4,
        recorder::VideoFormat::Avi,
        recorder::VideoFormat::Wmv,
        recorder::VideoFormat::Rm,
        recorder::VideoFormat::Rmvb,
    };
    for (recorder::VideoFormat fmt : formats)
    {
        combo->addItem(QString::fromUtf8(recorder::videoFormatLabel(fmt)), static_cast<int>(fmt));
        if (!recorder::isFormatSupported(fmt))
        {
            QStandardItemModel *model = qobject_cast<QStandardItemModel *>(combo->model());
            if (model)
            {
                QStandardItem *item = model->item(combo->count() - 1);
                if (item)
                    item->setEnabled(false);
            }
        }
    }
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

recorder::CaptureMode RecorderUiBinder::previewCaptureMode(recorder::CaptureMode selected, bool regionValid)
{
    if (selected == recorder::CaptureMode::Region && regionValid)
        return recorder::CaptureMode::Region;
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
                                          bool regionValid)
{
    if (mode == recorder::CaptureMode::Region && regionValid && region.width >= 100 && region.height >= 100)
        return QSize(evenDim(region.width), evenDim(region.height));

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
