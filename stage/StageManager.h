#pragma once

#include "core/AppTypes.h"
#include <QDateTime>
#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QTimer>

// StageManager：按阶段表顺序执行多轮采集，以目标帧数结束阶段，按 fps 触发存图
class StageManager : public QObject
{
    Q_OBJECT

public:
    explicit StageManager(QObject *parent = nullptr);

    void setStages(const QList<StageItem> &stages);
    void setLoopCount(int count); // 总轮数，最小为 1

    void start();
    void stop();                  // 用户停止：立即停止帧定时器并 emit stoppedByUser
    bool isRunning() const { return m_running; }
    bool isPendingStageFinish() const { return m_pendingStageFinish; }
    void notifySaveWriteFinished(bool ok);     // 写盘成功/失败均计入，用于与入队数对齐后切阶段
    void notifySaveEnqueued();                 // 主窗口入队成功后回调，计入目标帧数
    void notifySaveEnqueueFailed();            // 入队失败回调，触发重试
    void onSaveQueueDrained();                 // 存图队列排空时再次尝试完成本阶段

signals:
    void applyFps(double fps);           // 进入新阶段时通知主窗口改相机帧率
    void saveFrameRequested();           // 帧 tick 到达且启用存图时触发
    void stageStarted(const QString &name, const QDateTime &startTime, int loopIndex);
    void stageFinished(const QString &name,
                       const QDateTime &startTime,
                       const QDateTime &endTime,
                       int frameCount,
                       int saveRequestCount,
                       int savedCount,
                       int saveFailCount,
                       double setFps);
    void allLoopsFinished();             // 全部轮次与阶段完成
    void stoppedByUser();
    void loopStarted(int loopIndex, int totalLoops);

private slots:
    void onFrameTickTimer(); // 按 fps 节拍计数/存图，达标后结束本阶段

private:
    void enterCurrentStage();
    void finishCurrentStage();
    void advanceStage();     // 下一阶段或下一轮，或全部结束
    void startFrameTickTimer(double fps);
    void tryEmitSaveRequest();       // 未达目标且无待确认请求时 emit 存图
    bool tryFinishStageIfDone();     // 未启用存图：计帧达标即结束；启用存图：入队达标后等待写盘
    bool tryCompleteStageAfterSave(); // 入队已满且写盘回调数（成功+失败）对齐后 emit stageFinished

    QList<StageItem> m_stages;
    int m_loopCount = 1;         // 用户设置的总轮数
    int m_currentLoop = 0;       // 当前第几轮（1-based）
    int m_currentStage = 0;      // 当前阶段索引（0-based）
    bool m_running = false;
    bool m_userStop = false;     // stop() 置位，advance 时不再进入下一阶段
    QTimer m_frameTickTimer;     // 周期：存图/计帧节拍（间隔 1000/fps ms）
    int m_targetFrameCount = 0;  // 本阶段目标帧数 = round(时长 × 帧率)
    int m_frameCount = 0;        // 本阶段已过帧数
    int m_saveRequestCount = 0;  // 本阶段实际入队成功次数
    bool m_awaitingEnqueueAck = false; // 已 emit 存图请求、尚未收到入队结果
    bool m_pendingStageFinish = false; // 入队已满，等待本阶段写盘回调与入队数对齐
    int m_saveCount = 0;         // 本阶段写盘成功数
    int m_saveFailCount = 0;     // 本阶段写盘失败数（与成功合计须达到入队数才切阶段）
    QElapsedTimer m_pendingFinishTimer; // pending 起始时刻，用于队列排空后的超时强制完成
    QDateTime m_stageStartTime;
};
