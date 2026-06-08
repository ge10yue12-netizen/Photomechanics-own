// StageManager.cpp — 阶段采集状态机：目标帧数驱动切阶段 + fps 节拍存图

#include "StageManager.h"
#include <QtGlobal>

StageManager::StageManager(QObject *parent)
    : QObject(parent)
{
    connect(&m_frameTickTimer, &QTimer::timeout, this, &StageManager::onFrameTickTimer);
}

void StageManager::setStages(const QList<StageItem> &stages)
{
    m_stages = stages;
}

void StageManager::setLoopCount(int count)
{
    m_loopCount = count < 1 ? 1 : count;
}

// 从第 1 轮第 1 阶段开始；阶段表为空则直接返回
void StageManager::start()
{
    if (m_stages.isEmpty())
        return;

    m_userStop = false;
    m_running = true;
    m_currentLoop = 1;
    m_currentStage = 0;
    emit loopStarted(m_currentLoop, m_loopCount);
    enterCurrentStage();
}

void StageManager::stop()
{
    if (!m_running)
        return;
    m_userStop = true;
    m_frameTickTimer.stop();
    m_pendingStageFinish = false;
    m_running = false;
    emit stoppedByUser();
}

// 进入本阶段：计算目标帧数，勾选存图则 t=0 立即入队首帧，余下由 tick 补齐
void StageManager::enterCurrentStage()
{
    if (m_userStop || m_currentStage >= m_stages.size())
        return;

    const StageItem &st = m_stages[m_currentStage];
    m_frameCount = 0;
    m_saveRequestCount = 0;
    m_saveCount = 0;
    m_awaitingEnqueueAck = false;
    m_pendingStageFinish = false;
    m_stageStartTime = QDateTime::currentDateTime();
    // 目标帧数由时长×帧率决定
    m_targetFrameCount = qMax(0, qRound(st.durationSec * st.fps));

    emit applyFps(st.fps);
    emit stageStarted(st.name, m_stageStartTime);

    if (m_targetFrameCount <= 0)
    {
        finishCurrentStage();
        advanceStage();
        return;
    }

    // 勾选存图时在阶段起点立即尝试存第一帧，入队成功后才计入目标帧数
    if (st.saveImage && st.fps > 0.0)
        tryEmitSaveRequest();

    startFrameTickTimer(st.fps);
}

void StageManager::startFrameTickTimer(double fps)
{
    m_frameTickTimer.stop();
    if (fps <= 0.0)
        return;
    m_frameTickTimer.setInterval(qMax(1, static_cast<int>(1000.0 / fps)));
    m_frameTickTimer.start();
}

// 未达目标且无待确认请求时，向主窗口发起一次存图
void StageManager::tryEmitSaveRequest()
{
    if (!m_running || m_userStop)
        return;
    if (m_saveRequestCount >= m_targetFrameCount || m_awaitingEnqueueAck)
        return;
    m_awaitingEnqueueAck = true;
    emit saveFrameRequested();
}

// 主窗口 BMP 写盘完成后累加，若本阶段已在等写盘则尝试切阶段
void StageManager::notifyImageSaved()
{
    ++m_saveCount;
    tryCompleteStageAfterSave();
}

// 主窗口入队成功后计入本阶段帧数，并判断是否达标
void StageManager::notifySaveEnqueued()
{
    m_awaitingEnqueueAck = false;
    ++m_frameCount;
    ++m_saveRequestCount;
    tryFinishStageIfDone();
}

// 入队失败（如 t=0 相机尚无帧）时不计数，异步重试避免同步递归
void StageManager::notifySaveEnqueueFailed()
{
    m_awaitingEnqueueAck = false;
    if (!m_running || m_userStop || m_saveRequestCount >= m_targetFrameCount)
        return;
    QTimer::singleShot(1, this, [this]() { tryEmitSaveRequest(); });
}

// 本阶段达标后结束：存图阶段需等写盘数与入队数一致再 emit stageFinished
bool StageManager::tryFinishStageIfDone()
{
    if (m_currentStage >= m_stages.size() || m_targetFrameCount <= 0)
        return false;

    const StageItem &st = m_stages[m_currentStage];
    const int currentCount = st.saveImage ? m_saveRequestCount : m_frameCount;
    if (currentCount < m_targetFrameCount)
        return false;

    m_frameTickTimer.stop();

    // 勾选存图：最后一帧可能仍在写盘队列，延迟到 notifyImageSaved 对齐后再打日志
    if (st.saveImage && st.fps > 0.0)
    {
        m_pendingStageFinish = true;
        return tryCompleteStageAfterSave();
    }

    finishCurrentStage();
    advanceStage();
    return true;
}

// 入队已满且 m_saveCount 已追上 m_saveRequestCount 时真正结束本阶段
bool StageManager::tryCompleteStageAfterSave()
{
    if (!m_pendingStageFinish)
        return false;
    if (m_saveCount < m_saveRequestCount)
        return false;

    m_pendingStageFinish = false;
    finishCurrentStage();
    advanceStage();
    return true;
}

// 每个 tick：存图或计帧，达标后结束本阶段
void StageManager::onFrameTickTimer()
{
    if (!m_running || m_userStop || m_currentStage >= m_stages.size())
        return;

    const StageItem &st = m_stages[m_currentStage];
    if (st.saveImage && st.fps > 0.0)
        tryEmitSaveRequest();
    else
    {
        ++m_frameCount;
        tryFinishStageIfDone();
    }
}

void StageManager::finishCurrentStage()
{
    if (m_currentStage >= m_stages.size())
        return;

    const StageItem &st = m_stages[m_currentStage];
    emit stageFinished(st.name, m_stageStartTime, QDateTime::currentDateTime(),
                       m_frameCount, m_saveRequestCount, m_saveCount, st.fps);
}

// 顺序：下一阶段 → 下一轮首阶段 → 全部完成
void StageManager::advanceStage()
{
    if (m_userStop)
    {
        m_frameTickTimer.stop();
        m_running = false;
        return;
    }

    ++m_currentStage;
    if (m_currentStage < m_stages.size())
    {
        enterCurrentStage();
        return;
    }

    if (m_currentLoop < m_loopCount)
    {
        ++m_currentLoop;
        m_currentStage = 0;
        emit loopStarted(m_currentLoop, m_loopCount);
        enterCurrentStage();
        return;
    }

    m_frameTickTimer.stop();
    m_running = false;
    emit allLoopsFinished();
}
