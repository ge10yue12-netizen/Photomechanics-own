#pragma once

#include "RecorderKit.h"

#include <QSize>

class QComboBox;
class QLineEdit;
class QString;

// 录屏参数与 Qt 控件的双向绑定（无 UI 布局，可接入自定义界面）。
class RecorderUiBinder
{
public:
    static void fillCaptureModeCombo(QComboBox *combo);
    static void fillQualityLevelCombo(QComboBox *combo);
    static void fillEncodeLevelCombo(QComboBox *combo);
    static void fillFrameRateCombo(QComboBox *combo);
    static void fillVideoFormatCombo(QComboBox *combo);

    static recorder::RecorderConfig configFromWidgets(QComboBox *modeCombo,
                                                      QComboBox *qualityCombo,
                                                      QComboBox *encodeCombo,
                                                      QComboBox *fpsCombo,
                                                      QComboBox *formatCombo,
                                                      const QLineEdit *pathEdit,
                                                      const recorder::Rect &region,
                                                      int captureWidth,
                                                      int captureHeight);

    static QString validateForUi(const recorder::RecorderConfig &config);

    static void setCaptureMode(QComboBox *combo, recorder::CaptureMode mode);
    static void setVideoFormat(QComboBox *combo, recorder::VideoFormat format);
    static void setQualityLevel(QComboBox *combo, recorder::QualityLevel level);
    static void setEncodeLevel(QComboBox *combo, recorder::EncodeLevel level);
    static void setFrameRatePreset(QComboBox *combo, recorder::FrameRatePreset preset);

    static recorder::CaptureMode previewCaptureMode(recorder::CaptureMode selected, bool regionValid);

    static QSize captureSourceSize(recorder::CaptureMode mode,
                                   const recorder::Rect &region,
                                   bool regionValid,
                                   std::uintptr_t windowHandle = 0);

    static QString presetSummaryText(QComboBox *qualityCombo,
                                     QComboBox *encodeCombo,
                                     QComboBox *fpsCombo,
                                     QComboBox *formatCombo,
                                     int captureWidth,
                                     int captureHeight);
};
