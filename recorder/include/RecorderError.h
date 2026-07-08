#pragma once

#include "RecorderTypes.h"
#include <string>

namespace recorder
{

// 模块内部错误码。
enum class RecorderErrorCode
{
    None = 0,
    InvalidConfig,
    InvalidRegion,
    InvalidWindowTarget,
    InvalidOutputPath,
    UnsupportedFormat,
    CaptureInitFailed,
    EncoderInitFailed,
    WriteFileFailed,
    ThreadStartFailed,
    InvalidState,
    Unknown
};

// 校验配置；失败时写入 code 与 message（中文）。
bool validateConfig(const RecorderConfig &config,
                    RecorderErrorCode *code,
                    std::string *message);

// 将错误码映射为默认中文说明。
const char *errorMessage(RecorderErrorCode code);

// 将状态枚举映射为中文标签。
const char *stateLabel(RecorderState state);

// 将采集模式映射为中文标签。
const char *captureModeLabel(CaptureMode mode);

// 将导出格式映射为中文标签。
const char *videoFormatLabel(VideoFormat format);

// 判断当前构建是否支持该导出格式。
bool isFormatSupported(VideoFormat format);

// 屏幕录制建议码率（kbps），按像素与帧率估算。
int suggestScreenBitrateKbps(int width, int height, int fps);

// 解析有效码率：尊重用户设置，仅对过低值做安全下限钳制。
int effectiveScreenBitrateKbps(int userKbps, int width, int height, int fps);

// 编码器允许的绝对最低码率（kbps）。
int minimumScreenBitrateKbps(int width, int height, int fps);

} // namespace recorder
