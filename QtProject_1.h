#pragma once

#include <QtWidgets/QWidget>
#include <QTimer>
#include "camera/CameraController.h"
#include "stage/StageManager.h"
#include "save/ImageSaveThread.h"
#include "save/SavePathHelper.h"
#include "ui_QtProject_1.h"

// QtProject_1：主窗口，负责 UI、相机预览/采集、阶段存图与日志
class QtProject_1 : public QWidget
{
    Q_OBJECT

public:
    explicit QtProject_1(QWidget *parent = nullptr);
    ~QtProject_1() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    // 相机连接与预览
    void onOpenCamera();
    void onCloseCamera();
    void onStartGrab();
    void onStopGrab();
    void onApplyParams();
    void onSaveOneBmp();
    void onBrowseSavePath();
    void onDisplayTimer();
    void onCameraError(const QString &message);

    // 阶段表编辑
    void onAddStage();
    void onDeleteStage();
    void onClearStages();

    // 阶段采集
    void onStartStageCapture();
    void onStopStageCapture();
    void onStageApplyFps(double fps);
    void onStageSaveFrame();
    void onStageStarted(const QString &name, const QDateTime &startTime, int loopIndex);
    void onStageFinished(const QString &name,
                         const QDateTime &startTime,
                         const QDateTime &endTime,
                         int frameCount,
                         int saveRequestCount,
                         int savedCount,
                         double setFps);
    void onStageLoopStarted(int loopIndex, int totalLoops);
    void onStageAllFinished();
    void onStageStoppedByUser();

    // 存图线程回调
    void onSaveThreadFinished(const QString &path, bool ok, const QString &errorMsg);
    void onSaveQueueBacklog(int queueSize);
    void onSaveQueueFull(int queueSize);

    void updateSaveModeUi();

private:
    void log(const QString &msg);
    void setCamStatus(const QString &text, const QString &color);
    void setStageStatus(const QString &text);
    void setupStageTable();
    void refreshButtonState();       // 按相机/采集/阶段状态统一 enable 控件
    void loadDefaultUiValues();      // 启动时将存图路径与参数控件恢复为固定默认值
    void updateParamSpinLimits();
    bool applyCamParams();
    void syncSavePath();             // 将界面存图配置同步至 m_savePath
    bool validateStageTable();
    QList<StageItem> readStageListFromTable() const;
    bool enqueueCurrentFrame();    // 复制最新帧并提交存图任务
    void waitSaveQueueDrained(bool warnIfTimeout = false); // 阻塞等待存图队列排空
    void resetEnqueueFrameSeqFromCamera(); // 从当前最新帧初始化阶段入队序号基准
    void startLiveView();          // 启动 grab 与预览定时器
    void ensureLiveView();         // 确保 grab 与预览定时器处于运行状态
    void stopLiveView();           // 关闭 grab 与预览定时器
    void stopCaptureAndWaitSave(bool userStop);
    void shutdownAll();              // 退出清理：停采、排空队列、关相机、终止 Pylon
    void insertStageRow(int row, const QString &name);
    void refreshStageTableSerialNumbers(); // 按行刷新「序号」列（1-based）

    Ui::QtProject_1Class ui;
    CameraController m_camera;
    StageManager m_stageMgr;
    ImageSaveThread m_saveThread;
    SavePathHelper m_savePath;
    QTimer m_displayTimer;           // 约 30fps 刷新预览 label
    bool m_liveViewActive = false;   // 相机连续 grab + 预览定时器（打开相机后常开）
    bool m_acquisitionActive = false; // 用户点击「开始采集」后的采集会话
    bool m_stageRunning = false;     // StageManager 是否正在执行阶段表
    bool m_shutdownDone = false;     // 退出标志，防止退出过程中回调访问 UI
    QString m_stageStatusText;       // 阶段状态栏文本缓存，用于附加队列长度
    quint64 m_lastEnqueuedFrameSeq = 0; // 阶段存图已入队的最新帧序号，用于去重
    quint64 m_lastDisplayFrameSeq = 0;  // 预览已显示的最新帧序号，用于跳过重复绘制
    QSize m_lastDisplayLabelSize;       // 预览区上次尺寸，尺寸变化时需重绘
};
