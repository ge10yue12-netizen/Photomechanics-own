#include "RemoteServiceDialog.h"

#include "RemoteHost.h"
#include "RemoteStatusText.h"
#include "ui_RemoteServiceDialog.h"

#include <QShowEvent>
#include <QSignalBlocker>

RemoteServiceDialog::RemoteServiceDialog(RemoteHost *host, QWidget *parent)
    : QDialog(parent)
    , m_host(host)
    , ui(new Ui::RemoteServiceDialog)
{
    ui->setupUi(this);

    connect(ui->enableCheck, &QCheckBox::toggled, this, &RemoteServiceDialog::onEnableToggled);

    if (m_host)
        connect(m_host, &RemoteHost::statusDisplayChanged, this, &RemoteServiceDialog::refreshInfoLabels);
}

RemoteServiceDialog::~RemoteServiceDialog()
{
    delete ui;
}

void RemoteServiceDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    refreshInfoLabels();
}

void RemoteServiceDialog::syncKitEnabled(bool enabled)
{
    {
        QSignalBlocker block(ui->enableCheck);
        ui->enableCheck->setChecked(enabled);
    }
    refreshInfoLabels();
}

void RemoteServiceDialog::onEnableToggled(bool checked)
{
    emit kitEnabledChanged(checked);
}

void RemoteServiceDialog::refreshInfoLabels()
{
    if (!m_host)
        return;

    ui->bleValueLabel->setText(
        RemoteStatusText::bleSummary(m_host->kit().ble().isRunning(), m_host->kit().ble().lastError()));
    ui->httpValueLabel->setText(RemoteStatusText::httpSummary(m_host->kit().http().isListening(),
                                                              m_host->kit().http().lastError()));

    const bool running = ui->enableCheck->isChecked();
    if (!running || !m_host->kit().http().isListening())
    {
        ui->endpointValueLabel->setText(QStringLiteral("—"));
        ui->tokenValueLabel->setText(QStringLiteral("—"));
        return;
    }

    ui->endpointValueLabel->setText(m_host->kit().httpEndpoint());
    ui->tokenValueLabel->setText(m_host->kit().config().token.isEmpty()
                                     ? QStringLiteral("—")
                                     : m_host->kit().config().token);
}
