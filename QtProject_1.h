#pragma once

#include <QtWidgets/QWidget>
#include <QTimer>
#include "camera/CameraController.h"
#include "stage/StageManager.h"
#include "save/ImageSaveThread.h"
#include "save/SavePathHelper.h"
#include "ui_QtProject_1.h"

// QtProject_1 — 主窗口：UI（.ui）+ 相机预览/采集 + 阶段存图 + 日志
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

    void updateSaveModeUi();

private:
    void log(const QString &msg);
    void setCamStatus(const QString &text, const QString &color);
    void setStageStatus(const QString &text);
    void setupStageTable();
    void refreshButtonState();       // 按相机/采集/阶段状态统一 enable 控件
    void updateActionButtonStyles(); // 六个主操作按钮蓝/红样式角色
    void updateParamSpinLimits();
    bool applyCamParams();
    void syncSavePath();             // 界面存图选项 → m_savePath
    bool validateStageTable();
    QList<StageItem> readStageListFromTable() const;
    bool enqueueCurrentFrame();    // 抓最新帧并入存图队列
    void startPreview();           // 点击「开始采集」时启动 grab + 预览定时器
    void stopPreview();            // 点击「停止采集」或关相机时停止 grab 与预览
    void stopCaptureAndWaitSave(bool userStop);
    void shutdownAll();              // 退出/析构：停采、排空队列、关相机、终止 Pylon
    void insertStageRow(int row, const QString &name);

    Ui::QtProject_1Class ui;
    CameraController m_camera;
    StageManager m_stageMgr;
    ImageSaveThread m_saveThread;
    SavePathHelper m_savePath;
    QTimer m_displayTimer;           // 约 30fps 刷新预览 label
    bool m_capturing = false;        // 相机处于连续 grab（预览或阶段）
    bool m_stageRunning = false;     // StageManager 正在跑阶段表
    bool m_shutdownDone = false;     // 防止退出过程中 log/回调再碰 UI
    QString m_stageStatusText;       // 阶段状态栏缓存，便于追加队列信息
    quint64 m_lastEnqueuedFrameSeq = 0; // 阶段存图已入队的最新帧序号，避免重复保存同一帧
};
