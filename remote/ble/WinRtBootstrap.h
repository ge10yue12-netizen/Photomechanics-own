#pragma once

#ifdef _WIN32
#include <winrt/Windows.Foundation.h>

// 仅在专用 WinRT 工作线程（MTA）入口调用一次
inline void initWinRtMtaOnWorkerThread()
{
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
}
#else
inline void initWinRtMtaOnWorkerThread() {}
#endif