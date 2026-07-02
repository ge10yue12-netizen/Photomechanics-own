#pragma once

#include <QDialog>

class QComboBox;
class QLabel;
class MobileHost;
class QPushButton;

// @brief 扫码遥控对话框（可选 UI；会话由 MobileHost 维护）。
class RemoteControlDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RemoteControlDialog(MobileHost *host, QWidget *parent = nullptr);

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
    void rebuildUi();
    void loadIpList();
    bool ensureSessionStarted();
    void updateQrAndUrl();
    void updateStatusText();
    QString selectedIp() const;

    MobileHost *m_host = nullptr;
    bool m_refreshInProgress = false;
    QLabel *m_qrLabel = nullptr;
    QLabel *m_urlLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QComboBox *m_ipCombo = nullptr;
    QPushButton *m_refreshBtn = nullptr;
    QPushButton *m_copyBtn = nullptr;
    QPushButton *m_disconnectBtn = nullptr;
};
