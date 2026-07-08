#include "../include/RecorderError.h"

#include "../include/IRecorderWindowTarget.h"
#include "../include/VisualFrameCache.h"
#include "Capture/CaptureCommon.h"

#include <algorithm>
#include <cctype>

namespace recorder
{
namespace
{

bool hasSupportedVideoExtension(const std::string &path, VideoFormat format)
{
    const auto dot = path.find_last_of('.');
    if (dot == std::string::npos)
        return false;
    std::string ext = path.substr(dot + 1);
    for (char &ch : ext)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

    switch (format)
    {
    case VideoFormat::Avi:
        return ext == "avi";
    case VideoFormat::Wmv:
        return ext == "wmv";
    case VideoFormat::Rm:
        return ext == "rm";
    case VideoFormat::Rmvb:
        return ext == "rmvb";
    case VideoFormat::Mp4:
        return ext == "mp4";
    }
    return false;
}

} // namespace

bool isFormatSupported(VideoFormat format)
{
    return format == VideoFormat::Mp4 || format == VideoFormat::Avi;
}

const char *errorMessage(RecorderErrorCode code)
{
    switch (code)
    {
    case RecorderErrorCode::None:
        return "无错误";
    case RecorderErrorCode::InvalidConfig:
        return "录制参数无效，请检查帧率、分辨率与保存路径。";
    case RecorderErrorCode::InvalidRegion:
        return "录制区域无效，请重新选择录制区域。";
    case RecorderErrorCode::InvalidWindowTarget:
        return "窗口录制目标无效，请先打开相机并显示预览。";
    case RecorderErrorCode::InvalidOutputPath:
        return "保存路径无效或无法写入，请更换目录后重试。";
    case RecorderErrorCode::UnsupportedFormat:
        return "当前版本暂不支持该导出格式，请选择 MP4 或 AVI。";
    case RecorderErrorCode::CaptureInitFailed:
        return "屏幕采集初始化失败，请检查录制区域是否超出屏幕。";
    case RecorderErrorCode::EncoderInitFailed:
        return "视频编码器初始化失败，请检查导出格式或文件路径。";
    case RecorderErrorCode::WriteFileFailed:
        return "写入视频文件失败，请检查磁盘空间与写入权限。";
    case RecorderErrorCode::ThreadStartFailed:
        return "录制线程启动失败。";
    case RecorderErrorCode::InvalidState:
        return "当前状态不允许此操作。";
    case RecorderErrorCode::Unknown:
    default:
        return "未知错误。";
    }
}

const char *stateLabel(RecorderState state)
{
    switch (state)
    {
    case RecorderState::Idle:
        return "未开始";
    case RecorderState::Initialized:
        return "已就绪";
    case RecorderState::Recording:
        return "录制中";
    case RecorderState::Paused:
        return "已暂停";
    case RecorderState::Stopped:
        return "已停止";
    case RecorderState::Error:
        return "错误";
    }
    return "—";
}

const char *captureModeLabel(CaptureMode mode)
{
    switch (mode)
    {
    case CaptureMode::FullScreen:
        return "主屏录制";
    case CaptureMode::Region:
        return "区域录制";
    case CaptureMode::Window:
        return "窗口录制";
    }
    return "—";
}

const char *videoFormatLabel(VideoFormat format)
{
    switch (format)
    {
    case VideoFormat::Avi:
        return "AVI";
    case VideoFormat::Wmv:
        return "WMV";
    case VideoFormat::Rm:
        return "RM";
    case VideoFormat::Rmvb:
        return "RMVB";
    case VideoFormat::Mp4:
        return "MP4";
    }
    return "—";
}

bool validateConfig(const RecorderConfig &config,
                    RecorderErrorCode *code,
                    std::string *message)
{
    auto fail = [&](RecorderErrorCode c) {
        if (code)
            *code = c;
        if (message)
            *message = errorMessage(c);
        return false;
    };

    if (config.video.fps < 1 || config.video.fps > 60)
        return fail(RecorderErrorCode::InvalidConfig);

    if (config.video.bitrateKbps < 500)
        return fail(RecorderErrorCode::InvalidConfig);

    if (config.output.filePath.empty())
        return fail(RecorderErrorCode::InvalidOutputPath);

    if (!hasSupportedVideoExtension(config.output.filePath, config.output.format))
        return fail(RecorderErrorCode::InvalidOutputPath);

    if (!isFormatSupported(config.output.format))
        return fail(RecorderErrorCode::UnsupportedFormat);

    if (config.mode == CaptureMode::Region)
    {
        if (config.region.width < 100 || config.region.height < 100)
            return fail(RecorderErrorCode::InvalidRegion);
    }

    if (config.mode == CaptureMode::Window)
    {
        if (config.windowTarget.provider && config.windowTarget.provider->visualCache())
        {
            if (!config.windowTarget.provider->visualCache()->hasFrame())
                return fail(RecorderErrorCode::InvalidWindowTarget);
        }
        else if (!capture::isValidWindowHandle(config.windowTarget.nativeHandle) ||
                 !capture::isWindowCapturable(config.windowTarget.nativeHandle))
        {
            return fail(RecorderErrorCode::InvalidWindowTarget);
        }
    }

    if (config.video.outputWidth != 0 &&
        (config.video.outputWidth < 100 || config.video.outputHeight < 100))
    {
        return fail(RecorderErrorCode::InvalidConfig);
    }

    if (code)
        *code = RecorderErrorCode::None;
    if (message)
        message->clear();
    return true;
}

int suggestScreenBitrateKbps(int width, int height, int fps)
{
    if (width < 100 || height < 100)
        return 3000;
    const int fp = fps > 0 ? fps : 20;
    const long long pixels = static_cast<long long>(width) * static_cast<long long>(height);
    // 屏录约 0.08 bpp/frame；VBR + 静止跳帧下实际远低于均值。
    int kbps = static_cast<int>((pixels * fp * 8) / 100000);
    if (kbps < 2000)
        kbps = 2000;
    if (kbps > 50000)
        kbps = 50000;
    return kbps;
}

int minimumScreenBitrateKbps(int width, int height, int fps)
{
    (void)fps;
    if (width < 100 || height < 100)
        return 1200;
    const long long pixels = static_cast<long long>(width) * static_cast<long long>(height);
    int kbps = static_cast<int>(pixels / 500000);
    if (kbps < 1200)
        kbps = 1200;
    if (kbps > 4000)
        kbps = 4000;
    return kbps;
}

int effectiveScreenBitrateKbps(int userKbps, int width, int height, int fps)
{
    const int suggested = suggestScreenBitrateKbps(width, height, fps);
    if (userKbps < 500)
        return suggested;

    const int minKbps = minimumScreenBitrateKbps(width, height, fps);
    if (userKbps < minKbps)
        return minKbps;
    return userKbps;
}

} // namespace recorder
