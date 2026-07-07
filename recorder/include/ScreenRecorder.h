#pragma once

#include "RecorderError.h"

namespace recorder
{

// 屏幕录制对外门面（Pimpl）。
// 职责：校验配置、驱动采集/编码线程、维护状态机；不依赖 Qt 与 FFmpeg 头文件。
class ScreenRecorder
{
public:
    ScreenRecorder();
    ~ScreenRecorder();

    ScreenRecorder(const ScreenRecorder &) = delete;
    ScreenRecorder &operator=(const ScreenRecorder &) = delete;

    // 按 config 初始化采集与编码；成功进入 Initialized。
    bool init(const RecorderConfig &config);

    // 开始录制；须处于 Initialized 或 Stopped（重新 Init 后）。
    bool start();

    // 暂停采集入队；须处于 Recording。
    bool pause();

    // 从 Paused 恢复；须处于 Paused。
    bool resume();

    // 停止并 flush 输出文件；可从 Recording 或 Paused 调用。
    bool stop();

    // 返回当前状态。
    RecorderState state() const;

    // 返回最近一次错误的用户可读说明。
    std::string lastError() const;

    // 返回已录制秒数（含暂停前累计）。
    int recordedSeconds() const;

    // 注册回调；可在 Init 前后调用。
    void setCallback(const RecorderCallback &callback);

private:
    class Impl;
    Impl *m_impl;
};

} // namespace recorder
