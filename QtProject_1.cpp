// QtProject_1.cpp — 主窗口：信号槽、预览定时器、按钮状态与阶段/存图编排

#include "QtProject_1.h"
#include "core/AppConfig.h"

#include <QCloseEvent>
#include <QPushButton>
#include <QStyle>
#include <QDateTime>
#include <QFileDialog>
#include <QHeaderView>
#include <QMessageBox>
#include <QTableWidgetItem>

namespace
{
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
    // 左右分栏比例：预览区 3，右侧控制 2
    ui.mainSplitter->setStretchFactor(0, 3);
    ui.mainSplitter->setStretchFactor(1, 2);
    ui.imageLabel->setMinimumSize(640, 512);
    ui.logTextEdit->setMaximumBlockCount(2000);
    // 六个主操作按钮通过 btnRole 区分蓝(primary)/红(danger)；禁用为灰
    setStyleSheet(QStringLiteral(
        "QPushButton[btnRole=\"primary\"]{background-color:rgb(0,120,212);color:white;border:none;min-height:28px;}"
        "QPushButton[btnRole=\"primary\"]:hover{background-color:rgb(0,100,180);}"
        "QPushButton[btnRole=\"primary\"]:pressed{background-color:rgb(0,80,150);}"
        "QPushButton[btnRole=\"danger\"]{background-color:rgb(196,43,28);color:white;border:none;min-height:28px;}"
        "QPushButton[btnRole=\"danger\"]:hover{background-color:rgb(160,35,22);}"
        "QPushButton[btnRole=\"danger\"]:pressed{background-color:rgb(130,28,18);}"
        "QPushButton[btnRole=\"primary\"]:disabled,QPushButton[btnRole=\"danger\"]:disabled"
        "{background-color:rgb(200,200,200);color:rgb(240,240,240);}"
        "QPlainTextEdit#logTextEdit{font-family:Consolas,monospace;}"));
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
    connect(&m_camera, &CameraController::errorOccurred, this, &QtProject_1::onCameraError);

    AppConfig::loadUi(ui);
    updateSaveModeUi();
    syncSavePath();
    m_savePath.resumeFromDisk(); // 启动时续接已有 Pic 序号，避免覆盖

    m_displayTimer.setInterval(50); // 预览约 20Hz；阶段采集时会改为 33ms
    connect(&m_displayTimer, &QTimer::timeout, this, &QtProject_1::onDisplayTimer);
    m_saveThread.start();

    log(m_camera.initPylon() ? QStringLiteral("Pylon 初始化成功。")
                              : QStringLiteral("[警告] Pylon 初始化失败。"));
    log(QStringLiteral("软件启动。"));
    refreshButtonState();
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

// 只执行一次：停阶段、断信号、停采、等存图、关相机、PylonTerminate
void QtProject_1::shutdownAll()
{
    if (m_shutdownDone)
        return;
    m_shutdownDone = true;

    m_displayTimer.stop();
    if (m_stageRunning)
    {
        m_stageMgr.stop();
        m_stageRunning = false;
    }

    disconnect(&m_camera, nullptr, this, nullptr);
    disconnect(&m_saveThread, nullptr, this, nullptr);
    disconnect(&m_stageMgr, nullptr, this, nullptr);

    stopCaptureAndWaitSave(false);
    m_saveThread.requestStopAndWait(30000);

    if (m_camera.isOpen())
        m_camera.close();
    if (m_camera.isPylonInitialized())
        m_camera.shutdownPylon();
}

void QtProject_1::log(const QString &msg)
{
    if (m_shutdownDone)
        return;
    ui.logTextEdit->appendPlainText(QStringLiteral("[%1] %2")
                                        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss")),
                                             msg));
}

// 相机连接状态文字 + 颜色
void QtProject_1::setCamStatus(const QString &text, const QString &color)
{
    ui.cameraStatusLabel->setText(text);
    ui.cameraStatusLabel->setStyleSheet(
        QStringLiteral("color:%1;font-weight:bold;padding:4px 0;").arg(color));
}

// 阶段状态栏；运行中且存图队列非空时追加队列长度
void QtProject_1::setStageStatus(const QString &text)
{
    m_stageStatusText = text;
    QString label = m_stageStatusText.isEmpty() ? QStringLiteral("当前无运行中的阶段") : m_stageStatusText;
    if (m_stageRunning)
    {
        const int pending = m_saveThread.queueSize();
        if (pending > 0)
            label += QStringLiteral(" | 队列 %1").arg(pending);
    }
    ui.stageStatusLabel->setText(label);
}

void QtProject_1::insertStageRow(int row, const QString &name)
{
    ui.stageTable->insertRow(row);
    ui.stageTable->setItem(row, 0, new QTableWidgetItem(name));
    ui.stageTable->setItem(row, 1, new QTableWidgetItem(QStringLiteral("1.0")));
    ui.stageTable->setItem(row, 2, new QTableWidgetItem(QStringLiteral("20")));
    auto *chk = new QTableWidgetItem();
    chk->setCheckState(Qt::Unchecked);
    ui.stageTable->setItem(row, 3, chk);
}

void QtProject_1::setupStageTable()
{
    auto *t = ui.stageTable;
    t->setColumnCount(4);
    t->setHorizontalHeaderLabels({QStringLiteral("阶段名称"), QStringLiteral("时长(s)"),
                                  QStringLiteral("帧率(fps)"), QStringLiteral("存图")});
    t->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
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
    const bool grab = m_capturing;
    const bool stage = m_stageRunning;
    const bool idle = !stage && !grab; // 非采集非阶段时可编辑阶段表

    ui.openCameraBtn->setEnabled(!open);
    ui.closeCameraBtn->setEnabled(open && !grab); // 采集中不允许关相机
    ui.startGrabBtn->setEnabled(open && !grab && !stage);
    ui.stopGrabBtn->setEnabled(open && grab && !stage);
    ui.applyParamBtn->setEnabled(open && !stage);
    ui.saveOneBmpBtn->setEnabled(open && grab);
    ui.startStageBtn->setEnabled(open && !grab);
    ui.stopStageBtn->setEnabled(stage);
    ui.cameraSelectCombo->setEnabled(!open && idle);
    ui.stageTable->setEnabled(idle);
    ui.addStageBtn->setEnabled(idle);
    ui.deleteStageBtn->setEnabled(idle);
    ui.clearStageBtn->setEnabled(idle);
    ui.loopCountSpin->setEnabled(idle);

    updateActionButtonStyles();
}

void QtProject_1::updateActionButtonStyles()
{
    // 正向操作：蓝色；停止/关闭：红色（可执行时由 refreshButtonState 控制 enabled）
    applyButtonRole(ui.openCameraBtn, "primary");
    applyButtonRole(ui.startGrabBtn, "primary");
    applyButtonRole(ui.startStageBtn, "primary");
    applyButtonRole(ui.closeCameraBtn, "danger");
    applyButtonRole(ui.stopGrabBtn, "danger");
    applyButtonRole(ui.stopStageBtn, "danger");
}

// 停止 grab/阶段，可选等待存图队列排空，并恢复状态栏与按钮
void QtProject_1::stopCaptureAndWaitSave(bool userStop)
{
    m_displayTimer.stop();
    m_displayTimer.setInterval(50);
    if (m_capturing)
    {
        m_camera.stopGrab();
        m_capturing = false;
    }
    m_stageRunning = false;

    if (userStop)
        log(QStringLiteral("用户停止，等待存图队列..."));

    if (m_saveThread.queueSize() > 0)
    {
        m_saveThread.waitUntilEmpty(30000);
        if (m_saveThread.queueSize() > 0)
            log(QStringLiteral("[警告] 仍有图片未保存完成。"));
    }

    if (m_shutdownDone)
        return;

    setStageStatus(QString());
    setCamStatus(m_camera.isOpen() ? QStringLiteral("状态: 已连接") : QStringLiteral("状态: 未连接"),
                 m_camera.isOpen() ? QStringLiteral("rgb(16, 124, 16)") : QStringLiteral("rgb(102, 102, 102)"));
    refreshButtonState();
}

bool QtProject_1::enqueueCurrentFrame()
{
    QImage frame;
    quint64 frameSeq = 0;
    if (!m_camera.copyLatestImage(frame, &frameSeq) || frame.isNull())
        return false;
    // 阶段存图：必须等新帧，避免 tick 快于相机时重复保存同一静止画面
    if (m_stageRunning && frameSeq <= m_lastEnqueuedFrameSeq)
        return false;
    if (m_savePath.isSaveLimitReached())
        return false;

    bool ok = false;
    const QString path = m_savePath.nextFilePath(&ok);
    if (!ok || path.isEmpty())
        return false;

    SaveTask task{frame.copy(), path};
    m_saveThread.enqueue(task);
    if (m_stageRunning)
        m_lastEnqueuedFrameSeq = frameSeq;
    return true;
}

void QtProject_1::onOpenCamera()
{
    if (m_camera.open())
    {
        updateParamSpinLimits();
        applyCamParams();
        setCamStatus(QStringLiteral("状态: 已连接"), QStringLiteral("rgb(16, 124, 16)"));
        log(QStringLiteral("相机连接成功。"));
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
    stopCaptureAndWaitSave(false);
    m_camera.close();
    ui.imageLabel->clear();
    ui.imageLabel->setText(QStringLiteral("未连接相机"));
    setCamStatus(QStringLiteral("状态: 未连接"), QStringLiteral("rgb(102, 102, 102)"));
    log(QStringLiteral("相机关闭。"));
    refreshButtonState();
}

void QtProject_1::onStartGrab()
{
    applyCamParams();
    if (!m_camera.startGrab())
    {
        log(QStringLiteral("[警告] 开始采集失败。"));
        return;
    }
    m_capturing = true;
    m_displayTimer.start();
    setCamStatus(QStringLiteral("状态: 预览中"), QStringLiteral("rgb(0, 120, 212)"));
    log(QStringLiteral("开始采集。"));
    refreshButtonState();
}

void QtProject_1::onStopGrab()
{
    stopCaptureAndWaitSave(false);
    if (!m_shutdownDone)
        log(QStringLiteral("停止采集。"));
}

void QtProject_1::onApplyParams()
{
    log(applyCamParams() ? QStringLiteral("参数已应用。") : QStringLiteral("[警告] 参数应用失败。"));
}

void QtProject_1::onDisplayTimer()
{
    QImage frame;
    if (!m_camera.copyLatestImage(frame) || frame.isNull())
        return;

    ui.imageLabel->setPixmap(QPixmap::fromImage(frame).scaled(
        ui.imageLabel->size(), Qt::KeepAspectRatio, Qt::FastTransformation));
    QString info = QStringLiteral("%1×%2").arg(frame.width()).arg(frame.height());
    if (m_stageRunning)
        info += QStringLiteral(" | 阶段");
    else if (m_capturing)
        info += QStringLiteral(" | 预览");
    const int q = m_saveThread.queueSize();
    if (q > 0)
        info += QStringLiteral(" | 队列%1").arg(q);
    ui.previewInfoLabel->setText(info);
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
    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择保存目录"),
                                                          ui.savePathEdit->text());
    if (!dir.isEmpty())
    {
        ui.savePathEdit->setText(dir);
        syncSavePath();
        m_savePath.resetSession();
        m_savePath.resumeFromDisk(); // 换目录后从该目录已有 Pic 续接
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
    m_savePath.syncIndexWithDisk(); // 只对齐磁盘，不回退 Pic 序号（避免覆盖）
    m_stageMgr.setStages(readStageListFromTable());
    const int loopCount = ui.loopCountSpin->value();
    m_stageMgr.setLoopCount(loopCount);
    applyCamParams();

    // 已在预览采集时不必重复 startGrab，否则返回 false 导致阶段无法启动
    if (!m_capturing)
    {
        if (!m_camera.startGrab())
        {
            log(QStringLiteral("[警告] 阶段采集启动失败。"));
            return;
        }
        m_capturing = true;
    }

    m_lastEnqueuedFrameSeq = 0;
    {
        QImage warmFrame;
        quint64 warmSeq = 0;
        if (m_camera.copyLatestImage(warmFrame, &warmSeq))
            m_lastEnqueuedFrameSeq = warmSeq;
    }

    m_stageRunning = true;
    m_displayTimer.setInterval(33); // 阶段采集预览约 30Hz，更接近实时
    m_displayTimer.start();
    setCamStatus(QStringLiteral("状态: 阶段采集中"), QStringLiteral("rgb(202, 80, 16)"));
    log(QStringLiteral("开始阶段采集，循环 %1 轮。").arg(loopCount));
    ui.rightTabWidget->setCurrentWidget(ui.stageSaveTab);
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
    // 入队失败时不计入目标帧数，由 StageManager 立即重试
    m_stageMgr.notifySaveEnqueueFailed();
    // 达到最大存图张数时自动结束阶段采集
    if (m_savePath.isSaveLimitReached())
        m_stageMgr.stop();
}

void QtProject_1::onStageStarted(const QString &name, const QDateTime &startTime)
{
    log(QStringLiteral("阶段开始: %1").arg(name));
    setStageStatus(QStringLiteral("运行中: %1").arg(name));
    Q_UNUSED(startTime);
}

void QtProject_1::onStageFinished(const QString &name,
                                  const QDateTime &,
                                  const QDateTime &,
                                  int,
                                  int saveRequestCount,
                                  int savedCount,
                                  double)
{
    log(QStringLiteral("阶段结束: %1，入队%2，已写%3").arg(name).arg(saveRequestCount).arg(savedCount));
    setStageStatus(QStringLiteral("已完成: %1").arg(name));
}

void QtProject_1::onStageLoopStarted(int loopIndex, int totalLoops)
{
    log(QStringLiteral("第 %1/%2 轮开始（Pic 编号继续累加）。").arg(loopIndex).arg(totalLoops));
    setStageStatus(QStringLiteral("第 %1/%2 轮").arg(loopIndex).arg(totalLoops));
}

void QtProject_1::onStageAllFinished()
{
    log(QStringLiteral("阶段采集全部完成。"));
    stopCaptureAndWaitSave(false);
}

void QtProject_1::onStageStoppedByUser()
{
    stopCaptureAndWaitSave(true);
}

void QtProject_1::onSaveThreadFinished(const QString &path, bool ok, const QString &errorMsg)
{
    if (m_shutdownDone)
        return;
    if (ok)
    {
        m_savePath.onFileSaved();
        if (m_stageRunning)
        {
            m_stageMgr.notifyImageSaved();
            setStageStatus(m_stageStatusText); // 刷新队列计数
        }
        else
        {
            log(QStringLiteral("已保存: %1").arg(path));
        }
    }
    else
    {
        log(QStringLiteral("[错误] 保存失败: %1 %2").arg(path, errorMsg));
    }
}

void QtProject_1::onSaveQueueBacklog(int queueSize)
{
    if (!m_shutdownDone)
    {
        log(QStringLiteral("[警告] 存图队列积压 %1 张。").arg(queueSize));
        setStageStatus(m_stageStatusText);
    }
}

void QtProject_1::onAddStage()
{
    if (m_capturing || m_stageRunning)
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
        auto *nameItem = ui.stageTable->item(r, 0);
        auto *durItem = ui.stageTable->item(r, 1);
        auto *fpsItem = ui.stageTable->item(r, 2);
        // 列 0：阶段名
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
    return true;
}

QList<StageItem> QtProject_1::readStageListFromTable() const
{
    QList<StageItem> list;
    for (int r = 0; r < ui.stageTable->rowCount(); ++r)
    {
        StageItem s;
        if (auto *it = ui.stageTable->item(r, 0))
            s.name = it->text().trimmed();
        if (auto *it = ui.stageTable->item(r, 1))
            s.durationSec = it->text().toDouble();
        if (auto *it = ui.stageTable->item(r, 2))
            s.fps = it->text().toDouble();
        if (auto *it = ui.stageTable->item(r, 3))
            s.saveImage = it->checkState() == Qt::Checked;
        list.append(s);
    }
    return list;
}

void QtProject_1::onDeleteStage()
{
    if (m_capturing || m_stageRunning)
        return;
    const int row = ui.stageTable->currentRow();
    if (row >= 0)
        ui.stageTable->removeRow(row);
}

void QtProject_1::onClearStages()
{
    if (!m_capturing && !m_stageRunning)
        ui.stageTable->setRowCount(0);
}
