#pragma once

#include "IVideoEncoder.h"

namespace recorder
{

// MP4/H.264 编码（Windows Media Foundation Sink Writer；仅支持 Mp4 格式）。
class Mp4MfEncoder : public IVideoEncoder
{
public:
    Mp4MfEncoder();
    ~Mp4MfEncoder() override;

    bool open(const EncoderOpenParams &params, std::string *error) override;
    bool writeFrame(const CaptureFrame &frame, std::string *error) override;
    bool close(std::string *error) override;

private:
    class Impl;
    Impl *m_impl;
};

} // namespace recorder
