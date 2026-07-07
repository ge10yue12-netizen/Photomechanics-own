#pragma once

#include "../RecorderKit.h"
#include "../RecorderOutputStore.h"
#include "../RecorderNavSidebar.h"

#include <QDialog>

class QTimer;
class RecorderHost;

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

    void bindRecorderHost(RecorderHost *host);

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private slots:
    void onBrowsePathClicked();
    void onReselectRegionClicked();
    void onStartClicked();
    void onPauseClicked();
    void onResumeClicked();
    void onStopClicked();
    void onHostStateChanged(recorder::RecorderState state);
    void onHostDurationChanged(int seconds);
    void onHostError(const QString &message);
    void onRegionComboChanged(int index);
    void onSettingsChanged();
    void onPreviewTimer();
    void onDurationTimer();
    void onNavPageChanged(RecorderNavSidebar::Page page);

private:
    void wireUi();
    void wireHost();
    void ensureDefaultPath();
    void refreshRegionSummary();
    void refreshSettingsSummary();
    void refreshPreviewCapture();
    void updatePathTooltip();
    void invalidatePreviewSessions();
    void applyStateToUi(recorder::RecorderState state);
    recorder::CaptureMode currentPreviewMode() const;
    void refreshStatusText();
    recorder::RecorderConfig currentConfig() const;
    recorder::VideoFormat currentFormat() const;
    void syncPathExtensionToFormat();
    void syncResolutionFromCapture();
    bool beginRegionSelection();
    void handleStopFinished();
    void handleCloseWhileRecording();
    void addOutputToHistory(const QString &path, int durationSeconds);

    Ui::ScreenRecorderDialog *ui = nullptr;
    RecorderHost *m_host = nullptr;
    RecorderOutputStore m_outputStore;
    QTimer *m_previewTimer = nullptr;
    QTimer *m_durationTimer = nullptr;

    recorder::Rect m_region;
    bool m_regionValid = false;
    recorder::CaptureMode m_lastCaptureMode = recorder::CaptureMode::FullScreen;
    bool m_blockRegionCombo = false;
    bool m_pathInitialized = false;
    QString m_activeRecordPath;
    bool m_previewGrabbing = false;

    static constexpr int kPreviewIntervalIdleMs = 100;
    static constexpr int kDurationTickMs = 200;
};
