#include "ScreenRecorderDialog.h"
#include "ui_ScreenRecorderDialog.h"
#include "ScreenRecorderSettingsDialog.h"

#include "../RecorderControlBar.h"
#include "../RecorderHost.h"
#include "../RecorderOutputListWidget.h"
#include "../RecorderPathHelper.h"
#include "../RecorderPreviewCapture.h"
#include "../RecorderStatusText.h"
#include "../RecorderUiBinder.h"
#include "../RegionSelectorWidget.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>
#include <QShowEvent>
#include <QSize>
#include <QSpinBox>
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

    wireUi();
}

ScreenRecorderDialog::~ScreenRecorderDialog()
{
    delete ui;
}

void ScreenRecorderDialog::bindRecorderHost(RecorderHost *host)
{
    m_host = host;
    wireHost();
    if (m_host)
        applyStateToUi(m_host->state());
}

void ScreenRecorderDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    m_outputStore.load(nullptr);
    ui->outputList->setStore(&m_outputStore);
    ensureDefaultPath();
    syncResolutionFromCapture();
    refreshSettingsSummary();
    refreshStatusText();
    refreshPreviewCapture();
    m_previewTimer->start();
}

void ScreenRecorderDialog::hideEvent(QHideEvent *event)
{
    QDialog::hideEvent(event);
    m_previewTimer->stop();
    invalidatePreviewSessions();
}

void ScreenRecorderDialog::closeEvent(QCloseEvent *event)
{
    handleCloseWhileRecording();
    event->ignore();
    hide();
}

void ScreenRecorderDialog::handleCloseWhileRecording()
{
    if (!m_host)
        return;

    const recorder::RecorderState st = m_host->state();
    if (st != recorder::RecorderState::Recording && st != recorder::RecorderState::Paused)
        return;

    if (m_host->stop())
        handleStopFinished();
    else
        refreshStatusText();
}

void ScreenRecorderDialog::wireUi()
{
    // 布局拉伸比例在代码中设置（uic 对 stretch 属性支持不佳）。
    ui->generalVBox->setStretch(0, 1);
    ui->generalVBox->setStretch(1, 0);
    ui->contentHBox->setStretch(1, 1);
    ui->previewCol->setStretch(1, 1);
    ui->controlBarRow->setStretch(0, 1);
    ui->controlBarRow->setStretch(1, 0);
    ui->controlBarRow->setStretch(2, 1);

    RecorderUiBinder::fillCaptureModeCombo(ui->regionCombo);

    connect(ui->navSidebar, &RecorderNavSidebar::pageChanged, this, &ScreenRecorderDialog::onNavPageChanged);
    connect(ui->regionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScreenRecorderDialog::onRegionComboChanged);
    connect(ui->reselectRegionBtn, &QPushButton::clicked, this, &ScreenRecorderDialog::onReselectRegionClicked);
    connect(ui->browseBtn, &QPushButton::clicked, this, &ScreenRecorderDialog::onBrowsePathClicked);
    connect(ui->controlBar, &RecorderControlBar::startClicked, this, &ScreenRecorderDialog::onStartClicked);
    connect(ui->controlBar, &RecorderControlBar::pauseClicked, this, &ScreenRecorderDialog::onPauseClicked);
    connect(ui->controlBar, &RecorderControlBar::resumeClicked, this, &ScreenRecorderDialog::onResumeClicked);
    connect(ui->controlBar, &RecorderControlBar::stopClicked, this, &ScreenRecorderDialog::onStopClicked);
    connect(ui->pathEdit, &QLineEdit::textChanged, this, &ScreenRecorderDialog::updatePathTooltip);

    connect(ui->settingsPanel, &ScreenRecorderSettingsDialog::settingsChanged,
            this, &ScreenRecorderDialog::onSettingsChanged);

    m_lastCaptureMode = recorder::CaptureMode::FullScreen;
    onRegionComboChanged(ui->regionCombo->currentIndex());
    applyStateToUi(recorder::RecorderState::Idle);
}

void ScreenRecorderDialog::wireHost()
{
    if (!m_host)
        return;
    connect(m_host, &RecorderHost::stateChanged, this, &ScreenRecorderDialog::onHostStateChanged);
    connect(m_host, &RecorderHost::durationChanged, this, &ScreenRecorderDialog::onHostDurationChanged);
    connect(m_host, &RecorderHost::errorOccurred, this, &ScreenRecorderDialog::onHostError);
}

void ScreenRecorderDialog::onNavPageChanged(RecorderNavSidebar::Page page)
{
    ui->stack->setCurrentIndex(static_cast<int>(page));
    if (page == RecorderNavSidebar::List)
        ui->outputList->reloadList();
}

void ScreenRecorderDialog::ensureDefaultPath()
{
    QString err;
    RecorderPathHelper::ensureRecordRootExists(&err);
    if (ui->pathEdit->text().trimmed().isEmpty() || !m_pathInitialized)
    {
        ui->pathEdit->setText(RecorderPathHelper::defaultOutputFile(currentFormat()));
        m_pathInitialized = true;
    }
    ui->controlBar->setDurationSeconds(0);
    updatePathTooltip();
}

void ScreenRecorderDialog::updatePathTooltip()
{
    ui->pathEdit->setToolTip(ui->pathEdit->text().trimmed());
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
    ui->reselectRegionBtn->setVisible(regionMode);

    if (!regionMode)
    {
        ui->regionSummaryLabel->hide();
        return;
    }
    ui->regionSummaryLabel->show();
    if (!m_regionValid)
    {
        ui->regionSummaryLabel->setText(QStringLiteral("区域录制：尚未选择区域"));
        return;
    }
    ui->regionSummaryLabel->setText(
        QStringLiteral("区域 %1×%2，起点 (%3, %4)")
            .arg(m_region.width)
            .arg(m_region.height)
            .arg(m_region.x)
            .arg(m_region.y));
}

void ScreenRecorderDialog::refreshSettingsSummary()
{
    if (!ui->settingsPanel)
        return;
    const int fps = ui->settingsPanel->fpsSpin() ? ui->settingsPanel->fpsSpin()->value() : 25;
    const int width = ui->settingsPanel->widthSpin() ? ui->settingsPanel->widthSpin()->value() : 0;
    const int height = ui->settingsPanel->heightSpin() ? ui->settingsPanel->heightSpin()->value() : 0;
    const int bitrate = ui->settingsPanel->bitrateSpin() ? ui->settingsPanel->bitrateSpin()->value() : 10000;
    const auto fmt = currentFormat();
    const QString fmtText = QString::fromUtf8(recorder::videoFormatLabel(fmt));
    ui->settingsSummaryLabel->setText(
        QStringLiteral("%1 FPS · %2×%3 · %4 · %5 kbps").arg(fps).arg(width).arg(height).arg(fmtText).arg(bitrate));
}

void ScreenRecorderDialog::syncResolutionFromCapture()
{
    if (!ui->settingsPanel)
        return;

    QSpinBox *widthSpin = ui->settingsPanel->widthSpin();
    QSpinBox *heightSpin = ui->settingsPanel->heightSpin();
    if (!widthSpin || !heightSpin)
        return;

    const auto mode = static_cast<recorder::CaptureMode>(ui->regionCombo->currentData().toInt());
    const QSize size = RecorderUiBinder::captureSourceSize(mode, m_region, m_regionValid);
    widthSpin->setValue(size.width());
    heightSpin->setValue(size.height());
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
    recorder::Rect unused{};

    const bool ok = mode == recorder::CaptureMode::Region
                        ? RecorderPreviewCapture::grabFrame(mode, m_region, m_regionValid, &previewImage, &err)
                        : RecorderPreviewCapture::grabFrame(recorder::CaptureMode::FullScreen,
                                                            unused,
                                                            false,
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
    ui->pathEdit->setEnabled(idleLike);
    ui->navSidebar->setSettingsEnabled(idleLike);
    if (ui->settingsPanel)
        ui->settingsPanel->setControlsEnabled(idleLike);
    ui->outputList->setControlsEnabled(idleLike);
    ui->controlBar->setRecorderState(state);

    const bool recordingLike = recording || paused;
    if (recordingLike)
    {
        m_previewTimer->stop();
        if (!m_durationTimer->isActive())
            m_durationTimer->start();
    }
    else
    {
        m_durationTimer->stop();
        if (isVisible() && !m_previewTimer->isActive())
            m_previewTimer->start();
    }
}

void ScreenRecorderDialog::refreshStatusText()
{
    const recorder::RecorderState st = m_host ? m_host->state() : recorder::RecorderState::Idle;
    const QString err = m_host ? m_host->lastError() : QString();
    ui->statusLabel->setText(RecorderStatusText::sessionSummary(recorder::stateLabel(st), err));
}

recorder::RecorderConfig ScreenRecorderDialog::currentConfig() const
{
    return RecorderUiBinder::configFromWidgets(
        ui->regionCombo,
        ui->settingsPanel ? ui->settingsPanel->fpsSpin() : nullptr,
        ui->settingsPanel ? ui->settingsPanel->widthSpin() : nullptr,
        ui->settingsPanel ? ui->settingsPanel->heightSpin() : nullptr,
        ui->settingsPanel ? ui->settingsPanel->bitrateSpin() : nullptr,
        ui->settingsPanel ? ui->settingsPanel->formatCombo() : nullptr,
        ui->pathEdit,
        m_region);
}

void ScreenRecorderDialog::syncPathExtensionToFormat()
{
    QString path = ui->pathEdit->text().trimmed();
    if (path.isEmpty())
        return;

    const QString ext = RecorderPathHelper::extensionFor(currentFormat());
    const int dot = path.lastIndexOf('.');
    if (dot > 0)
        path = path.left(dot) + ext;
    else
        path += ext;
    ui->pathEdit->setText(path);
    updatePathTooltip();
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
    syncResolutionFromCapture();
    refreshRegionSummary();
    refreshSettingsSummary();
    refreshPreviewCapture();
    return true;
}

void ScreenRecorderDialog::addOutputToHistory(const QString &path, int durationSeconds)
{
    if (path.isEmpty())
        return;

    RecorderOutputEntry entry;
    entry.filePath = path;
    entry.durationSeconds = durationSeconds;
    entry.savedAt = QDateTime::currentDateTime().toString(Qt::ISODate);
    if (QFileInfo::exists(path))
        entry.sizeBytes = QFileInfo(path).size();

    m_outputStore.addEntry(entry);
    m_outputStore.save(nullptr);
}

void ScreenRecorderDialog::handleStopFinished()
{
    const QString path = m_activeRecordPath.isEmpty() ? ui->pathEdit->text().trimmed() : m_activeRecordPath;
    const int duration = m_host ? m_host->recordedSeconds() : 0;
    addOutputToHistory(path, duration);

    const auto ret = QMessageBox::question(this,
                                           QStringLiteral("录制完成"),
                                           QStringLiteral("文件已保存至：\n%1\n\n时长 %2\n\n打开文件还是打开所在文件夹？")
                                               .arg(path)
                                               .arg(RecorderStatusText::formatDuration(duration)),
                                           QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                                           QMessageBox::Yes);
    if (ret == QMessageBox::Yes)
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    else if (ret == QMessageBox::No)
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));

    m_activeRecordPath.clear();
    ui->controlBar->setDurationSeconds(0);
    applyStateToUi(m_host ? m_host->state() : recorder::RecorderState::Stopped);
    refreshStatusText();
    refreshPreviewCapture();
}

void ScreenRecorderDialog::onBrowsePathClicked()
{
    const QString path = QFileDialog::getSaveFileName(this,
                                                      QStringLiteral("选择保存路径"),
                                                      ui->pathEdit->text(),
                                                      QStringLiteral("MP4 (*.mp4);;AVI (*.avi)"));
    if (!path.isEmpty())
    {
        ui->pathEdit->setText(path);
        updatePathTooltip();
    }
}

void ScreenRecorderDialog::onSettingsChanged()
{
    syncPathExtensionToFormat();
    refreshSettingsSummary();
    updatePathTooltip();
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
    if (m_previewGrabbing)
        return;
    m_previewGrabbing = true;
    refreshPreviewCapture();
    m_previewGrabbing = false;
}

void ScreenRecorderDialog::onStartClicked()
{
    if (!m_host)
        return;

    const auto mode = static_cast<recorder::CaptureMode>(ui->regionCombo->currentData().toInt());
    if (mode == recorder::CaptureMode::Region && !m_regionValid)
    {
        if (!beginRegionSelection())
        {
            QMessageBox::warning(this, QStringLiteral("屏幕录制"), QStringLiteral("请先选择录制区域。"));
            return;
        }
    }

    syncResolutionFromCapture();
    refreshSettingsSummary();

    const recorder::RecorderConfig cfg = currentConfig();
    const QString err = RecorderUiBinder::validateForUi(cfg);
    if (!err.isEmpty())
    {
        QMessageBox::warning(this, QStringLiteral("屏幕录制"), err);
        return;
    }

    m_activeRecordPath = ui->pathEdit->text().trimmed();

    // DXGI 每输出仅允许一个 Duplication 会话；开录前释放预览占用。
    m_previewTimer->stop();
    invalidatePreviewSessions();

    if (!m_host->init(cfg))
    {
        QMessageBox::warning(this, QStringLiteral("屏幕录制"), m_host->lastError());
        refreshStatusText();
        return;
    }
    if (!m_host->start())
    {
        m_host->stop();
        QMessageBox::warning(this, QStringLiteral("屏幕录制"), m_host->lastError());
        refreshStatusText();
        return;
    }
    applyStateToUi(m_host->state());
    refreshStatusText();
}

void ScreenRecorderDialog::onPauseClicked()
{
    if (m_host)
        m_host->pause();
}

void ScreenRecorderDialog::onResumeClicked()
{
    if (m_host)
        m_host->resume();
}

void ScreenRecorderDialog::onStopClicked()
{
    if (!m_host)
        return;

    const auto ret = QMessageBox::question(this,
                                           QStringLiteral("停止录制"),
                                           QStringLiteral("确定停止并保存当前录制？"),
                                           QMessageBox::Yes | QMessageBox::No,
                                           QMessageBox::Yes);
    if (ret != QMessageBox::Yes)
        return;

    if (m_host->stop())
        handleStopFinished();
    else
        refreshStatusText();
}

void ScreenRecorderDialog::onHostStateChanged(recorder::RecorderState state)
{
    applyStateToUi(state);
    refreshStatusText();
}

void ScreenRecorderDialog::onHostDurationChanged(int seconds)
{
    ui->controlBar->setDurationSeconds(seconds);
}

void ScreenRecorderDialog::onDurationTimer()
{
    if (!m_host)
        return;
    const recorder::RecorderState st = m_host->state();
    if (st != recorder::RecorderState::Recording && st != recorder::RecorderState::Paused)
        return;
    ui->controlBar->setDurationSeconds(m_host->recordedSeconds());
}

void ScreenRecorderDialog::onHostError(const QString &message)
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
        syncResolutionFromCapture();
    }

    m_lastCaptureMode = mode;
    refreshRegionSummary();
    refreshSettingsSummary();
    refreshPreviewCapture();
    applyStateToUi(m_host ? m_host->state() : recorder::RecorderState::Idle);
}
