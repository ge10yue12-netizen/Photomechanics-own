#include "RecorderHost.h"

#include <QTimer>

RecorderHost::RecorderHost(QObject *parent)
    : QObject(parent)
{
    recorder::RecorderCallback cb;
    cb.onStateChanged = [this](recorder::RecorderState st) {
        QTimer::singleShot(0, this, [this, st]() { emit stateChanged(st); });
    };
    cb.onDurationChanged = [this](int sec) {
        QTimer::singleShot(0, this, [this, sec]() { emit durationChanged(sec); });
    };
    cb.onError = [this](const std::string &msg) {
        const QString qmsg = QString::fromStdString(msg);
        QTimer::singleShot(0, this, [this, qmsg]() { emit errorOccurred(qmsg); });
    };
    cb.onLog = [this](const std::string &msg) {
        const QString qmsg = QString::fromStdString(msg);
        QTimer::singleShot(0, this, [this, qmsg]() { emit logMessage(qmsg); });
    };
    m_recorder.setCallback(cb);
}

RecorderHost::~RecorderHost()
{
    m_recorder.stop();
}

bool RecorderHost::init(const recorder::RecorderConfig &config)
{
    return m_recorder.init(config);
}

bool RecorderHost::start()
{
    return m_recorder.start();
}

bool RecorderHost::pause()
{
    return m_recorder.pause();
}

bool RecorderHost::resume()
{
    return m_recorder.resume();
}

bool RecorderHost::stop()
{
    return m_recorder.stop();
}

recorder::RecorderState RecorderHost::state() const
{
    return m_recorder.state();
}

QString RecorderHost::lastError() const
{
    return QString::fromStdString(m_recorder.lastError());
}

int RecorderHost::recordedSeconds() const
{
    return m_recorder.recordedSeconds();
}
