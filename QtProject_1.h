#pragma once

#include <QtWidgets/QWidget>
#include <QFileSystemWatcher>
#include <QJsonObject>
#include <QTimer>
#include <QLabel>
#include "camera/CameraController.h"
#include "remote/RemoteHost.h"
#include "remote/RemoteControlGuard.h"
#include "remote-qr/MobileHost.h"
#include "host-remote/RemoteControlCenter.h"
#include "recorder/RecorderQtKit.h"
#include "core/AppLogger.h"
#include "stage/StageManager.h"
#include "save/ImageSaveThread.h"
#include "save/SavePathHelper.h"
#include "ui_QtProject_1.h"

class GuideManager;

// QtProject_1：主窗口；协调 UI、相机预览/采集、阶段调度与存图
class QtProject_1 : public QWidget
{
    Q_OBJECT

public:
    explicit QtProject_1(QWidget *parent = nullptr);
    ~QtProject_1() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void changeEvent(QEvent *event) override;

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
    void onPreviewPixelInfo(int x, int y, int gray, bool valid); // 预览区悬停像素读数
    void onCameraError(const QString &message);
    // HTTP/BLE 遥控命令入口，与主界面按钮共用同一套业务槽。
    void onRemoteCommand(const QString &cmd);
    void onRemoteControl();
    void onScreenRecorder();

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
                         int saveFailCount,
                         double setFps);
    void onStageLoopStarted(int loopIndex, int totalLoops);
    void onStageAllFinished();
    void onStageStoppedByUser();

    // 存图线程回调
    void onSaveThreadFinished(const QString &path, bool ok, const QString &errorMsg);
    void onSaveQueueBacklog(int queueSize);
    void onSaveQueueFull(int queueSize);
    void logSaveQueueWarn(const QString &msg); // 队列积压/已满：写日志并刷新队列相关 UI

    void updateSaveModeUi();

private:
    void log(const QString &msg);
    void setCamStatus(const QString &text, const QString &color);
    void setStageStatus(const QString &text);
    void refreshStageStatusLabel();      // 按 m_stageStatusText 刷新工作流栏阶段标签（含队列后缀）
    void refreshQueueDependentUi();      // 队列/总保存变化时统一刷新阶段栏 + 底部状态条
    void setupStageTable();
    void refreshButtonState();       // 按相机/采集/阶段状态统一 enable 控件
    void loadDefaultUiValues();      // 启动时将存图路径与参数控件恢复为固定默认值
    void updateParamSpinLimits();
    bool applyCamParams();
    void syncSavePath();             // 将界面存图配置同步至 m_savePath
    void resyncSavePathFromDisk();   // 扫描磁盘同步 Pic 序号与总保存显示（外部删改文件后）
    void updateSaveDirWatcher();     // 监视保存根目录，目录变化时触发 resync
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
    void updatePreviewInfoLabel();         // 合并状态行与悬停像素信息
    void updateGlobalStatus();             // 刷新底部状态栏四段摘要
    void setupStartupGuide();         // 创建 GuideManager 并填充测试引导步骤
    void populateBasicCaptureGuide(); // 注册 basic_capture 的 7 步引导配置（宿主样例）
    // 组装遥控状态 JSON，供 HTTP 响应与 BLE Notify 使用。
    QJsonObject buildRemoteStatusJson() const;

    // 向 BLE 已连接客户端推送一次状态 Notify。
    void pushRemoteStatus();

    // 启用或关闭远程服务总闸；控制 HTTP/BLE/扫码会话生命周期。
    void applyRemoteService(bool enabled);

    // 判断当前是否应对预览帧执行 JPEG 编码。
    bool wantsPreviewEncoding() const;

    // 刷新主界面远程控制摘要标签。
    void refreshRemoteSummaryLabel();

    // 刷新主界面录屏摘要标签。
    void refreshRecorderSummaryLabel();

    Ui::QtProject_1Class ui;
    CameraController m_camera;
    StageManager m_stageMgr;
    ImageSaveThread m_saveThread;
    SavePathHelper m_savePath;
    AppLogger m_logger;              // 运行日志写入 Log/run_*.log；log() 同步写入文件与界面控件
    RemoteHost m_remoteHost;         // WiFi/BLE 遥控门面
    MobileHost m_mobileHost;         // 扫码 HTTP 遥控门面
    RemoteControlGuard m_remoteGuard; // 跨客户端命令通道互斥
    RemoteControlCenter *m_remoteCenter = nullptr; // 远程控制管理对话框
    QLabel *m_remoteSummaryLabel = nullptr;        // 主界面远程状态摘要标签
    RecorderController m_recorderController;
    QWidgetRecorderTarget m_cameraPreviewTarget;
    ScreenRecorderDialog *m_recorderDialog = nullptr; // 屏幕录制管理对话框
    QLabel *m_recorderSummaryLabel = nullptr;      // 主界面录屏状态摘要标签
    bool m_remoteEnabled = false;    // 远程服务总闸；为 false 时不监听 HTTP/BLE/扫码
    GuideManager *m_startupGuide = nullptr;       // GuideKit 实例
    bool m_startupGuideScheduled = false;         // showEvent 中仅调度一次 startIfNeeded
    QTimer m_displayTimer;           // 预览刷新定时器，间隔约 33 ms（约 30 Hz）
    QFileSystemWatcher m_saveDirWatcher; // 监视保存根目录，外部删改文件时 resync
    QTimer m_saveDirResyncTimer;     // 目录变化防抖，避免连续 resync
    QTimer m_saveCountPollTimer;     // 空闲时周期刷新总保存（子目录删文件 watcher 可能漏报）
    bool m_liveViewActive = false;   // 连续 grab 与预览定时器已启动（打开相机后常开）
    bool m_remotePreviewActive = false; // 远程页（H5/小程序）是否开启预览拉流
    bool m_acquisitionActive = false; // 用户已点击「开始采集」，处于采集业务会话
    bool m_stageRunning = false;     // StageManager 正在执行阶段表
    bool m_shutdownDone = false;     // 退出标志；为 true 时禁止回调访问 UI 控件
    QString m_stageStatusText;       // 阶段状态栏文本缓存，用于附加队列长度
    QString m_previewBaseInfo;       // 预览区底部状态行（分辨率、模式、队列等）
    QString m_previewPixelInfo;      // 悬停像素坐标与灰度值片段
    QString m_statusCameraSummary;   // 底部状态栏「相机」段摘要
    bool m_initialLayoutApplied = false; // showEvent 首次按比例设置 Splitter 尺寸
    quint64 m_lastEnqueuedFrameSeq = 0; // 阶段存图已入队的最新帧序号，用于去重
    quint64 m_lastDisplayFrameSeq = 0;  // 预览已显示的最新帧序号，用于跳过重复绘制
    QSize m_lastDisplayLabelSize;       // 预览区上次尺寸，尺寸变化时需重绘
};