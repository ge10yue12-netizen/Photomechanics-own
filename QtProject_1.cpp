// QtProject_1.cpp：主窗口信号槽、预览定时器、按钮状态与阶段/存图调度

#include "QtProject_1.h"

#include "ui/PreviewWidget.h"

#include <QCloseEvent>
#include <QShowEvent>
#include <QEvent>
#include <QDir>
#include <QPushButton>
#include <QStyle>
#include <QDateTime>
#include <QFileDialog>
#include <QHeaderView>
#include <QMessageBox>
#include <QTableWidgetItem>

namespace
{
// 阶段表列索引：0=序号（只读），1~4 为阶段配置
enum StageTableCol
{
    ColSerial = 0,
    ColName = 1,
    ColDuration = 2,
    ColFps = 3,
    ColSave = 4,
    StageTableColumnCount = 5
};

// 阶段表「存图」列须设置 ItemIsUserCheckable，否则勾选框无法交互
QTableWidgetItem *makeSaveCheckItem(bool checked = true)
{
    auto *chk = new QTableWidgetItem();
    chk->setFlags(chk->flags() | Qt::ItemIsUserCheckable);
    chk->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    return chk;
}

// 更新按钮 btnRole 属性并刷新样式（primary=蓝 / danger=红）
void applyButtonRole(QPushButton *btn, const char *role)
{
    if (!btn)
        return;
    btn->setProperty("btnRole", role);
    btn->style()->unpolish(btn);
    btn->style()->polish(btn);
}
}

QtProject_1::QtProject_1(QWidget *parent)
    : QWidget(parent)
{
    ui.setupUi(this);
    // 上半左右栏比例：预览 5，工作流栏 4，工作流栏可拖动但不可折叠到 0
    ui.mainSplitter->setStretchFactor(0, 5);
    ui.mainSplitter->setStretchFactor(1, 4);
    ui.mainSplitter->setCollapsible(0, false);
    ui.mainSplitter->setCollapsible(1, false);
    // 外层上下分栏：上半（预览+工作流）5，下半（日志）2，日志栏可拖到 0 完全收起
    ui.outerSplitter->setStretchFactor(0, 5);
    ui.outerSplitter->setStretchFactor(1, 2);
    ui.outerSplitter->setCollapsible(0, false);
    ui.outerSplitter->setCollapsible(1, true);
    ui.imageLabel->setMinimumSize(640, 400);
    ui.imageLabel->setPlaceholderText(QStringLiteral("未连接相机"));
    connect(ui.imageLabel, &PreviewWidget::pixelInfoChanged, this, &QtProject_1::onPreviewPixelInfo);
    ui.logTextEdit->setMaximumBlockCount(2000);
    // 六个主操作按钮通过 btnRole 区分蓝(primary)/红(danger)；禁用为灰
    // 底部状态条用浅灰底 + 顶部分隔线，区别于普通区域
    setStyleSheet(QStringLiteral(
        "QPushButton[btnRole=\"primary\"]{background-color:rgb(0,120,212);color:white;border:none;min-height:28px;}"
        "QPushButton[btnRole=\"primary\"]:hover{background-color:rgb(0,100,180);}"
        "QPushButton[btnRole=\"primary\"]:pressed{background-color:rgb(0,80,150);}"
        "QPushButton[btnRole=\"danger\"]{background-color:rgb(196,43,28);color:white;border:none;min-height:28px;}"
        "QPushButton[btnRole=\"danger\"]:hover{background-color:rgb(160,35,22);}"
        "QPushButton[btnRole=\"danger\"]:pressed{background-color:rgb(130,28,18);}"
        "QPushButton[btnRole=\"primary\"]:disabled,QPushButton[btnRole=\"danger\"]:disabled"
        "{background-color:rgb(200,200,200);color:rgb(240,240,240);}"
        "QPlainTextEdit#logTextEdit{font-family:Consolas,monospace;}"
        "QFrame#statusBarFrame{background-color:rgb(245,245,245);border-top:1px solid rgb(200,200,200);}"
        "QFrame#statusBarFrame QLabel{color:rgb(64,64,64);}"));
    m_statusCameraSummary = QStringLiteral("未连接");
    setCamStatus(QStringLiteral("状态: 未连接"), QStringLiteral("rgb(102, 102, 102)"));

    setupStageTable();
    ui.cameraSelectCombo->addItem(QStringLiteral("Basler"));

    // 控件 → 槽
    connect(ui.openCameraBtn, &QPushButton::clicked, this, &QtProject_1::onOpenCamera);
    connect(ui.closeCameraBtn, &QPushButton::clicked, this, &QtProject_1::onCloseCamera);
    connect(ui.startGrabBtn, &QPushButton::clicked, this, &QtProject_1::onStartGrab);
    connect(ui.stopGrabBtn, &QPushButton::clicked, this, &QtProject_1::onStopGrab);
    connect(ui.applyParamBtn, &QPushButton::clicked, this, &QtProject_1::onApplyParams);
    connect(ui.saveOneBmpBtn, &QPushButton::clicked, this, &QtProject_1::onSaveOneBmp);
    connect(ui.browsePathBtn, &QPushButton::clicked, this, &QtProject_1::onBrowseSavePath);
    connect(ui.clearLogBtn, &QPushButton::clicked, ui.logTextEdit, &QPlainTextEdit::clear);
    connect(ui.savePathEdit, &QLineEdit::editingFinished, this, [this]() {
        syncSavePath();
        updateSaveDirWatcher();
        if (!m_stageRunning && !m_acquisitionActive)
            resyncSavePathFromDisk();
    });
    connect(ui.saveModeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &QtProject_1::updateSaveModeUi);
    connect(ui.addStageBtn, &QPushButton::clicked, this, &QtProject_1::onAddStage);
    connect(ui.deleteStageBtn, &QPushButton::clicked, this, &QtProject_1::onDeleteStage);
    connect(ui.clearStageBtn, &QPushButton::clicked, this, &QtProject_1::onClearStages);
    connect(ui.startStageBtn, &QPushButton::clicked, this, &QtProject_1::onStartStageCapture);
    connect(ui.stopStageBtn, &QPushButton::clicked, this, &QtProject_1::onStopStageCapture);
    connect(&m_stageMgr, &StageManager::applyFps, this, &QtProject_1::onStageApplyFps);
    connect(&m_stageMgr, &StageManager::saveFrameRequested, this, &QtProject_1::onStageSaveFrame);
    connect(&m_stageMgr, &StageManager::stageStarted, this, &QtProject_1::onStageStarted);
    connect(&m_stageMgr, &StageManager::stageFinished, this, &QtProject_1::onStageFinished);
    connect(&m_stageMgr, &StageManager::loopStarted, this, &QtProject_1::onStageLoopStarted);
    connect(&m_stageMgr, &StageManager::allLoopsFinished, this, &QtProject_1::onStageAllFinished);
    connect(&m_stageMgr, &StageManager::stoppedByUser, this, &QtProject_1::onStageStoppedByUser);
    connect(&m_saveThread, &ImageSaveThread::saveFinished, this, &QtProject_1::onSaveThreadFinished);
    connect(&m_saveThread, &ImageSaveThread::queueBacklog, this, &QtProject_1::onSaveQueueBacklog);
    connect(&m_saveThread, &ImageSaveThread::queueFull, this, &QtProject_1::onSaveQueueFull);
    connect(&m_camera, &CameraController::errorOccurred, this, &QtProject_1::onCameraError);

    // 六个主操作按钮样式固定：正向 primary=蓝，停止/关闭 danger=红
    applyButtonRole(ui.openCameraBtn, "primary");
    applyButtonRole(ui.startGrabBtn, "primary");
    applyButtonRole(ui.startStageBtn, "primary");
    applyButtonRole(ui.closeCameraBtn, "danger");
    applyButtonRole(ui.stopGrabBtn, "danger");
    applyButtonRole(ui.stopStageBtn, "danger");

    loadDefaultUiValues();
    updateSaveModeUi();
    syncSavePath();
    m_savePath.resumeFromDisk(); // 启动时根据磁盘已有文件续接 Pic 序号
    updateSaveDirWatcher();

    m_displayTimer.setInterval(33); // 预览约 30Hz，打开相机后持续刷新
    connect(&m_displayTimer, &QTimer::timeout, this, &QtProject_1::onDisplayTimer);
    // 保存目录监视：外部删改文件后 resync；空闲时定时刷新总保存显示
    m_saveDirResyncTimer.setSingleShot(true);
    m_saveDirResyncTimer.setInterval(400);
    connect(&m_saveDirWatcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &) {
        m_saveDirResyncTimer.start();
    });
    connect(&m_saveDirResyncTimer, &QTimer::timeout, this, &QtProject_1::resyncSavePathFromDisk);
    m_saveCountPollTimer.setInterval(2000);
    connect(&m_saveCountPollTimer, &QTimer::timeout, this, [this]() {
        if (!m_shutdownDone && !m_stageRunning)
            refreshQueueDependentUi();
    });
    m_saveCountPollTimer.start();
    m_saveThread.start();

    m_logger.openRunLog();

    log(m_camera.initPylon() ? QStringLiteral("Pylon 初始化成功。")
                              : QStringLiteral("[警告] Pylon 初始化失败。"));
    log(QStringLiteral("软件启动。"));
    refreshButtonState();
    updateGlobalStatus();
}

QtProject_1::~QtProject_1()
{
    shutdownAll();
}

void QtProject_1::closeEvent(QCloseEvent *event)
{
    shutdownAll();
    QWidget::closeEvent(event);
}

// 首次显示时按 5:2 / 5:4 比例设置 Splitter 初始尺寸，避免首次打开分栏过窄或过宽
void QtProject_1::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (m_initialLayoutApplied)
        return;
    m_initialLayoutApplied = true;

    const int outerH = ui.outerSplitter->height();
    if (outerH > 0)
    {
        const int topH = outerH * 5 / 7;
        ui.outerSplitter->setSizes({topH, outerH - topH});
    }

    const int mainW = ui.mainSplitter->width();
    if (mainW > 0)
    {
        const int previewW = mainW * 5 / 9;
        ui.mainSplitter->setSizes({previewW, mainW - previewW});
    }
}

void QtProject_1::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    // 从资源管理器切回软件时 resync，覆盖子目录内删文件 watcher 未触达的情况
    if (event->type() == QEvent::ActivationChange && isActiveWindow())
        resyncSavePathFromDisk();
}

// 退出清理（幂等）：停止阶段、断开信号、停止采集、等待写盘队列、关闭相机、终止 Pylon
void QtProject_1::shutdownAll()
{
    if (m_shutdownDone)
        return;
    m_shutdownDone = true;

    if (m_stageRunning)
    {
        m_stageMgr.stop();
        m_stageRunning = false;
    }

    disconnect(&m_camera, nullptr, this, nullptr);
    disconnect(&m_saveThread, nullptr, this, nullptr);
    disconnect(&m_stageMgr, nullptr, this, nullptr);

    waitSaveQueueDrained();
    stopLiveView();
    m_saveThread.requestStopAndWait(30000);

    if (m_camera.isOpen())
        m_camera.close();
    if (m_camera.isPylonInitialized())
        m_camera.shutdownPylon();

    m_logger.closeRunLog();
}

void QtProject_1::loadDefaultUiValues()
{
    // 每次启动恢复固定默认值，不读写 QSettings
    ui.savePathEdit->setText(SavePathHelper::defaultRootPath());
    ui.saveModeCombo->setCurrentIndex(0);
    ui.picsPerFolderSpin->setValue(100);
    ui.secondsPerFolderSpin->setValue(10);
    ui.maxSaveCountSpin->setValue(0);
    ui.loopCountSpin->setValue(1);
    ui.exposureSpin->setValue(10000.0);
    ui.gainSpin->setValue(0.0);
    ui.fpsSpin->setValue(20.0);
}

void QtProject_1::waitSaveQueueDrained(bool warnIfTimeout)
{
    if (m_saveThread.queueSize() <= 0)
        return;
    m_saveThread.waitUntilEmpty(60000);
    if (warnIfTimeout && m_saveThread.queueSize() > 0 && !m_shutdownDone)
        log(QStringLiteral("[警告] 仍有图片未保存完成。"));
}

void QtProject_1::resetEnqueueFrameSeqFromCamera()
{
    m_lastEnqueuedFrameSeq = 0;
    QImage warmFrame;
    quint64 warmSeq = 0;
    if (m_camera.copyLatestImage(warmFrame, &warmSeq))
        m_lastEnqueuedFrameSeq = warmSeq;
}

void QtProject_1::log(const QString &msg)
{
    const QString line = m_logger.formatLine(msg);

    // 退出流程中仍将日志写入运行日志文件；不再更新界面日志控件
    m_logger.writeLine(line);

    if (m_shutdownDone)
        return;
    ui.logTextEdit->appendPlainText(line);
}

// 更新相机连接状态标签的文本与颜色样式，同时同步底部状态栏「相机」摘要
void QtProject_1::setCamStatus(const QString &text, const QString &color)
{
    ui.cameraStatusLabel->setText(text);
    ui.cameraStatusLabel->setStyleSheet(
        QStringLiteral("color:%1;font-weight:bold;padding:4px 0;").arg(color));
    // 状态栏摘要去掉「状态: 」前缀，留下纯短描述
    QString summary = text;
    const QString prefix = QStringLiteral("状态: ");
    if (summary.startsWith(prefix))
        summary = summary.mid(prefix.size());
    m_statusCameraSummary = summary;
    if (!m_shutdownDone)
        updateGlobalStatus();
}

// 更新阶段状态文本，并刷新依赖队列长度的 UI
void QtProject_1::setStageStatus(const QString &text)
{
    m_stageStatusText = text;
    refreshQueueDependentUi();
}

// 仅刷新工作流栏 stageStatusLabel（阶段运行中且队列非空时附加「| 队列 N」）
void QtProject_1::refreshStageStatusLabel()
{
    QString label = m_stageStatusText.isEmpty() ? QStringLiteral("当前无运行中的阶段") : m_stageStatusText;
    if (m_stageRunning)
    {
        const int pending = m_saveThread.queueSize();
        if (pending > 0)
            label += QStringLiteral(" | 队列 %1").arg(pending);
    }
    ui.stageStatusLabel->setText(label);
}

// 队列长度或总保存变化时统一刷新：工作流阶段栏 + 底部状态条
void QtProject_1::refreshQueueDependentUi()
{
    if (m_shutdownDone)
        return;
    refreshStageStatusLabel();
    updateGlobalStatus();
}

// 刷新底部状态栏 4 段：相机、阶段、队列(满 80% 红字)、总保存
void QtProject_1::updateGlobalStatus()
{
    if (m_shutdownDone)
        return;

    ui.statusCamera->setText(QStringLiteral("相机: %1").arg(m_statusCameraSummary));

    QString stageText = m_stageRunning && !m_stageStatusText.isEmpty()
                            ? m_stageStatusText
                            : (m_stageRunning ? QStringLiteral("运行中") : QStringLiteral("空闲"));
    ui.statusStage->setText(QStringLiteral("阶段: %1").arg(stageText));

    const int qSize = m_saveThread.queueSize();
    const int qCap = m_saveThread.capacity();
    ui.statusQueue->setText(QStringLiteral("队列: %1/%2").arg(qSize).arg(qCap));
    // 队列占比 ≥ 80% 时切红字告警，恢复后回灰
    const bool warn = qCap > 0 && qSize * 5 >= qCap * 4;
    ui.statusQueue->setStyleSheet(warn ? QStringLiteral("color:rgb(196,43,28);font-weight:bold;")
                                       : QString());

    ui.statusTotal->setText(QStringLiteral("总保存: %1").arg(m_savePath.totalSaved())); // 实时扫盘，非内存累计
}

void QtProject_1::insertStageRow(int row, const QString &name)
{
    ui.stageTable->insertRow(row);
    ui.stageTable->setItem(row, ColName, new QTableWidgetItem(name));
    ui.stageTable->setItem(row, ColDuration, new QTableWidgetItem(QStringLiteral("1.0")));
    ui.stageTable->setItem(row, ColFps, new QTableWidgetItem(QStringLiteral("20")));
    ui.stageTable->setItem(row, ColSave, makeSaveCheckItem(true)); // 默认启用存图
    refreshStageTableSerialNumbers();
}

void QtProject_1::refreshStageTableSerialNumbers()
{
    for (int r = 0; r < ui.stageTable->rowCount(); ++r)
    {
        auto *item = ui.stageTable->item(r, ColSerial);
        if (!item)
        {
            item = new QTableWidgetItem();
            item->setTextAlignment(Qt::AlignCenter);
            // 序号由程序维护，用户不可编辑
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            ui.stageTable->setItem(r, ColSerial, item);
        }
        item->setText(QString::number(r + 1));
    }
}

void QtProject_1::setupStageTable()
{
    auto *t = ui.stageTable;
    t->setColumnCount(StageTableColumnCount);
    t->setHorizontalHeaderLabels({QStringLiteral("序号"), QStringLiteral("阶段名称"),
                                  QStringLiteral("时长(s)"), QStringLiteral("帧率(fps)"),
                                  QStringLiteral("存图")});
    t->verticalHeader()->setVisible(false); // 隐藏 Qt 垂直表头行号，避免清空表格后表头布局异常
    t->verticalHeader()->setDefaultSectionSize(22); // 紧凑行高，适配工作流栏纵向空间
    t->horizontalHeader()->setSectionResizeMode(ColSerial, QHeaderView::ResizeToContents);
    t->horizontalHeader()->setSectionResizeMode(ColName, QHeaderView::Stretch);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    if (t->rowCount() == 0)
        insertStageRow(0, QStringLiteral("阶段1"));
}

void QtProject_1::updateParamSpinLimits()
{
    const CamParamLimits lim = m_camera.paramLimits();
    ui.exposureSpin->setRange(lim.exposureMinUs, lim.exposureMaxUs);
    ui.gainSpin->setRange(lim.gainMinDb, lim.gainMaxDb);
    ui.fpsSpin->setRange(lim.fpsMin, lim.fpsMax);
    // 底部标签展示相机实际可读范围
    ui.paramRangeLabel->setText(QStringLiteral("曝光 %1~%2 | 增益 %3~%4 | 帧率 %5~%6")
                                    .arg(lim.exposureMinUs, 0, 'f', 0)
                                    .arg(lim.exposureMaxUs, 0, 'f', 0)
                                    .arg(lim.gainMinDb, 0, 'f', 1)
                                    .arg(lim.gainMaxDb, 0, 'f', 1)
                                    .arg(lim.fpsMin, 0, 'f', 1)
                                    .arg(lim.fpsMax, 0, 'f', 1));
}

bool QtProject_1::applyCamParams()
{
    return m_camera.setExposureUs(ui.exposureSpin->value())
        && m_camera.setGainDb(ui.gainSpin->value())
        && m_camera.setFps(ui.fpsSpin->value());
}

void QtProject_1::syncSavePath()
{
    m_savePath.setRootPath(ui.savePathEdit->text().trimmed());
    m_savePath.setMode(static_cast<SaveFolderMode>(ui.saveModeCombo->currentIndex()));
    m_savePath.setPicsPerFolder(ui.picsPerFolderSpin->value());
    m_savePath.setSecondsPerFolder(ui.secondsPerFolderSpin->value());
    m_savePath.setMaxSaveCount(ui.maxSaveCountSpin->value());
}

// 扫描磁盘同步 Pic 序号与总保存；阶段/采集中跳过，避免打断进行中的路径上下文
void QtProject_1::resyncSavePathFromDisk()
{
    if (m_shutdownDone || m_stageRunning || m_acquisitionActive)
        return;
    syncSavePath();
    m_savePath.resumeFromDisk();
    refreshQueueDependentUi();
}

void QtProject_1::updateSaveDirWatcher()
{
    const QString path = ui.savePathEdit->text().trimmed();
    const QStringList watched = m_saveDirWatcher.directories();
    for (const QString &p : watched)
        m_saveDirWatcher.removePath(p);
    if (!path.isEmpty() && QDir(path).exists())
        m_saveDirWatcher.addPath(path);
}

void QtProject_1::updateSaveModeUi()
{
    const auto mode = static_cast<SaveFolderMode>(ui.saveModeCombo->currentIndex());
    // 按模式只启用对应的「每文件夹张数 / 秒数」控件
    ui.picsPerFolderLabel->setEnabled(mode == SaveFolderMode::ByCount);
    ui.picsPerFolderSpin->setEnabled(mode == SaveFolderMode::ByCount);
    ui.secondsPerFolderLabel->setEnabled(mode == SaveFolderMode::ByTime);
    ui.secondsPerFolderSpin->setEnabled(mode == SaveFolderMode::ByTime);
}

void QtProject_1::refreshButtonState()
{
    const bool open = m_camera.isOpen();
    const bool stage = m_stageRunning;
    const bool idle = !stage && !m_acquisitionActive; // 非采集会话、非阶段时可编辑阶段表

    ui.openCameraBtn->setEnabled(!open);
    ui.closeCameraBtn->setEnabled(open && !stage); // 阶段运行中不允许关相机
    ui.startGrabBtn->setEnabled(open && !m_acquisitionActive && !stage);
    ui.stopGrabBtn->setEnabled(open && m_acquisitionActive && !stage);
    ui.applyParamBtn->setEnabled(open && !stage);
    ui.saveOneBmpBtn->setEnabled(open && m_acquisitionActive && m_liveViewActive);
    ui.startStageBtn->setEnabled(open && !stage); // 预览常开，可直接启动阶段采集
    ui.stopStageBtn->setEnabled(stage);
    ui.cameraSelectCombo->setEnabled(!open && idle);
    ui.stageTable->setEnabled(idle);
    ui.addStageBtn->setEnabled(idle);
    ui.deleteStageBtn->setEnabled(idle);
    ui.clearStageBtn->setEnabled(idle);
    ui.loopCountSpin->setEnabled(idle);
    // 阶段或手动采集中禁止改存图路径/模式，避免打断阶段存图上下文
    ui.savePathEdit->setEnabled(idle);
    ui.browsePathBtn->setEnabled(idle);
    ui.saveModeCombo->setEnabled(idle);
    ui.maxSaveCountSpin->setEnabled(idle);
    if (idle)
        updateSaveModeUi();
    else
    {
        ui.picsPerFolderLabel->setEnabled(false);
        ui.picsPerFolderSpin->setEnabled(false);
        ui.secondsPerFolderLabel->setEnabled(false);
        ui.secondsPerFolderSpin->setEnabled(false);
    }
}

// 打开相机后启动 grab 与预览定时器
void QtProject_1::startLiveView()
{
    if (!m_camera.isOpen() || m_liveViewActive)
        return;

    applyCamParams();
    if (!m_camera.startGrab())
    {
        log(QStringLiteral("[警告] 实时预览启动失败。"));
        return;
    }
    m_liveViewActive = true;
    m_lastDisplayFrameSeq = 0;
    m_displayTimer.setInterval(33);
    m_displayTimer.start();
    setCamStatus(QStringLiteral("状态: 预览中"), QStringLiteral("rgb(0, 120, 212)"));
}

// 采集会话结束或阶段结束后，确保预览仍在刷新
void QtProject_1::ensureLiveView()
{
    if (!m_camera.isOpen())
        return;

    if (!m_liveViewActive)
        startLiveView();
    else if (!m_displayTimer.isActive())
    {
        m_displayTimer.setInterval(33);
        m_displayTimer.start();
    }

    if (!m_stageRunning)
    {
        if (m_acquisitionActive)
            setCamStatus(QStringLiteral("状态: 采集中"), QStringLiteral("rgb(202, 80, 16)"));
        else
            setCamStatus(QStringLiteral("状态: 预览中"), QStringLiteral("rgb(0, 120, 212)"));
    }
}

// 仅关相机/退出时停止 grab 与预览
void QtProject_1::stopLiveView()
{
    m_displayTimer.stop();
    if (m_liveViewActive)
    {
        m_camera.stopGrab();
        m_liveViewActive = false;
    }
    m_lastDisplayFrameSeq = 0;
}

// 停止阶段业务并等待存图队列；不停止实时预览
void QtProject_1::stopCaptureAndWaitSave(bool userStop)
{
    m_stageRunning = false;

    if (userStop)
        log(QStringLiteral("用户停止，等待存图队列..."));

    waitSaveQueueDrained(true);

    if (m_shutdownDone)
        return;

    setStageStatus(QString());
    ensureLiveView();
    refreshButtonState();
}

// 复制最新帧、生成路径并非阻塞入队；阶段模式下按 frameSeq 去重，避免同一帧重复保存
bool QtProject_1::enqueueCurrentFrame()
{
    QImage frame;
    quint64 frameSeq = 0;
    if (!m_camera.copyLatestImage(frame, &frameSeq) || frame.isNull())
        return false;
    // 阶段存图须基于递增帧序号入队，避免定时器周期短于采集周期时重复保存同一帧
    if (m_stageRunning && frameSeq <= m_lastEnqueuedFrameSeq)
        return false;
    if (m_savePath.isSaveLimitReached())
        return false;

    bool ok = false;
    const QString path = m_savePath.nextFilePath(&ok);
    if (!ok || path.isEmpty())
        return false;

    SaveTask task;
    task.filePath = path;
    task.image = std::move(frame); // 帧数据已在 copyLatestImage 中深拷贝，此处 move 入队
    if (!m_saveThread.trySubmit(task))
        return false;
    if (m_stageRunning)
        m_lastEnqueuedFrameSeq = frameSeq;
    if (!m_shutdownDone)
        refreshQueueDependentUi();
    return true;
}

void QtProject_1::onOpenCamera()
{
    if (m_camera.open())
    {
        updateParamSpinLimits();
        startLiveView(); // 打开相机后启动连续采集与预览定时器；「开始采集」为独立业务会话
        log(m_liveViewActive ? QStringLiteral("相机连接成功，预览已启动。")
                             : QStringLiteral("相机连接成功。"));
    }
    else
    {
        setCamStatus(QStringLiteral("状态: 连接失败"), QStringLiteral("rgb(196, 43, 28)"));
        log(QStringLiteral("[警告] 相机连接失败。"));
    }
    refreshButtonState();
}

void QtProject_1::onCloseCamera()
{
    if (m_stageRunning)
        m_stageMgr.stop();
    m_stageRunning = false;
    // 关相机时退出阶段存图路径模式，避免 SavePathHelper 仍按阶段名分目录
    if (m_savePath.isStageCaptureActive())
        m_savePath.endStageCapture();
    waitSaveQueueDrained();
    m_acquisitionActive = false;
    stopLiveView();
    m_camera.close();
    ui.imageLabel->clearImage();
    ui.imageLabel->setPlaceholderText(QStringLiteral("未连接相机"));
    m_previewBaseInfo.clear();
    m_previewPixelInfo.clear();
    updatePreviewInfoLabel();
    setCamStatus(QStringLiteral("状态: 未连接"), QStringLiteral("rgb(102, 102, 102)"));
    log(QStringLiteral("相机关闭。"));
    refreshButtonState();
}

void QtProject_1::onStartGrab()
{
    if (!m_camera.isOpen())
        return;

    ensureLiveView();
    if (!m_liveViewActive)
        return;

    m_acquisitionActive = true;
    applyCamParams();
    setCamStatus(QStringLiteral("状态: 采集中"), QStringLiteral("rgb(202, 80, 16)"));
    log(QStringLiteral("开始采集。"));
    refreshButtonState();
}

void QtProject_1::onStopGrab()
{
    m_acquisitionActive = false;
    waitSaveQueueDrained();
    ensureLiveView(); // 停止采集会话，预览继续实时刷新
    if (!m_shutdownDone)
        log(QStringLiteral("停止采集，预览继续。"));
    refreshButtonState();
}

void QtProject_1::onApplyParams()
{
    log(applyCamParams() ? QStringLiteral("参数已应用。") : QStringLiteral("[警告] 参数应用失败。"));
}

void QtProject_1::onDisplayTimer()
{
    QImage frame;
    quint64 frameSeq = 0;
    if (!m_camera.copyLatestImage(frame, &frameSeq) || frame.isNull())
        return;

    const QSize viewSize = ui.imageLabel->size();
    // 帧序号与预览区尺寸均未变化时跳过重绘，以降低界面刷新开销
    if (frameSeq == m_lastDisplayFrameSeq && viewSize == m_lastDisplayLabelSize)
        return;

    m_lastDisplayFrameSeq = frameSeq;
    m_lastDisplayLabelSize = viewSize;
    ui.imageLabel->setImage(frame);

    m_previewBaseInfo = QStringLiteral("%1×%2").arg(frame.width()).arg(frame.height());
    if (m_stageRunning)
        m_previewBaseInfo += QStringLiteral(" | 阶段");
    else if (m_acquisitionActive)
        m_previewBaseInfo += QStringLiteral(" | 采集");
    else if (m_liveViewActive)
        m_previewBaseInfo += QStringLiteral(" | 预览");
    const int q = m_saveThread.queueSize();
    if (q > 0)
        m_previewBaseInfo += QStringLiteral(" | 队列%1").arg(q);
    updatePreviewInfoLabel();
}

void QtProject_1::onPreviewPixelInfo(int x, int y, int gray, bool valid)
{
    if (valid)
    {
        m_previewPixelInfo = QStringLiteral(" | 像素(%1,%2)=%3").arg(x).arg(y).arg(gray);
    }
    else
    {
        m_previewPixelInfo.clear();
    }
    updatePreviewInfoLabel();
}

void QtProject_1::updatePreviewInfoLabel()
{
    ui.previewInfoLabel->setText(m_previewBaseInfo + m_previewPixelInfo);
}

void QtProject_1::onSaveOneBmp()
{
    syncSavePath();
    if (enqueueCurrentFrame())
        log(QStringLiteral("手动存图已入队。"));
    else
        log(QStringLiteral("[警告] 手动存图失败。"));
}

void QtProject_1::onBrowseSavePath()
{
    if (m_stageRunning || m_acquisitionActive)
        return;

    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择保存目录"),
                                                          ui.savePathEdit->text());
    if (!dir.isEmpty())
    {
        ui.savePathEdit->setText(dir);
        syncSavePath();
        m_savePath.resetSession();
        m_savePath.resumeFromDisk(); // 切换目录后根据磁盘续接 Pic 序号
        updateSaveDirWatcher();
        refreshQueueDependentUi();
    }
}

void QtProject_1::onCameraError(const QString &message)
{
    if (m_shutdownDone)
        return;
    log(QStringLiteral("[错误] %1").arg(message));
    QMessageBox::warning(this, QStringLiteral("相机"), message);
}

void QtProject_1::onStartStageCapture()
{
    if (!m_camera.isOpen())
    {
        QMessageBox::warning(this, QStringLiteral("阶段采集"), QStringLiteral("请先打开相机。"));
        return;
    }
    if (!validateStageTable())
        return;
    if (ui.savePathEdit->text().trimmed().isEmpty())
    {
        QMessageBox::warning(this, QStringLiteral("阶段采集"), QStringLiteral("请先设置保存路径。"));
        return;
    }

    syncSavePath();
    m_savePath.beginStageCapture(); // 阶段存图：{保存路径}/阶段名/Pic001.bmp
    const QList<StageItem> stages = readStageListFromTable();
    m_stageMgr.setStages(stages);
    const int loopCount = ui.loopCountSpin->value();
    m_stageMgr.setLoopCount(loopCount);
    applyCamParams();

    ensureLiveView();
    if (!m_liveViewActive)
    {
        log(QStringLiteral("[警告] 阶段采集启动失败：预览未就绪。"));
        return;
    }

    m_stageRunning = true;
    m_displayTimer.setInterval(33);
    if (!m_displayTimer.isActive())
        m_displayTimer.start(); // 阶段采集中同步刷新预览
    setCamStatus(QStringLiteral("状态: 阶段采集中"), QStringLiteral("rgb(202, 80, 16)"));
    log(QStringLiteral("开始阶段采集，循环 %1 轮，保存到 %2")
            .arg(loopCount)
            .arg(ui.savePathEdit->text().trimmed()));
    for (const StageItem &st : stages)
    {
        log(QStringLiteral("  · %1：%2s × %3fps，目标 %4 张%5")
                .arg(st.name)
                .arg(st.durationSec, 0, 'f', 1)
                .arg(st.fps, 0, 'f', 1)
                .arg(qRound(st.durationSec * st.fps))
                .arg(st.saveImage ? QStringLiteral("，存图") : QStringLiteral("，仅计时")));
    }
    m_stageMgr.start();
    refreshButtonState();
}

void QtProject_1::onStopStageCapture()
{
    m_stageMgr.stop();
}

void QtProject_1::onStageApplyFps(double fps)
{
    ui.fpsSpin->setValue(fps);
    m_camera.setFps(fps);
}

void QtProject_1::onStageSaveFrame()
{
    if (enqueueCurrentFrame())
    {
        m_stageMgr.notifySaveEnqueued();
        return;
    }
    // 入队失败不计入目标帧数，由 StageManager 触发重试
    m_stageMgr.notifySaveEnqueueFailed();
    // 达到最大存图张数时自动结束阶段采集
    if (m_savePath.isSaveLimitReached())
        m_stageMgr.stop();
}

void QtProject_1::onStageStarted(const QString &name, const QDateTime &startTime, int loopIndex)
{
    m_savePath.setStageContext(loopIndex, name);
    resetEnqueueFrameSeqFromCamera();
    log(QStringLiteral("阶段开始: 第%1轮 %2 → %3/")
            .arg(loopIndex)
            .arg(name)
            .arg(name + QStringLiteral("/")));
    setStageStatus(QStringLiteral("第%1轮 运行中: %2").arg(loopIndex).arg(name));
    Q_UNUSED(startTime);
}

void QtProject_1::onStageFinished(const QString &name,
                                  const QDateTime &,
                                  const QDateTime &,
                                  int,
                                  int saveRequestCount,
                                  int savedCount,
                                  int saveFailCount,
                                  double)
{
    if (saveRequestCount == 0)
    {
        log(QStringLiteral("阶段结束: %1，入队0，已写0（本阶段未勾选存图，或相机无可用帧）")
                .arg(name));
    }
    else if (saveFailCount > 0)
    {
        log(QStringLiteral("阶段结束: %1，入队%2，已写%3，失败%4")
                .arg(name)
                .arg(saveRequestCount)
                .arg(savedCount)
                .arg(saveFailCount));
    }
    else
    {
        log(QStringLiteral("阶段结束: %1，入队%2，已写%3").arg(name).arg(saveRequestCount).arg(savedCount));
    }
    setStageStatus(QStringLiteral("已完成: %1").arg(name));
}

void QtProject_1::onStageLoopStarted(int loopIndex, int totalLoops)
{
    log(QStringLiteral("第 %1/%2 轮开始（存图目录 {保存路径}/阶段名/）。")
            .arg(loopIndex)
            .arg(totalLoops));
    setStageStatus(QStringLiteral("第 %1/%2 轮").arg(loopIndex).arg(totalLoops));
}

void QtProject_1::onStageAllFinished()
{
    log(QStringLiteral("阶段采集全部完成。"));
    m_savePath.endStageCapture();
    stopCaptureAndWaitSave(false);
}

void QtProject_1::onStageStoppedByUser()
{
    m_savePath.endStageCapture();
    stopCaptureAndWaitSave(true);
}

void QtProject_1::onSaveThreadFinished(const QString &path, bool ok, const QString &errorMsg)
{
    if (m_shutdownDone)
        return;

    // 阶段采集中：运行态或 pending 等待写盘计数对齐时，均须将写盘结果通知 StageManager
    const bool inStageCapture = m_savePath.isStageCaptureActive();
    const bool ackStage = inStageCapture
        && (m_stageMgr.isRunning() || m_stageMgr.isPendingStageFinish());

    if (ok)
    {
        m_savePath.onFileSaved();
        if (ackStage)
            m_stageMgr.notifySaveWriteFinished(true);
        else if (!inStageCapture)
            log(QStringLiteral("已保存: %1").arg(path));
    }
    else
    {
        log(QStringLiteral("[错误] 保存失败: %1 %2").arg(path, errorMsg));
        if (ackStage)
            m_stageMgr.notifySaveWriteFinished(false);
    }

    // 存图队列排空后再次尝试完成本阶段（含超时强制完成）
    if (inStageCapture && m_saveThread.queueSize() == 0)
        m_stageMgr.onSaveQueueDrained();

    refreshQueueDependentUi();
}

void QtProject_1::logSaveQueueWarn(const QString &msg)
{
    if (m_shutdownDone)
        return;
    log(msg);
    refreshQueueDependentUi();
}

void QtProject_1::onSaveQueueBacklog(int queueSize)
{
    logSaveQueueWarn(QStringLiteral("[警告] 存图队列积压 %1 张。").arg(queueSize));
}

void QtProject_1::onSaveQueueFull(int queueSize)
{
    logSaveQueueWarn(QStringLiteral("[警告] 存图队列已满(%1/%2)，本帧已拒绝入队。")
                         .arg(queueSize)
                         .arg(m_saveThread.capacity()));
}

void QtProject_1::onAddStage()
{
    if (m_acquisitionActive || m_stageRunning)
        return;
    insertStageRow(ui.stageTable->rowCount(),
                   QStringLiteral("阶段%1").arg(ui.stageTable->rowCount() + 1));
}

// 阶段采集启动前校验：至少一行，名称非空，时长>0，帧率在相机范围内
bool QtProject_1::validateStageTable()
{
    const CamParamLimits lim = m_camera.paramLimits();
    if (ui.stageTable->rowCount() <= 0)
    {
        QMessageBox::warning(this, QStringLiteral("阶段采集"), QStringLiteral("请至少添加一个阶段。"));
        return false;
    }

    for (int r = 0; r < ui.stageTable->rowCount(); ++r)
    {
        auto *nameItem = ui.stageTable->item(r, ColName);
        auto *durItem = ui.stageTable->item(r, ColDuration);
        auto *fpsItem = ui.stageTable->item(r, ColFps);
        // ColName 列：阶段名称非空
        if (!nameItem || nameItem->text().trimmed().isEmpty())
        {
            QMessageBox::warning(this, QStringLiteral("阶段采集"),
                                 QStringLiteral("第 %1 行名称不能为空。").arg(r + 1));
            return false;
        }
        bool ok = false;
        const double dur = durItem ? durItem->text().toDouble(&ok) : 0.0;
        if (!ok || dur <= 0.0)
        {
            QMessageBox::warning(this, QStringLiteral("阶段采集"),
                                 QStringLiteral("第 %1 行时长须大于 0。").arg(r + 1));
            return false;
        }
        ok = false;
        const double fps = fpsItem ? fpsItem->text().toDouble(&ok) : 0.0;
        if (!ok || fps < lim.fpsMin || fps > lim.fpsMax)
        {
            QMessageBox::warning(this, QStringLiteral("阶段采集"),
                                 QStringLiteral("第 %1 行帧率须在 %2~%3。")
                                     .arg(r + 1)
                                     .arg(lim.fpsMin, 0, 'f', 1)
                                     .arg(lim.fpsMax, 0, 'f', 1));
            return false;
        }
    }

    // 全部阶段均未勾选存图时提示，避免用户预期将生成 BMP 文件
    bool anySave = false;
    for (int r = 0; r < ui.stageTable->rowCount(); ++r)
    {
        if (auto *chk = ui.stageTable->item(r, ColSave))
        {
            if (chk->checkState() == Qt::Checked)
            {
                anySave = true;
                break;
            }
        }
    }
    if (!anySave)
    {
        const auto ans = QMessageBox::question(
            this, QStringLiteral("阶段采集"),
            QStringLiteral("所有阶段均未勾选「存图」，将只计时不保存任何图片。\n是否仍要开始？"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ans != QMessageBox::Yes)
            return false;
    }
    return true;
}

QList<StageItem> QtProject_1::readStageListFromTable() const
{
    QList<StageItem> list;
    for (int r = 0; r < ui.stageTable->rowCount(); ++r)
    {
        StageItem s;
        if (auto *it = ui.stageTable->item(r, ColName))
            s.name = it->text().trimmed();
        if (auto *it = ui.stageTable->item(r, ColDuration))
            s.durationSec = it->text().toDouble();
        if (auto *it = ui.stageTable->item(r, ColFps))
            s.fps = it->text().toDouble();
        if (auto *it = ui.stageTable->item(r, ColSave))
            s.saveImage = it->checkState() == Qt::Checked;
        list.append(s);
    }
    return list;
}

void QtProject_1::onDeleteStage()
{
    if (m_acquisitionActive || m_stageRunning)
        return;
    const int row = ui.stageTable->currentRow();
    if (row >= 0)
    {
        ui.stageTable->removeRow(row);
        refreshStageTableSerialNumbers();
    }
}

void QtProject_1::onClearStages()
{
    if (!m_acquisitionActive && !m_stageRunning)
        ui.stageTable->setRowCount(0);
}
