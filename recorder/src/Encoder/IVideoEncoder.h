#pragma once

#include "../Capture/IScreenCapture.h"
#include <string>

namespace recorder
{

// 编码器打开参数。
struct EncoderOpenParams
{
    std::string filePath;
    VideoFormat format = VideoFormat::Avi;
    int width = 0;
    int height = 0;
    int fps = 30;
    int bitrateKbps = 8000;
    EncodeLevel encodeLevel = EncodeLevel::Default;
};

// 视频编码抽象；FFmpeg 等实现仅出现在 .cpp。
class IVideoEncoder
{
public:
    virtual ~IVideoEncoder() = default;

    // 创建输出文件与编码上下文。
    virtual bool open(const EncoderOpenParams &params, std::string *error) = 0;

    // 写入一帧 BGRA；timelineSlots 为本次 sample 覆盖的时间槽数（静止合并时 >1，EV 式 VFR）。
    virtual bool writeFrame(const CaptureFrame &frame, std::string *error, int timelineSlots = 1) = 0;

    // 复用上一帧像素写入时间轴（停止前 flush 静止累积）。
    virtual bool writeCachedTimeline(std::string *error, int timelineSlots) = 0;

    // 关闭文件并释放资源。
    virtual bool close(std::string *error) = 0;
};

} // namespace recorder
