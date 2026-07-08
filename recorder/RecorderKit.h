#pragma once

#ifndef _WIN32
#error "recorder module requires Windows (DXGI / GDI / Media Foundation)."
#endif

// 可移植核心 API。
#include "include/RecorderTypes.h"
#include "include/RecorderError.h"
#include "include/RecorderPresets.h"
#include "include/ScreenRecorder.h"
#include "include/RecorderWindowTarget.h"
