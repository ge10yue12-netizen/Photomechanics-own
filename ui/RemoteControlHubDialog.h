#pragma once

#include <QDialog>

class MobileHost;
class RemoteControlDialog;
class RemoteHost;
class RemoteServiceDialog;

namespace Ui
{
class RemoteControlHubDialog;
}

// 宿主专用 Hub：外壳见 RemoteControlHubDialog.ui；Tab 页在代码中嵌入各模块面板
class RemoteControlHubDialog : public QDialog
{
    Q_OBJECT

public:
    RemoteControlHubDialog(RemoteHost *kitHost, MobileHost *mobileHost, QWidget *parent = nullptr);
    ~RemoteControlHubDialog() override;

    void syncKitEnabled(bool enabled);

signals:
    void kitEnabledChanged(bool enabled);
    void notify(const QString &message);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    RemoteServiceDialog *m_kitPanel = nullptr;
    RemoteControlDialog *m_qrPanel = nullptr;
    Ui::RemoteControlHubDialog *ui = nullptr;
};
