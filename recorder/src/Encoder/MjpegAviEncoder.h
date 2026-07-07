#pragma once

#include "IVideoEncoder.h"

namespace recorder
{

// MJPEG-in-AVI 编码（Windows WIC + 手写 RIFF；仅支持 Avi 格式）。
class MjpegAviEncoder : public IVideoEncoder
{
public:
    MjpegAviEncoder();
    ~MjpegAviEncoder() override;

    bool open(const EncoderOpenParams &params, std::string *error) override;
    bool writeFrame(const CaptureFrame &frame, std::string *error) override;
    bool close(std::string *error) override;

private:
    class Impl;
    Impl *m_impl;
};

} // namespace recorder
