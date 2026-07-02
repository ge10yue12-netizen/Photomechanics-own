#pragma once

#include <QDialog>

class RemoteHost;

namespace Ui
{
class RemoteServiceDialog;
}

// 小程序 WiFi/BLE 遥控面板：布局见 RemoteServiceDialog.ui，可在 Qt Designer 中编辑
class RemoteServiceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RemoteServiceDialog(RemoteHost *host, QWidget *parent = nullptr);
    ~RemoteServiceDialog() override;

    void syncKitEnabled(bool enabled);

signals:
    void kitEnabledChanged(bool enabled);
    void notify(const QString &message);

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void onEnableToggled(bool checked);

private:
    void refreshInfoLabels();

    RemoteHost *m_host = nullptr;
    Ui::RemoteServiceDialog *ui = nullptr;
};
