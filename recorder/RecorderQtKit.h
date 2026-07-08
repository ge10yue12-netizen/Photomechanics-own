#pragma once

// 录屏模块 — Qt 集成唯一入口
//
// 集成步骤见 README.md。典型用法：
//
//   #include "recorder/RecorderQtKit.h"
//
//   RecorderController m_recorder;
//   ScreenRecorderDialog *m_dialog = nullptr;
//   m_dialog->bindController(&m_recorder);

#include "RecorderKit.h"
#include "RecorderController.h"
#include "RecorderUiBinder.h"
#include "RecorderStatusText.h"
#include "RecorderPathHelper.h"
#include "RegionSelectorWidget.h"
#include "dialog/ScreenRecorderDialog.h"
#include "RecorderQtWindowTarget.h"
