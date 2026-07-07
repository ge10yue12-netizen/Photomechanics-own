#pragma once

#include "../../include/RecorderTypes.h"
#include <cstdint>
#include <vector>

namespace recorder
{

// 单帧 BGRA 像素缓冲。
struct CaptureFrame
{
    int width = 0;
    int height = 0;
    int stride = 0;
    std::vector<std::uint8_t> bgra;
};

// 屏幕采集抽象；实现类仅出现在 .cpp。
class IScreenCapture
{
public:
    virtual ~IScreenCapture() = default;

    // 按模式与区域初始化；FullScreen 为主显示器，忽略 region。
    virtual bool open(CaptureMode mode, const Rect &region, std::string *error) = 0;

    // 抓取一帧 BGRA；失败时写入 error。
    virtual bool grab(CaptureFrame *frame, std::string *error) = 0;

    // 释放 GDI 等资源。
    virtual void close() = 0;

    // 返回采集区域像素宽。
    virtual int captureWidth() const = 0;

    // 返回采集区域像素高。
    virtual int captureHeight() const = 0;
};

} // namespace recorder
