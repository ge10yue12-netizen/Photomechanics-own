#include "RemoteControlHubDialog.h"

#include "remote-qr/RemoteControlDialog.h"
#include "remote/RemoteServiceDialog.h"
#include "ui_RemoteControlHubDialog.h"

#include <QCloseEvent>

RemoteControlHubDialog::RemoteControlHubDialog(RemoteHost *kitHost,
                                               MobileHost *mobileHost,
                                               QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::RemoteControlHubDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose, false);

    m_kitPanel = new RemoteServiceDialog(kitHost, this);
    m_kitPanel->setWindowFlags(Qt::Widget);
    ui->tabWidget->addTab(m_kitPanel, QStringLiteral("小程序 WiFi / BLE"));

    m_qrPanel = new RemoteControlDialog(mobileHost, this);
    m_qrPanel->setWindowFlags(Qt::Widget);
    ui->tabWidget->addTab(m_qrPanel, QStringLiteral("扫码浏览器"));

    connect(m_kitPanel, &RemoteServiceDialog::kitEnabledChanged, this, &RemoteControlHubDialog::kitEnabledChanged);
    connect(m_kitPanel, &RemoteServiceDialog::notify, this, &RemoteControlHubDialog::notify);
}

RemoteControlHubDialog::~RemoteControlHubDialog()
{
    delete ui;
}

void RemoteControlHubDialog::syncKitEnabled(bool enabled)
{
    if (m_kitPanel)
        m_kitPanel->syncKitEnabled(enabled);
}

void RemoteControlHubDialog::closeEvent(QCloseEvent *event)
{
    event->ignore();
    hide();
}
