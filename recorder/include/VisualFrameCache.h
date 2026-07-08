#pragma once

#include "../src/Capture/IScreenCapture.h"

#include <cstdint>
#include <mutex>

namespace recorder
{

// 主线程写入、录制线程只读的 BGRA 帧缓存（避免 PrintWindow / 跨线程 grab）。
class VisualFrameCache
{
public:
    void publish(CaptureFrame frame);
    bool copyLatest(CaptureFrame *out, std::uint64_t *versionOut = nullptr) const;
    bool hasFrame() const;
    std::uint64_t version() const;

private:
    mutable std::mutex m_mutex;
    CaptureFrame m_frame;
    std::uint64_t m_version = 0;
};

} // namespace recorder
