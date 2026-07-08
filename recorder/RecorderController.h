#pragma once

#include "RecorderKit.h"

#include <QObject>
#include <QString>

// Qt 集成层：持有 recorder::ScreenRecorder，将 C++ 回调转为 Qt 信号。
// 生命周期须不短于绑定它的 UI；析构时自动 stop()。
class RecorderController : public QObject
{
    Q_OBJECT

public:
    explicit RecorderController(QObject *parent = nullptr);
    ~RecorderController() override;

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
