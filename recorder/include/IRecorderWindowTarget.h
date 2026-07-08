#pragma once

#include <cstdint>

namespace recorder
{

class VisualFrameCache;

// 谁需要读 VisualFrameCache（可按位组合）。
enum class VisualConsumerFlag : int
{
    None = 0,
    LivePreview = 1, // 录屏对话框预览区轮询
    Recording = 2,   // 后台录制线程读缓存
};

inline VisualConsumerFlag operator|(VisualConsumerFlag a, VisualConsumerFlag b)
{
    return static_cast<VisualConsumerFlag>(static_cast<int>(a) | static_cast<int>(b));
}

inline int visualConsumerFlags(VisualConsumerFlag a, VisualConsumerFlag b)
{
    return static_cast<int>(a) | static_cast<int>(b);
}

inline bool hasVisualConsumerFlag(int flags, VisualConsumerFlag flag)
{
    return (flags & static_cast<int>(flag)) != 0;
}

// 固定窗口录制目标：宿主实现或使用 FixedWindowTarget / QWidgetRecorderTarget。
class IRecorderWindowTarget
{
public:
    virtual ~IRecorderWindowTarget() = default;

    virtual std::uintptr_t windowHandle() const = 0;
    virtual bool isAvailable() const = 0;

    virtual VisualFrameCache *visualCache() { return nullptr; }

    // 主线程：渲染当前可见内容到 visualCache()；force 时忽略消费者守卫（预览轮询用）。
    virtual void refreshVisualCache(bool force = false) { (void)force; }

    // 源内容变化（新帧/缩放/占位）；有消费者时刷新缓存。
    virtual void notifyVisualChanged();

    virtual void setVisualConsumerFlags(int flags);
    virtual int visualConsumerFlags() const;
    virtual bool hasVisualConsumers() const;

protected:
    int m_visualConsumerFlags = 0;
};

} // namespace recorder
