#pragma once

#include "RecorderKit.h"

#include <QObject>
#include <QString>

// 录屏集成门面（对齐 RemoteHost）。
// 职责：持有 ScreenRecorder、将 std::function 回调转 Qt 信号、供对话框或宿主直接调用。
class RecorderHost : public QObject
{
    Q_OBJECT

public:
    explicit RecorderHost(QObject *parent = nullptr);
    ~RecorderHost() override;

    // 按 config 初始化核心模块。
    bool init(const recorder::RecorderConfig &config);

    bool start();
    bool pause();
    bool resume();
    bool stop();

    recorder::RecorderState state() const;
    QString lastError() const;
    int recordedSeconds() const;

signals:
    void stateChanged(recorder::RecorderState state);
    void durationChanged(int seconds);
    void errorOccurred(const QString &message);
    void logMessage(const QString &message);

private:
    recorder::ScreenRecorder m_recorder;
};
