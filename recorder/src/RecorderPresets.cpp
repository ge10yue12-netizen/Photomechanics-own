#include "../include/RecorderPresets.h"

#include "../include/RecorderError.h"

namespace recorder
{

namespace
{

int fallbackCaptureDim(int value, int fallback)
{
    return value >= 100 ? value : fallback;
}

int evenDim(int value)
{
    return value > 0 ? (value & ~1) : value;
}

} // namespace

const char *qualityLevelLabel(QualityLevel level)
{
    switch (level)
    {
    case QualityLevel::Original:
        return "原画";
    case QualityLevel::UltraClear:
        return "超清";
    case QualityLevel::HD:
        return "高清";
    case QualityLevel::Clear:
        return "清晰";
    case QualityLevel::Normal:
        return "普清";
    case QualityLevel::Low:
        return "一般";
    }
    return "超清";
}

const char *qualityLevelDescription(QualityLevel level)
{
    switch (level)
    {
    case QualityLevel::Original:
        return "采集原生分辨率，最高码率，文件较大，适合后期剪辑。";
    case QualityLevel::UltraClear:
        return "采集原生分辨率，推荐码率，画质与体积均衡（推荐）。";
    case QualityLevel::HD:
        return "采集原生分辨率，适中码率，日常分享。";
    case QualityLevel::Clear:
        return "略缩小输出尺寸并降低码率，适合长时间录制。";
    case QualityLevel::Normal:
        return "明显缩小输出尺寸，文件更小，适合文档/课件。";
    case QualityLevel::Low:
        return "最小输出尺寸与码率，预览/存档用途。";
    }
    return "推荐码率，画质与体积均衡。";
}

const char *encodeLevelLabel(EncodeLevel level)
{
    switch (level)
    {
    case EncodeLevel::General:
        return "一般";
    case EncodeLevel::Default:
        return "默认";
    case EncodeLevel::Fine:
        return "精细";
    }
    return "默认";
}

const char *encodeLevelDescription(EncodeLevel level)
{
    switch (level)
    {
    case EncodeLevel::General:
        return "编码更快，CPU 占用更低，快速动效略软。";
    case EncodeLevel::Default:
        return "速度与画质平衡，适合大多数场景。";
    case EncodeLevel::Fine:
        return "细节更好，静止画面更清晰，CPU 占用略高。";
    }
    return "速度与画质平衡。";
}

const char *frameRatePresetLabel(FrameRatePreset preset)
{
    switch (preset)
    {
    case FrameRatePreset::Smooth:
        return "流畅 (15 fps)";
    case FrameRatePreset::Standard:
        return "标准 (20 fps)";
    case FrameRatePreset::High:
        return "高帧 (30 fps)";
    }
    return "标准 (20 fps)";
}

int frameRateFromPreset(FrameRatePreset preset)
{
    switch (preset)
    {
    case FrameRatePreset::Smooth:
        return 15;
    case FrameRatePreset::Standard:
        return 20;
    case FrameRatePreset::High:
        return 30;
    }
    return 20;
}

double qualityScaleRatio(QualityLevel level)
{
    switch (level)
    {
    case QualityLevel::Original:
    case QualityLevel::UltraClear:
    case QualityLevel::HD:
        return 1.0;
    case QualityLevel::Clear:
        return 0.85;
    case QualityLevel::Normal:
        return 0.70;
    case QualityLevel::Low:
        return 0.50;
    }
    return 1.0;
}

void computeOutputDimensions(int captureWidth,
                             int captureHeight,
                             QualityLevel quality,
                             VideoFormat format,
                             int *outputWidth,
                             int *outputHeight)
{
    const int capW = fallbackCaptureDim(captureWidth, 1920);
    const int capH = fallbackCaptureDim(captureHeight, 1080);
    const double ratio = qualityScaleRatio(quality);

    int outW = static_cast<int>(static_cast<double>(capW) * ratio + 0.5);
    int outH = static_cast<int>(static_cast<double>(capH) * ratio + 0.5);

    if (outW < 100 || outH < 100)
    {
        outW = capW;
        outH = capH;
    }

    if (format == VideoFormat::Mp4)
    {
        outW = evenDim(outW);
        outH = evenDim(outH);
    }

    if (outputWidth)
        *outputWidth = outW;
    if (outputHeight)
        *outputHeight = outH;
}

QualityLevel defaultQualityLevel()
{
    return QualityLevel::UltraClear;
}

EncodeLevel defaultEncodeLevel()
{
    return EncodeLevel::Default;
}

FrameRatePreset defaultFrameRatePreset()
{
    return FrameRatePreset::Standard;
}

ResolvedVideoParams resolveVideoPresets(QualityLevel quality,
                                      EncodeLevel encode,
                                      FrameRatePreset fpsPreset,
                                      int captureWidth,
                                      int captureHeight,
                                      VideoFormat format)
{
    ResolvedVideoParams out;
    out.encodeLevel = encode;
    out.fps = frameRateFromPreset(fpsPreset);

    const int capW = fallbackCaptureDim(captureWidth, 1920);
    const int capH = fallbackCaptureDim(captureHeight, 1080);
    computeOutputDimensions(capW, capH, quality, format, &out.outputWidth, &out.outputHeight);

    const int baseKbps = suggestScreenBitrateKbps(out.outputWidth, out.outputHeight, out.fps);

    int targetKbps = baseKbps;
    switch (quality)
    {
    case QualityLevel::Original:
        targetKbps = static_cast<int>(static_cast<long long>(baseKbps) * 120 / 100);
        if (targetKbps > 50000)
            targetKbps = 50000;
        break;
    case QualityLevel::UltraClear:
        targetKbps = baseKbps;
        break;
    case QualityLevel::HD:
        targetKbps = static_cast<int>(static_cast<long long>(baseKbps) * 85 / 100);
        break;
    case QualityLevel::Clear:
        targetKbps = static_cast<int>(static_cast<long long>(baseKbps) * 70 / 100);
        break;
    case QualityLevel::Normal:
        targetKbps = static_cast<int>(static_cast<long long>(baseKbps) * 55 / 100);
        break;
    case QualityLevel::Low:
        targetKbps = minimumScreenBitrateKbps(out.outputWidth, out.outputHeight, out.fps);
        break;
    }

    out.bitrateKbps =
        effectiveScreenBitrateKbps(targetKbps, out.outputWidth, out.outputHeight, out.fps);
    return out;
}

int encodeGopMultiplier(EncodeLevel level)
{
    switch (level)
    {
    case EncodeLevel::General:
        return 3;
    case EncodeLevel::Fine:
        return 1;
    case EncodeLevel::Default:
    default:
        return 2;
    }
}

double encodeMaxBitrateFactor(EncodeLevel level)
{
    switch (level)
    {
    case EncodeLevel::General:
        return 1.5;
    case EncodeLevel::Fine:
        return 2.5;
    case EncodeLevel::Default:
    default:
        return 2.0;
    }
}

int encodeGopSize(EncodeLevel level, int fps)
{
    const int fp = fps > 0 ? fps : 20;
    return fp * encodeGopMultiplier(level);
}

int encodeMaxBitrateKbps(EncodeLevel level, int meanBitrateKbps)
{
    if (meanBitrateKbps <= 0)
        return 0;
    return static_cast<int>(static_cast<double>(meanBitrateKbps) * encodeMaxBitrateFactor(level) + 0.5);
}

int encodeMjpegQuality(EncodeLevel level, int meanBitrateKbps)
{
    int quality = meanBitrateKbps / 100;
    if (quality < 50)
        quality = 50;
    if (quality > 95)
        quality = 95;

    switch (level)
    {
    case EncodeLevel::General:
        quality -= 10;
        break;
    case EncodeLevel::Fine:
        quality += 5;
        break;
    case EncodeLevel::Default:
    default:
        break;
    }

    if (quality < 50)
        quality = 50;
    if (quality > 95)
        quality = 95;
    return quality;
}

} // namespace recorder
