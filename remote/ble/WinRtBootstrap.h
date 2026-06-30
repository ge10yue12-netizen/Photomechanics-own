#pragma once

// 在专用 WinRT 工作线程（MTA）入口调用一次；实现见 WinRtBootstrap.cpp（C++17 编译单元）。
void initWinRtMtaOnWorkerThread();
