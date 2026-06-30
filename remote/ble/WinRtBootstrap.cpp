#include "WinRtBootstrap.h"

#ifdef _WIN32
#include <winrt/Windows.Foundation.h>

// 初始化 WinRT 多线程公寓（MTA），供 GATT Server 在同一线程调用。
void initWinRtMtaOnWorkerThread()
{
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
}
#else
void initWinRtMtaOnWorkerThread() {}
#endif
