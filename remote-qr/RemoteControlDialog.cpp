#include "RemoteControlDialog.h"

#include "MobileHost.h"
#include "NetworkHelper.h"
#include "QrCodeHelper.h"
#include "ui_RemoteControlDialog.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QPixmap>
#include <QShowEvent>
#include <QSignalBlocker>

RemoteControlDialog::RemoteControlDialog(MobileHost *host, QWidget *parent)
    : QDialog(parent)
    , m_host(host)
    , ui(new Ui::RemoteControlDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose, false);

    connect(ui->refreshBtn, &QPushButton::clicked, this, &RemoteControlDialog::onRefreshClicked);
    connect(ui->copyBtn, &QPushButton::clicked, this, &RemoteControlDialog::onCopyUrlClicked);
    connect(ui->disconnectBtn, &QPushButton::clicked, this, &RemoteControlDialog::onDisconnectClicked);
    connect(ui->closeBtn, &QPushButton::clicked, this, &RemoteControlDialog::onHideClicked);
    connect(ui->ipCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RemoteControlDialog::onIpChanged);

    if (m_host)
    {
        connect(m_host, &MobileHost::sessionStarted, this, &RemoteControlDialog::onSessionStarted);
        connect(m_host, &MobileHost::sessionStopped, this, [this]() {
            if (!m_refreshInProgress)
                updateStatusText();
        });
        connect(m_host, &MobileHost::phoneConnected, this, &RemoteControlDialog::updateStatusText);
        connect(m_host, &MobileHost::phoneDisconnected, this, &RemoteControlDialog::updateStatusText);
    }
}

RemoteControlDialog::~RemoteControlDialog()
{
    delete ui;
}

void RemoteControlDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    loadIpList();
    ensureSessionStarted();
}

void RemoteControlDialog::closeEvent(QCloseEvent *event)
{
    event->ignore();
    hide();
}

void RemoteControlDialog::loadIpList()
{
    const QString keepIp = m_host && m_host->isSessionActive()
        ? m_host->sessionDisplayIp()
        : selectedIp();

    QSignalBlocker blocker(ui->ipCombo);
    ui->ipCombo->clear();
    const QStringList ips = NetworkHelper::getLocalIPv4List();
    if (ips.isEmpty())
    {
        ui->ipCombo->addItem(QStringLiteral("（未检测到局域网地址）"), QString());
        ui->refreshBtn->setEnabled(false);
        return;
    }
    ui->refreshBtn->setEnabled(true);
    for (const QString &ip : ips)
        ui->ipCombo->addItem(ip, ip);

    const int keepIdx = ui->ipCombo->findData(keepIp);
    if (keepIdx >= 0)
        ui->ipCombo->setCurrentIndex(keepIdx);
    else
    {
        const int preferredIdx = ui->ipCombo->findData(NetworkHelper::preferredDefaultIp());
        if (preferredIdx >= 0)
            ui->ipCombo->setCurrentIndex(preferredIdx);
    }
}

QString RemoteControlDialog::selectedIp() const
{
    return ui->ipCombo->currentData().toString();
}

bool RemoteControlDialog::ensureSessionStarted()
{
    if (!m_host)
        return false;

    m_host->syncExpiredSession();

    if (m_host->isSessionActive())
    {
        updateQrAndUrl();
        updateStatusText();
        return true;
    }

    const QString ip = selectedIp();
    if (ip.isEmpty())
    {
        ui->statusLabel->setText(QStringLiteral("状态：无可用地址"));
        return false;
    }

    if (!m_host->startSession(ip))
    {
        ui->statusLabel->setText(QStringLiteral("状态：启动失败 — %1").arg(m_host->lastError()));
        ui->qrLabel->clear();
        ui->urlLabel->setText(QStringLiteral("访问 URL：（启动失败）"));
        return false;
    }

    updateQrAndUrl();
    updateStatusText();
    return true;
}

void RemoteControlDialog::onRefreshClicked()
{
    if (!m_host)
        return;

    const QString ip = selectedIp();
    if (ip.isEmpty())
    {
        ui->statusLabel->setText(QStringLiteral("状态：无可用地址"));
        return;
    }

    m_refreshInProgress = true;
    if (m_host->isSessionActive())
        m_host->stopSession();
    m_refreshInProgress = false;

    if (!m_host->startSession(ip))
    {
        ui->statusLabel->setText(QStringLiteral("状态：启动失败 — %1").arg(m_host->lastError()));
        ui->qrLabel->clear();
        ui->urlLabel->setText(QStringLiteral("访问 URL：（启动失败）"));
        return;
    }

    updateQrAndUrl();
    updateStatusText();
}

void RemoteControlDialog::onCopyUrlClicked()
{
    const QString url = m_host ? m_host->mobileUrl() : QString();
    if (url.isEmpty())
        return;
    QApplication::clipboard()->setText(url);
}

void RemoteControlDialog::onDisconnectClicked()
{
    if (m_host && m_host->isSessionActive())
        m_host->stopSession();
    ui->qrLabel->clear();
    ui->urlLabel->setText(QStringLiteral("访问 URL：（已断开）"));
    ui->statusLabel->setText(QStringLiteral("状态：已断开"));
}

void RemoteControlDialog::onHideClicked()
{
    hide();
}

void RemoteControlDialog::onIpChanged(int /*index*/)
{
    if (!m_host || !m_host->isSessionActive())
        return;
    if (selectedIp() != m_host->sessionDisplayIp())
        ui->statusLabel->setText(QStringLiteral("状态：IP 已变更，须重新生成会话"));
}

void RemoteControlDialog::onSessionStarted(const QString &url)
{
    ui->urlLabel->setText(QStringLiteral("访问 URL：%1").arg(url));
    updateQrAndUrl();
    updateStatusText();
}

void RemoteControlDialog::updateQrAndUrl()
{
    if (!m_host)
        return;

    const QString url = m_host->mobileUrl();
    ui->urlLabel->setText(QStringLiteral("访问 URL：%1").arg(url.isEmpty() ? QStringLiteral("（无）") : url));

    const QImage qr = QrCodeHelper::generateQrImage(url);
    if (qr.isNull())
    {
        ui->qrLabel->setText(QStringLiteral("二维码生成失败"));
        return;
    }
    ui->qrLabel->setPixmap(QPixmap::fromImage(qr));
}

void RemoteControlDialog::updateStatusText()
{
    if (!m_host || !m_host->isSessionActive())
    {
        ui->statusLabel->setText(QStringLiteral("状态：未启动"));
        return;
    }
    if (selectedIp() != m_host->sessionDisplayIp())
    {
        ui->statusLabel->setText(QStringLiteral("状态：IP 已变更，须重新生成会话"));
        return;
    }
    if (m_host->isPhoneConnected())
        ui->statusLabel->setText(QStringLiteral("状态：客户端已连接"));
    else
        ui->statusLabel->setText(QStringLiteral("状态：等待客户端连接"));
}
