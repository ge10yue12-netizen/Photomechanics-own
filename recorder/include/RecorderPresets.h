#pragma once

#include "RecorderTypes.h"

namespace recorder
{

struct ResolvedVideoParams
{
    int fps = 20;
    int bitrateKbps = 5000;
    int outputWidth = 0;
    int outputHeight = 0;
    EncodeLevel encodeLevel = EncodeLevel::Default;
};

const char *qualityLevelLabel(QualityLevel level);
const char *qualityLevelDescription(QualityLevel level);
const char *encodeLevelLabel(EncodeLevel level);
const char *encodeLevelDescription(EncodeLevel level);
const char *frameRatePresetLabel(FrameRatePreset preset);

int frameRateFromPreset(FrameRatePreset preset);

// 画质档位相对采集分辨率的缩放比（原画/超清/高清=1:1；低档逐级缩小）。
double qualityScaleRatio(QualityLevel level);

void computeOutputDimensions(int captureWidth,
                             int captureHeight,
                             QualityLevel quality,
                             VideoFormat format,
                             int *outputWidth,
                             int *outputHeight);

QualityLevel defaultQualityLevel();
EncodeLevel defaultEncodeLevel();
FrameRatePreset defaultFrameRatePreset();

ResolvedVideoParams resolveVideoPresets(QualityLevel quality,
                                        EncodeLevel encode,
                                        FrameRatePreset fpsPreset,
                                        int captureWidth,
                                        int captureHeight,
                                        VideoFormat format = VideoFormat::Mp4);

// 编码级别量化（MP4=GOP/峰值码率，AVI=JPEG 质量；与编码器共用）。
int encodeGopMultiplier(EncodeLevel level);
double encodeMaxBitrateFactor(EncodeLevel level);
int encodeGopSize(EncodeLevel level, int fps);
int encodeMaxBitrateKbps(EncodeLevel level, int meanBitrateKbps);
int encodeMjpegQuality(EncodeLevel level, int meanBitrateKbps);

} // namespace recorder
