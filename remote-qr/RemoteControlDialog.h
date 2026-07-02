#pragma once

#include <QDialog>

class MobileHost;

namespace Ui
{
class RemoteControlDialog;
}

// 扫码遥控对话框：布局见 RemoteControlDialog.ui，可在 Qt Designer 中编辑
class RemoteControlDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RemoteControlDialog(MobileHost *host, QWidget *parent = nullptr);
    ~RemoteControlDialog() override;

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onRefreshClicked();
    void onCopyUrlClicked();
    void onDisconnectClicked();
    void onHideClicked();
    void onIpChanged(int index);
    void onSessionStarted(const QString &url);

private:
    void loadIpList();
    bool ensureSessionStarted();
    void updateQrAndUrl();
    void updateStatusText();
    QString selectedIp() const;

    MobileHost *m_host = nullptr;
    bool m_refreshInProgress = false;
    Ui::RemoteControlDialog *ui = nullptr;
};
