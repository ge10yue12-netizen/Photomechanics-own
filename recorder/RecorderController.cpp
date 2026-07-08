#include "RecorderController.h"

#include <QTimer>

RecorderController::RecorderController(QObject *parent)
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

RecorderController::~RecorderController()
{
    m_recorder.stop();
}

bool RecorderController::init(const recorder::RecorderConfig &config)
{
    return m_recorder.init(config);
}

bool RecorderController::start()
{
    return m_recorder.start();
}

bool RecorderController::pause()
{
    return m_recorder.pause();
}

bool RecorderController::resume()
{
    return m_recorder.resume();
}

bool RecorderController::stop()
{
    return m_recorder.stop();
}

recorder::RecorderState RecorderController::state() const
{
    return m_recorder.state();
}

QString RecorderController::lastError() const
{
    return QString::fromStdString(m_recorder.lastError());
}

int RecorderController::recordedSeconds() const
{
    return m_recorder.recordedSeconds();
}
