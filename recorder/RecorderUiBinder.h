#pragma once

#include "RecorderKit.h"

#include <QSize>

class QComboBox;
class QSpinBox;
class QLineEdit;
class QString;

// 录屏参数与 Qt 控件的双向绑定（无 UI 布局，便于移植到 ComboBox / TableWidget 宿主）。
class RecorderUiBinder
{
public:
    // 填充采集模式下拉；itemData 为 CaptureMode 整型值。
    static void fillCaptureModeCombo(QComboBox *combo);

    // 填充导出格式下拉；不支持的格式标记为 disabled。
    static void fillVideoFormatCombo(QComboBox *combo);

    // 从控件读取配置；region 由区域选择步骤提供。
    static recorder::RecorderConfig configFromWidgets(QComboBox *modeCombo,
                                                      QSpinBox *fpsSpin,
                                                      QSpinBox *widthSpin,
                                                      QSpinBox *heightSpin,
                                                      QSpinBox *bitrateSpin,
                                                      QComboBox *formatCombo,
                                                      const QLineEdit *pathEdit,
                                                      const recorder::Rect &region);

    // 校验配置并返回中文错误；空串表示通过。
    static QString validateForUi(const recorder::RecorderConfig &config);

    // 将 CaptureMode 写入 combo 当前项。
    static void setCaptureMode(QComboBox *combo, recorder::CaptureMode mode);

    // 将 VideoFormat 写入 combo 当前项。
    static void setVideoFormat(QComboBox *combo, recorder::VideoFormat format);

    // 预览采集模式：区域模式且已选区时预览区域，否则主屏。
    static recorder::CaptureMode previewCaptureMode(recorder::CaptureMode selected, bool regionValid);

    // 当前录制区域的原始像素尺寸（主屏或选区；宽高压成偶数以利 H.264）。
    static QSize captureSourceSize(recorder::CaptureMode mode,
                                   const recorder::Rect &region,
                                   bool regionValid);
};
