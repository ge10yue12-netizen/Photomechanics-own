// StageManager.cpp：阶段采集状态机，按目标帧数切换阶段，按 fps 触发存图

#include "StageManager.h"
#include <QElapsedTimer>
#include <QtGlobal>

namespace
{
// 入队已满后队列为空仍无法对齐写盘计数时，超过该毫秒数则强制结束本阶段
const int kPendingFinishForceMs = 3000;
}

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

// 进入本阶段：计算目标帧数；若启用存图则在 t=0 立即入队首帧，其余由定时器补齐
void StageManager::enterCurrentStage()
{
    if (m_userStop || m_currentStage >= m_stages.size())
        return;

    const StageItem &st = m_stages[m_currentStage];
    m_frameCount = 0;
    m_saveRequestCount = 0;
    m_saveCount = 0;
    m_saveFailCount = 0;
    m_awaitingEnqueueAck = false;
    m_pendingStageFinish = false;
    m_stageStartTime = QDateTime::currentDateTime();
    // 目标帧数由时长×帧率决定
    m_targetFrameCount = qMax(0, qRound(st.durationSec * st.fps));

    emit applyFps(st.fps);
    emit stageStarted(st.name, m_stageStartTime, m_currentLoop);

    if (m_targetFrameCount <= 0)
    {
        finishCurrentStage();
        advanceStage();
        return;
    }

    // 启用存图时在阶段起点尝试保存首帧，入队成功后才计入目标帧数
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

// 未达目标且无待确认请求时，向主窗口发起存图请求
void StageManager::tryEmitSaveRequest()
{
    if (!m_running || m_userStop)
        return;
    if (m_saveRequestCount >= m_targetFrameCount || m_awaitingEnqueueAck)
        return;
    m_awaitingEnqueueAck = true;
    emit saveFrameRequested();
}

// 写盘线程回调：成功与失败均计入，避免单帧写盘失败导致阶段结束状态无法完成对齐
void StageManager::notifySaveWriteFinished(bool ok)
{
    if (ok)
        ++m_saveCount;
    else
        ++m_saveFailCount;
    tryCompleteStageAfterSave();
}

// 存图队列已排空时再次尝试完成本阶段；超时仍未对齐则按失败补齐计数并强制切换阶段
void StageManager::onSaveQueueDrained()
{
    if (!m_pendingStageFinish)
        return;
    if (tryCompleteStageAfterSave())
        return;
    if (m_pendingFinishTimer.isValid() && m_pendingFinishTimer.elapsed() < kPendingFinishForceMs)
        return;

    const int resolved = m_saveCount + m_saveFailCount;
    if (resolved < m_saveRequestCount)
        m_saveFailCount += m_saveRequestCount - resolved;
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

// 入队失败（例如 t=0 时相机尚无可用帧）不计入目标帧数，通过 singleShot 异步重试以避免同步递归
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

    // 启用存图时末帧可能仍在写盘队列中，须待写盘回调计数与入队数对齐后再 emit stageFinished
    if (st.saveImage && st.fps > 0.0)
    {
        m_awaitingEnqueueAck = false;
        m_pendingStageFinish = true;
        m_pendingFinishTimer.start();
        return tryCompleteStageAfterSave();
    }

    finishCurrentStage();
    advanceStage();
    return true;
}

// 入队数已满且（成功+失败）写盘回调数对齐后，触发 stageFinished
bool StageManager::tryCompleteStageAfterSave()
{
    if (!m_pendingStageFinish)
        return false;
    if (m_saveCount + m_saveFailCount < m_saveRequestCount)
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
                       m_frameCount, m_saveRequestCount, m_saveCount, m_saveFailCount, st.fps);
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
