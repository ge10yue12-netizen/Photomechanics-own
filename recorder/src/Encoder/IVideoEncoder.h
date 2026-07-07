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
};

// 视频编码抽象；FFmpeg 等实现仅出现在 .cpp。
class IVideoEncoder
{
public:
    virtual ~IVideoEncoder() = default;

    // 创建输出文件与编码上下文。
    virtual bool open(const EncoderOpenParams &params, std::string *error) = 0;

    // 写入一帧 BGRA；内部负责缩放与色彩转换。
    virtual bool writeFrame(const CaptureFrame &frame, std::string *error) = 0;

    // 关闭文件并释放资源。
    virtual bool close(std::string *error) = 0;
};

} // namespace recorder
