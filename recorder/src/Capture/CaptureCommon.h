#pragma once

#include "../../include/RecorderTypes.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>

namespace recorder
{
namespace capture
{

inline Rect virtualScreenRect()
{
    Rect r;
    r.x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    r.y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    r.width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    r.height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    return r;
}

// 主显示器桌面区域（FullScreen 模式：预览与 GDI 录制均以此为准，非虚拟全桌面）。
inline Rect primaryScreenRect()
{
    const POINT pt = {0, 0};
    const HMONITOR monitor = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO info = {};
    info.cbSize = sizeof(MONITORINFO);
    Rect r;
    if (monitor && GetMonitorInfo(monitor, &info))
    {
        r.x = info.rcMonitor.left;
        r.y = info.rcMonitor.top;
        r.width = info.rcMonitor.right - info.rcMonitor.left;
        r.height = info.rcMonitor.bottom - info.rcMonitor.top;
        return r;
    }
    r.x = 0;
    r.y = 0;
    r.width = GetSystemMetrics(SM_CXSCREEN);
    r.height = GetSystemMetrics(SM_CYSCREEN);
    return r;
}

inline bool validateRegion(const Rect &region, std::string *error)
{
    if (region.width <= 0 || region.height <= 0)
    {
        if (error)
            *error = "录制区域宽高无效。";
        return false;
    }

    const Rect screen = virtualScreenRect();
    if (region.x < screen.x || region.y < screen.y ||
        region.x + region.width > screen.x + screen.width ||
        region.y + region.height > screen.y + screen.height)
    {
        if (error)
            *error = "录制区域超出屏幕范围。";
        return false;
    }
    return true;
}

} // namespace capture
} // namespace recorder
