#pragma once

// 录屏模块 — Qt 宿主唯一入口
//
// 移植：复制 recorder/ → 按 README.md 改宿主 vcxproj 配置 → 宿主只 #include 本文件
//
//   RecorderHost m_recorderHost;
//   ScreenRecorderDialog *m_dialog = nullptr;
//   m_dialog->bindRecorderHost(&m_recorderHost);

#include "RecorderKit.h"
#include "RecorderHost.h"
#include "RecorderUiBinder.h"
#include "RecorderStatusText.h"
#include "RecorderPathHelper.h"
#include "RegionSelectorWidget.h"
#include "dialog/ScreenRecorderDialog.h"
