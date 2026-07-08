#pragma once

#include "../RecorderKit.h"
#include "../RecorderOutputStore.h"
#include "../RecorderNavSidebar.h"

#include <QDialog>
#include <QFileSystemWatcher>
#include <QString>
#include <QTimer>
class RecorderController;

class RecorderFloatBall;

namespace recorder {
class IRecorderWindowTarget;
}

namespace Ui {
class ScreenRecorderDialog;
}

// 屏幕录制主面板（ScreenRecorderDialog.ui 可在 Qt Designer 编辑）。
class ScreenRecorderDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ScreenRecorderDialog(QWidget *parent = nullptr);
    ~ScreenRecorderDialog() override;

    // controller 须由调用方持有，生命周期不短于本对话框。
    void bindController(RecorderController *controller);

    // 注册固定窗口录制目标（如相机预览）；label 为下拉第三项文案。
    void setFixedWindowTarget(recorder::IRecorderWindowTarget *target, const QString &label);

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void changeEvent(QEvent *event) override;

private slots:
    void onBrowsePathClicked();
    void onReselectRegionClicked();
    void onStartClicked();
    void onPauseClicked();
    void onResumeClicked();
    void onStopClicked();
    void onControllerStateChanged(recorder::RecorderState state);
    void onControllerDurationChanged(int seconds);
    void onControllerError(const QString &message);
    void onRegionComboChanged(int index);
    void onSettingsChanged();
    void onPathSettingsChanged();
    void onPreviewTimer();
    void onDurationTimer();
    void onNavPageChanged(RecorderNavSidebar::Page page);

private:
    void wireUi();
    void wireFloatBall();
    void wireController();
    void refreshCaptureModeCombo();
    void syncWindowVisualConsumers();
    std::uintptr_t currentWindowHandle() const;
    int resolveSavedVideoDurationSeconds(int fallbackSeconds) const;
    void ensureDefaultPath();
    void refreshRegionSummary();
    void refreshPreviewCapture();
    void updatePathTooltip();
    void invalidatePreviewSessions();
    void applyStateToUi(recorder::RecorderState state);
    recorder::CaptureMode currentPreviewMode() const;
    void refreshStatusText();
    recorder::RecorderConfig currentConfig() const;
    recorder::VideoFormat currentFormat() const;
    QString currentOutputDirectory() const;
    void updatePresetSummary();
    bool beginRegionSelection();
    void handleStopFinished(int finalDurationSeconds);
    void showSavedToast();
    bool isGeneralPage() const;
    bool isListPage() const;
    void updateOutputDirWatcher();
    void resyncOutputListFromDisk();
    void updatePreviewTimerForPage();
    bool isRecordingActive() const;
    void syncFloatBall();
    void showFloatBallForRecording();
    void hideFloatBall();

    Ui::ScreenRecorderDialog *ui = nullptr;
    RecorderController *m_controller = nullptr;
    RecorderFloatBall *m_floatBall = nullptr;
    RecorderOutputStore m_outputStore;
    QTimer *m_previewTimer = nullptr;
    QTimer *m_durationTimer = nullptr;
    QFileSystemWatcher m_outputDirWatcher;
    QTimer m_outputDirResyncTimer;

    recorder::Rect m_region;
    bool m_regionValid = false;
    recorder::CaptureMode m_lastCaptureMode = recorder::CaptureMode::FullScreen;
    bool m_blockRegionCombo = false;
    recorder::IRecorderWindowTarget *m_windowTarget = nullptr;
    QString m_windowTargetLabel;
    bool m_pathInitialized = false;
    QString m_activeRecordPath;
    bool m_previewGrabbing = false;

    static constexpr int kPreviewIntervalIdleMs = 300;
    static constexpr int kDurationTickMs = 200;
};
