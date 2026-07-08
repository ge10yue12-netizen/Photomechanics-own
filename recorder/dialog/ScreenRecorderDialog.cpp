#include "ScreenRecorderDialog.h"
#include "ui_ScreenRecorderDialog.h"
#include "ScreenRecorderSettingsDialog.h"

#include "../RecorderFloatBall.h"
#include "../RecorderControlBar.h"
#include "../RecorderController.h"
#include "../RecorderOutputListWidget.h"
#include "../RecorderPathHelper.h"
#include "../RecorderPreviewCapture.h"
#include "../RecorderVideoProbe.h"
#include "../RecorderStatusText.h"
#include "../RecorderUiBinder.h"
#include "../RecorderComboStyle.h"
#include "../RegionSelectorWidget.h"
#include "../include/RecorderWindowTarget.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QLineEdit>
#include <QPushButton>
#include <QShowEvent>
#include <QSize>
#include <QTimer>
#include <QUrl>

ScreenRecorderDialog::ScreenRecorderDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ScreenRecorderDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose, false);

    m_previewTimer = new QTimer(this);
    m_previewTimer->setInterval(kPreviewIntervalIdleMs);
    connect(m_previewTimer, &QTimer::timeout, this, &ScreenRecorderDialog::onPreviewTimer);

    m_durationTimer = new QTimer(this);
    m_durationTimer->setInterval(kDurationTickMs);
    connect(m_durationTimer, &QTimer::timeout, this, &ScreenRecorderDialog::onDurationTimer);

    m_outputDirResyncTimer.setSingleShot(true);
    m_outputDirResyncTimer.setInterval(400);
    connect(&m_outputDirWatcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &) {
        m_outputDirResyncTimer.start();
    });
    connect(&m_outputDirResyncTimer, &QTimer::timeout, this, &ScreenRecorderDialog::resyncOutputListFromDisk);

    wireUi();
    wireFloatBall();
}

ScreenRecorderDialog::~ScreenRecorderDialog()
{
    delete m_floatBall;
    delete ui;
}

void ScreenRecorderDialog::bindController(RecorderController *controller)
{
    m_controller = controller;
    wireController();
    if (m_controller)
        applyStateToUi(m_controller->state());
}

void ScreenRecorderDialog::setFixedWindowTarget(recorder::IRecorderWindowTarget *target, const QString &label)
{
    m_windowTarget = target;
    m_windowTargetLabel = label.trimmed();
    refreshCaptureModeCombo();
}

void ScreenRecorderDialog::refreshCaptureModeCombo()
{
    if (!ui || !ui->regionCombo)
        return;

    const auto prevMode = static_cast<recorder::CaptureMode>(ui->regionCombo->currentData().toInt());

    m_blockRegionCombo = true;
    RecorderUiBinder::fillCaptureModeCombo(ui->regionCombo);
    if (m_windowTarget && !m_windowTargetLabel.isEmpty())
    {
        ui->regionCombo->addItem(m_windowTargetLabel,
                                 static_cast<int>(recorder::CaptureMode::Window));
    }

    const int idx = ui->regionCombo->findData(static_cast<int>(prevMode));
    if (idx >= 0)
        ui->regionCombo->setCurrentIndex(idx);
    else if (ui->regionCombo->count() > 0)
        ui->regionCombo->setCurrentIndex(0);

    m_blockRegionCombo = false;
    refreshRegionSummary();
}

std::uintptr_t ScreenRecorderDialog::currentWindowHandle() const
{
    if (!m_windowTarget)
        return 0;
    return m_windowTarget->windowHandle();
}

void ScreenRecorderDialog::syncWindowVisualConsumers()
{
    if (!m_windowTarget)
        return;

    int flags = 0;
    const auto mode = static_cast<recorder::CaptureMode>(ui->regionCombo->currentData().toInt());
    if (mode == recorder::CaptureMode::Window)
    {
        if (isVisible() && isGeneralPage())
            flags |= static_cast<int>(recorder::VisualConsumerFlag::LivePreview);

        if (m_controller)
        {
            const recorder::RecorderState st = m_controller->state();
            if (st == recorder::RecorderState::Recording || st == recorder::RecorderState::Paused)
                flags |= static_cast<int>(recorder::VisualConsumerFlag::Recording);
        }
    }

    m_windowTarget->setVisualConsumerFlags(flags);
}

int ScreenRecorderDialog::resolveSavedVideoDurationSeconds(int fallbackSeconds) const
{
    if (m_activeRecordPath.isEmpty())
        return fallbackSeconds < 0 ? 0 : fallbackSeconds;
    return RecorderVideoProbe::durationSecondsWithFallback(m_activeRecordPath, fallbackSeconds);
}

void ScreenRecorderDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    m_outputStore.setOutputDirectory(currentOutputDirectory());
    m_outputStore.load(nullptr);
    ui->outputList->setStore(&m_outputStore);
    ensureDefaultPath();
    updatePresetSummary();
    refreshRegionSummary();
    refreshStatusText();
    syncWindowVisualConsumers();
    refreshPreviewCapture();
    updatePreviewTimerForPage();
    updateOutputDirWatcher();
}

void ScreenRecorderDialog::changeEvent(QEvent *event)
{
    QDialog::changeEvent(event);
    // 从资源管理器切回时 resync，覆盖 watcher 未触达的情况
    if (event->type() == QEvent::ActivationChange && isActiveWindow() && isVisible())
        resyncOutputListFromDisk();
}

void ScreenRecorderDialog::hideEvent(QHideEvent *event)
{
    QDialog::hideEvent(event);
    m_previewTimer->stop();
    invalidatePreviewSessions();
    syncWindowVisualConsumers();
}

void ScreenRecorderDialog::closeEvent(QCloseEvent *event)
{
    event->ignore();
    hide();
}

void ScreenRecorderDialog::wireUi()
{
    ui->generalVBox->setStretch(0, 1);
    ui->generalVBox->setStretch(1, 0);
    ui->previewCol->setStretch(0, 1);
    ui->controlBarRow->setStretch(4, 1);

    RecorderUiBinder::fillCaptureModeCombo(ui->regionCombo);
    RecorderComboStyle::applyTo(ui->regionCombo);

    connect(ui->navSidebar, &RecorderNavSidebar::pageChanged, this, &ScreenRecorderDialog::onNavPageChanged);
    connect(ui->regionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScreenRecorderDialog::onRegionComboChanged);
    connect(ui->reselectRegionBtn, &QPushButton::clicked, this, &ScreenRecorderDialog::onReselectRegionClicked);
    connect(ui->controlBar, &RecorderControlBar::startClicked, this, &ScreenRecorderDialog::onStartClicked);
    connect(ui->controlBar, &RecorderControlBar::pauseClicked, this, &ScreenRecorderDialog::onPauseClicked);
    connect(ui->controlBar, &RecorderControlBar::resumeClicked, this, &ScreenRecorderDialog::onResumeClicked);
    connect(ui->controlBar, &RecorderControlBar::stopClicked, this, &ScreenRecorderDialog::onStopClicked);

    if (ui->settingsPanel)
    {
        connect(ui->settingsPanel->browseBtn(), &QPushButton::clicked, this, &ScreenRecorderDialog::onBrowsePathClicked);
        connect(ui->settingsPanel->pathEdit(), &QLineEdit::textChanged, this, &ScreenRecorderDialog::onPathSettingsChanged);
        connect(ui->settingsPanel, &ScreenRecorderSettingsDialog::presetChanged,
                this, &ScreenRecorderDialog::onSettingsChanged);
    }

    m_lastCaptureMode = recorder::CaptureMode::FullScreen;
    onRegionComboChanged(ui->regionCombo->currentIndex());
    applyStateToUi(recorder::RecorderState::Idle);
}

void ScreenRecorderDialog::wireFloatBall()
{
    m_floatBall = new RecorderFloatBall();
    connect(m_floatBall, &RecorderFloatBall::pauseClicked, this, &ScreenRecorderDialog::onPauseClicked);
    connect(m_floatBall, &RecorderFloatBall::resumeClicked, this, &ScreenRecorderDialog::onResumeClicked);
    connect(m_floatBall, &RecorderFloatBall::stopClicked, this, &ScreenRecorderDialog::onStopClicked);
    connect(m_floatBall, &RecorderFloatBall::openPanelRequested, this, [this]() {
        show();
        raise();
        activateWindow();
    });
}

bool ScreenRecorderDialog::isRecordingActive() const
{
    if (!m_controller)
        return false;
    const recorder::RecorderState st = m_controller->state();
    return st == recorder::RecorderState::Recording || st == recorder::RecorderState::Paused;
}

void ScreenRecorderDialog::syncFloatBall()
{
    if (!m_floatBall || !m_controller)
        return;

    if (isRecordingActive())
    {
        m_floatBall->setRecorderState(m_controller->state());
        if (!m_floatBall->isVisible())
            m_floatBall->show();
    }
    else
    {
        hideFloatBall();
    }
}

void ScreenRecorderDialog::showFloatBallForRecording()
{
    if (!m_floatBall || !m_controller)
        return;

    m_floatBall->setRecorderState(m_controller->state());
    m_floatBall->setDurationSeconds(m_controller->recordedSeconds());
    m_floatBall->show();
    m_floatBall->raise();
}

void ScreenRecorderDialog::hideFloatBall()
{
    if (m_floatBall)
        m_floatBall->hide();
}

void ScreenRecorderDialog::wireController()
{
    if (!m_controller)
        return;
    disconnect(m_controller, nullptr, this, nullptr);
    connect(m_controller, &RecorderController::stateChanged, this, &ScreenRecorderDialog::onControllerStateChanged);
    connect(m_controller, &RecorderController::durationChanged, this, &ScreenRecorderDialog::onControllerDurationChanged);
    connect(m_controller, &RecorderController::errorOccurred, this, &ScreenRecorderDialog::onControllerError);
}

void ScreenRecorderDialog::onNavPageChanged(RecorderNavSidebar::Page page)
{
    ui->stack->setCurrentIndex(static_cast<int>(page));
    if (page == RecorderNavSidebar::List)
    {
        updateOutputDirWatcher();
        resyncOutputListFromDisk();
    }
    else if (page == RecorderNavSidebar::Settings)
    {
        updatePresetSummary();
        updatePresetSummary();
    }
    updatePreviewTimerForPage();
    syncWindowVisualConsumers();
}

void ScreenRecorderDialog::ensureDefaultPath()
{
    QString err;
    RecorderPathHelper::ensureRecordRootExists(&err);
    QLineEdit *pathEdit = ui->settingsPanel ? ui->settingsPanel->pathEdit() : nullptr;
    if (pathEdit && (pathEdit->text().trimmed().isEmpty() || !m_pathInitialized))
    {
        pathEdit->setText(RecorderPathHelper::recordRootDir());
        m_pathInitialized = true;
    }
    ui->controlBar->setDurationSeconds(0);
    updatePathTooltip();
}

QString ScreenRecorderDialog::currentOutputDirectory() const
{
    const QString raw = ui->settingsPanel && ui->settingsPanel->pathEdit()
                            ? ui->settingsPanel->pathEdit()->text().trimmed()
                            : QString();
    return RecorderPathHelper::normalizeOutputDirectory(raw);
}

void ScreenRecorderDialog::updatePathTooltip()
{
    if (!ui->settingsPanel || !ui->settingsPanel->pathEdit())
        return;
    ui->settingsPanel->pathEdit()->setToolTip(ui->settingsPanel->pathEdit()->text().trimmed());
}

void ScreenRecorderDialog::invalidatePreviewSessions()
{
    RecorderPreviewCapture::releaseSessions();
}

recorder::VideoFormat ScreenRecorderDialog::currentFormat() const
{
    if (!ui->settingsPanel || !ui->settingsPanel->formatCombo())
        return recorder::VideoFormat::Mp4;
    return static_cast<recorder::VideoFormat>(ui->settingsPanel->formatCombo()->currentData().toInt());
}

void ScreenRecorderDialog::refreshRegionSummary()
{
    const auto mode = static_cast<recorder::CaptureMode>(ui->regionCombo->currentData().toInt());
    const bool regionMode = mode == recorder::CaptureMode::Region;
    const bool windowMode = mode == recorder::CaptureMode::Window;
    ui->reselectRegionBtn->setVisible(regionMode);

    QString tip;
    if (windowMode)
    {
        tip = m_windowTargetLabel.isEmpty()
                  ? QString::fromUtf8(recorder::captureModeLabel(recorder::CaptureMode::Window))
                  : m_windowTargetLabel;
        if (m_windowTarget && !m_windowTarget->isAvailable())
            tip += QStringLiteral("（预览不可用）");
    }
    else if (!regionMode)
    {
        tip = QString::fromUtf8(recorder::captureModeLabel(recorder::CaptureMode::FullScreen));
    }
    else if (!m_regionValid)
    {
        tip = QStringLiteral("区域录制：尚未选择区域");
    }
    else
    {
        tip = QStringLiteral("区域 %1×%2 @ (%3, %4)")
                  .arg(m_region.width)
                  .arg(m_region.height)
                  .arg(m_region.x)
                  .arg(m_region.y);
    }
    ui->regionCombo->setToolTip(tip);
}

void ScreenRecorderDialog::updatePresetSummary()
{
    if (!ui->settingsPanel)
        return;

    const auto mode = static_cast<recorder::CaptureMode>(ui->regionCombo->currentData().toInt());
    const QSize size =
        RecorderUiBinder::captureSourceSize(mode, m_region, m_regionValid, currentWindowHandle());
    ui->settingsPanel->updatePresetSummary(size.width(), size.height());
}

recorder::CaptureMode ScreenRecorderDialog::currentPreviewMode() const
{
    const auto mode = static_cast<recorder::CaptureMode>(ui->regionCombo->currentData().toInt());
    return RecorderUiBinder::previewCaptureMode(mode, m_regionValid);
}

void ScreenRecorderDialog::refreshPreviewCapture()
{
    const auto mode = currentPreviewMode();
    QImage previewImage;
    QString err;

    const bool ok = RecorderPreviewCapture::grabFrame(mode,
                                                      m_region,
                                                      m_regionValid,
                                                      m_windowTarget,
                                                      &previewImage,
                                                      &err);
    if (ok)
        ui->mainPreview->setPreviewImage(previewImage);
    // 单帧失败保留上一帧，避免预览闪黑。
}

void ScreenRecorderDialog::applyStateToUi(recorder::RecorderState state)
{
    const bool idleLike = state == recorder::RecorderState::Idle ||
                          state == recorder::RecorderState::Initialized ||
                          state == recorder::RecorderState::Stopped ||
                          state == recorder::RecorderState::Error;
    const bool recording = state == recorder::RecorderState::Recording;
    const bool paused = state == recorder::RecorderState::Paused;

    ui->regionCombo->setEnabled(idleLike);
    ui->reselectRegionBtn->setEnabled(idleLike);
    if (ui->navSidebar)
        ui->navSidebar->setSettingsEnabled(idleLike);
    if (ui->settingsPanel)
        ui->settingsPanel->setControlsEnabled(idleLike);
    if (ui->outputList)
        ui->outputList->setControlsEnabled(idleLike);
    ui->controlBar->setRecorderState(state);

    const bool recordingLike = recording || paused;
    if (recordingLike)
    {
        if (!m_durationTimer->isActive())
            m_durationTimer->start();
        updatePreviewTimerForPage();
    }
    else
    {
        m_durationTimer->stop();
        updatePreviewTimerForPage();
    }
}

void ScreenRecorderDialog::refreshStatusText()
{
    const recorder::RecorderState st = m_controller ? m_controller->state() : recorder::RecorderState::Idle;
    const QString err = m_controller ? m_controller->lastError() : QString();
    const QString text = RecorderStatusText::sessionSummary(recorder::stateLabel(st), err);
    ui->statusLabel->setText(text);
    if (!err.isEmpty())
        ui->statusLabel->setStyleSheet(QStringLiteral("color: #D32F2F; font-size: 12px;"));
    else
        ui->statusLabel->setStyleSheet(QStringLiteral("color: #666; font-size: 12px;"));
}

recorder::RecorderConfig ScreenRecorderDialog::currentConfig() const
{
    const auto mode = static_cast<recorder::CaptureMode>(ui->regionCombo->currentData().toInt());
    const QSize capSize =
        RecorderUiBinder::captureSourceSize(mode, m_region, m_regionValid, currentWindowHandle());

    recorder::RecorderConfig cfg = RecorderUiBinder::configFromWidgets(
        ui->regionCombo,
        ui->settingsPanel ? ui->settingsPanel->qualityCombo() : nullptr,
        ui->settingsPanel ? ui->settingsPanel->encodeCombo() : nullptr,
        ui->settingsPanel ? ui->settingsPanel->fpsCombo() : nullptr,
        ui->settingsPanel ? ui->settingsPanel->formatCombo() : nullptr,
        ui->settingsPanel ? ui->settingsPanel->pathEdit() : nullptr,
        m_region,
        capSize.width(),
        capSize.height());
    if (cfg.mode == recorder::CaptureMode::Window && m_windowTarget)
        recorder::applyWindowTarget(&cfg, m_windowTarget);
    return cfg;
}

bool ScreenRecorderDialog::beginRegionSelection()
{
    hide();

    RegionSelectorWidget selector(nullptr);
    recorder::Rect region;
    const bool ok = selector.run(&region);

    show();
    raise();
    activateWindow();

    if (!ok)
        return false;

    m_region = region;
    m_regionValid = true;
    invalidatePreviewSessions();
    updatePresetSummary();
    refreshRegionSummary();
    refreshPreviewCapture();
    return true;
}

void ScreenRecorderDialog::handleStopFinished(int finalDurationSeconds)
{
    Q_UNUSED(finalDurationSeconds);

    if (!m_activeRecordPath.isEmpty())
        resyncOutputListFromDisk();

    showSavedToast();

    m_activeRecordPath.clear();
    ui->controlBar->setDurationSeconds(0);
    hideFloatBall();
    show();
    raise();
    applyStateToUi(m_controller ? m_controller->state() : recorder::RecorderState::Stopped);
    refreshStatusText();
    refreshPreviewCapture();
}

void ScreenRecorderDialog::showSavedToast()
{
    auto *msg = new QMessageBox(QMessageBox::Information,
                                QStringLiteral("屏幕录制"),
                                QStringLiteral("录制已保存。"),
                                QMessageBox::NoButton,
                                this);
    msg->setAttribute(Qt::WA_DeleteOnClose);
    msg->setWindowModality(Qt::NonModal);
    msg->show();
    QTimer::singleShot(2000, msg, &QWidget::close);
}

void ScreenRecorderDialog::onBrowsePathClicked()
{
    if (!ui->settingsPanel || !ui->settingsPanel->pathEdit())
        return;

    const QString dir = QFileDialog::getExistingDirectory(this,
                                                            QStringLiteral("选择保存目录"),
                                                            currentOutputDirectory());
    if (!dir.isEmpty())
        ui->settingsPanel->pathEdit()->setText(QDir(dir).absolutePath());
}

void ScreenRecorderDialog::onSettingsChanged()
{
    updatePresetSummary();
}

void ScreenRecorderDialog::onPathSettingsChanged()
{
    updatePathTooltip();
    updateOutputDirWatcher();
    resyncOutputListFromDisk();
}

void ScreenRecorderDialog::onReselectRegionClicked()
{
    if (static_cast<recorder::CaptureMode>(ui->regionCombo->currentData().toInt()) != recorder::CaptureMode::Region)
        return;
    invalidatePreviewSessions();
    beginRegionSelection();
}

void ScreenRecorderDialog::onPreviewTimer()
{
    if (!isGeneralPage())
        return;
    if (m_previewGrabbing)
        return;
    m_previewGrabbing = true;
    refreshPreviewCapture();
    m_previewGrabbing = false;
}

bool ScreenRecorderDialog::isGeneralPage() const
{
    return ui->navSidebar && ui->navSidebar->currentPage() == RecorderNavSidebar::General;
}

bool ScreenRecorderDialog::isListPage() const
{
    return ui->navSidebar && ui->navSidebar->currentPage() == RecorderNavSidebar::List;
}

void ScreenRecorderDialog::updateOutputDirWatcher()
{
    const QString path = currentOutputDirectory();
    const QStringList watched = m_outputDirWatcher.directories();
    for (const QString &p : watched)
        m_outputDirWatcher.removePath(p);
    if (!path.isEmpty() && QDir(path).exists())
        m_outputDirWatcher.addPath(path);
}

void ScreenRecorderDialog::resyncOutputListFromDisk()
{
    if (!isVisible())
        return;

    m_outputStore.setOutputDirectory(currentOutputDirectory());
    m_outputStore.rebuildFromDirectory(nullptr);
    if (isListPage() && ui->outputList)
        ui->outputList->refreshView();
}

void ScreenRecorderDialog::updatePreviewTimerForPage()
{
    if (!m_previewTimer)
        return;

    const recorder::RecorderState st =
        m_controller ? m_controller->state() : recorder::RecorderState::Idle;
    const bool idleLike = st == recorder::RecorderState::Idle ||
                          st == recorder::RecorderState::Initialized ||
                          st == recorder::RecorderState::Stopped ||
                          st == recorder::RecorderState::Error;
    const bool recordingLike = st == recorder::RecorderState::Recording ||
                               st == recorder::RecorderState::Paused;
    if (isVisible() && isGeneralPage() && (idleLike || recordingLike))
    {
        if (!m_previewTimer->isActive())
            m_previewTimer->start();
    }
    else
    {
        m_previewTimer->stop();
    }
}

void ScreenRecorderDialog::onStartClicked()
{
    if (!m_controller)
    {
        QMessageBox::warning(this, QStringLiteral("屏幕录制"), QStringLiteral("未绑定录制控制器。"));
        return;
    }

    const auto mode = static_cast<recorder::CaptureMode>(ui->regionCombo->currentData().toInt());
    if (mode == recorder::CaptureMode::Region && !m_regionValid)
    {
        if (!beginRegionSelection())
        {
            QMessageBox::warning(this, QStringLiteral("屏幕录制"), QStringLiteral("请先选择录制区域。"));
            return;
        }
    }
    if (mode == recorder::CaptureMode::Window)
    {
        if (!m_windowTarget || !m_windowTarget->isAvailable())
        {
            QMessageBox::warning(this,
                                 QStringLiteral("屏幕录制"),
                                 QStringLiteral("请先打开相机并显示预览。"));
            return;
        }
        m_windowTarget->refreshVisualCache(true);
    }

    updatePresetSummary();

    m_activeRecordPath =
        RecorderPathHelper::uniqueOutputFileInDir(currentOutputDirectory(), currentFormat());

    recorder::RecorderConfig cfg = currentConfig();
    cfg.output.filePath = m_activeRecordPath.toStdString();
    const QString err = RecorderUiBinder::validateForUi(cfg);
    if (!err.isEmpty())
    {
        QMessageBox::warning(this, QStringLiteral("屏幕录制"), err);
        m_activeRecordPath.clear();
        return;
    }

    // 预览走 GDI/视觉缓存，与录制 DXGI 会话独立；开录前释放 idle 预览占用以便重建。
    m_previewTimer->stop();
    invalidatePreviewSessions();

    if (!m_controller->init(cfg))
    {
        QMessageBox::warning(this, QStringLiteral("屏幕录制"), m_controller->lastError());
        refreshStatusText();
        updatePreviewTimerForPage();
        refreshPreviewCapture();
        return;
    }
    if (!m_controller->start())
    {
        m_controller->stop();
        QMessageBox::warning(this, QStringLiteral("屏幕录制"), m_controller->lastError());
        refreshStatusText();
        updatePreviewTimerForPage();
        refreshPreviewCapture();
        return;
    }
    applyStateToUi(m_controller->state());
    refreshStatusText();
    showFloatBallForRecording();
    syncWindowVisualConsumers();
    hide();
}

void ScreenRecorderDialog::onPauseClicked()
{
    if (m_controller)
        m_controller->pause();
}

void ScreenRecorderDialog::onResumeClicked()
{
    if (m_controller)
        m_controller->resume();
}

void ScreenRecorderDialog::onStopClicked()
{
    if (!m_controller)
        return;

    m_durationTimer->stop();

    if (m_controller->stop())
    {
        const int finalDuration =
            resolveSavedVideoDurationSeconds(m_controller->recordedSeconds());
        ui->controlBar->setDurationSeconds(finalDuration);
        handleStopFinished(finalDuration);
    }
    else
        refreshStatusText();
}

void ScreenRecorderDialog::onControllerStateChanged(recorder::RecorderState state)
{
    applyStateToUi(state);
    syncFloatBall();
    syncWindowVisualConsumers();
    refreshStatusText();
}

void ScreenRecorderDialog::onControllerDurationChanged(int seconds)
{
    if (!m_controller)
        return;
    const recorder::RecorderState st = m_controller->state();
    if (st != recorder::RecorderState::Recording && st != recorder::RecorderState::Paused)
        return;
    ui->controlBar->setDurationSeconds(seconds);
    if (m_floatBall)
        m_floatBall->setDurationSeconds(seconds);
}

void ScreenRecorderDialog::onDurationTimer()
{
    if (!m_controller)
        return;
    const recorder::RecorderState st = m_controller->state();
    if (st != recorder::RecorderState::Recording && st != recorder::RecorderState::Paused)
        return;
    ui->controlBar->setDurationSeconds(m_controller->recordedSeconds());
    if (m_floatBall)
        m_floatBall->setDurationSeconds(m_controller->recordedSeconds());
}

void ScreenRecorderDialog::onControllerError(const QString &message)
{
    if (!message.isEmpty())
        QMessageBox::warning(this, QStringLiteral("屏幕录制"), message);
    refreshStatusText();
}

void ScreenRecorderDialog::onRegionComboChanged(int index)
{
    Q_UNUSED(index);
    if (m_blockRegionCombo)
        return;

    const auto mode = static_cast<recorder::CaptureMode>(ui->regionCombo->currentData().toInt());

    if (mode == recorder::CaptureMode::Region)
    {
        if (!beginRegionSelection())
        {
            m_blockRegionCombo = true;
            RecorderUiBinder::setCaptureMode(ui->regionCombo, m_lastCaptureMode);
            m_blockRegionCombo = false;
            return;
        }
    }
    else
    {
        m_regionValid = false;
        invalidatePreviewSessions();
        updatePresetSummary();
    }

    m_lastCaptureMode = mode;
    refreshRegionSummary();
    syncWindowVisualConsumers();
    refreshPreviewCapture();
    applyStateToUi(m_controller ? m_controller->state() : recorder::RecorderState::Idle);
}
