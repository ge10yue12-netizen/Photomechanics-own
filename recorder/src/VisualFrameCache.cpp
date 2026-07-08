#include "../include/VisualFrameCache.h"

namespace recorder
{

void VisualFrameCache::publish(CaptureFrame frame)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_frame = std::move(frame);
    ++m_version;
}

bool VisualFrameCache::copyLatest(CaptureFrame *out, std::uint64_t *versionOut) const
{
    if (!out)
        return false;
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_frame.width <= 0 || m_frame.height <= 0 || m_frame.bgra.empty())
        return false;
    *out = m_frame;
    if (versionOut)
        *versionOut = m_version;
    return true;
}

bool VisualFrameCache::hasFrame() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_frame.width > 0 && m_frame.height > 0 && !m_frame.bgra.empty();
}

std::uint64_t VisualFrameCache::version() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_version;
}

} // namespace recorder
