#pragma once

#include <functional>
#include <string>

namespace recorder
{

// 采集范围模式。
enum class CaptureMode
{
    FullScreen = 0,
    Region = 1
};

// 导出容器格式；MP4/AVI 可用，其余在 Init 时返回明确错误。
enum class VideoFormat
{
    Avi = 0,
    Wmv = 1,
    Rm = 2,
    Rmvb = 3,
    Mp4 = 4
};

// 录制会话状态。
enum class RecorderState
{
    Idle = 0,
    Initialized = 1,
    Recording = 2,
    Paused = 3,
    Stopped = 4,
    Error = 5
};

// 屏幕矩形（像素，不含平台类型）。
struct Rect
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

// 视频编码参数。
struct VideoParams
{
    int fps = 25;
    int bitrateKbps = 10000;
    int outputWidth = 0;  // 0 表示跟随采集区域宽
    int outputHeight = 0; // 0 表示跟随采集区域高
};

// 输出文件参数。
struct OutputParams
{
    std::string filePath;
    VideoFormat format = VideoFormat::Mp4;
};

// 录制配置；Init 前由宿主或 UI 填充。
struct RecorderConfig
{
    CaptureMode mode = CaptureMode::FullScreen;
    Rect region;
    VideoParams video;
    OutputParams output;
};

// 状态/时长/错误回调；可能在后台线程触发，Qt 宿主须转主线程。
struct RecorderCallback
{
    std::function<void(RecorderState)> onStateChanged;
    std::function<void(int seconds)> onDurationChanged;
    std::function<void(const std::string &message)> onError;
    std::function<void(const std::string &message)> onLog;
};

} // namespace recorder
