#include "RemoteControlDialog.h"

#include "MobileHost.h"
#include "NetworkHelper.h"
#include "QrCodeHelper.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCloseEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QVBoxLayout>

RemoteControlDialog::RemoteControlDialog(MobileHost *host, QWidget *parent)
    : QDialog(parent)
    , m_host(host)
{
    setWindowTitle(QStringLiteral("QR 远程控制"));
    setMinimumSize(420, 480);
    setAttribute(Qt::WA_DeleteOnClose, false);
    rebuildUi();

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

void RemoteControlDialog::rebuildUi()
{
    auto *root = new QVBoxLayout(this);

    m_qrLabel = new QLabel(this);
    m_qrLabel->setAlignment(Qt::AlignCenter);
    m_qrLabel->setMinimumSize(280, 280);
    m_qrLabel->setFrameShape(QFrame::Box);
    root->addWidget(m_qrLabel);

    auto *ipRow = new QHBoxLayout();
    ipRow->addWidget(new QLabel(QStringLiteral("本机 IP："), this));
    m_ipCombo = new QComboBox(this);
    ipRow->addWidget(m_ipCombo, 1);
    root->addLayout(ipRow);

    m_urlLabel = new QLabel(QStringLiteral("访问 URL："), this);
    m_urlLabel->setWordWrap(true);
    m_urlLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(m_urlLabel);

    m_statusLabel = new QLabel(QStringLiteral("状态：—"), this);
    m_statusLabel->setWordWrap(true);
    root->addWidget(m_statusLabel);

    auto *btnRow = new QHBoxLayout();
    m_refreshBtn = new QPushButton(QStringLiteral("刷新"), this);
    m_copyBtn = new QPushButton(QStringLiteral("复制链接"), this);
    m_disconnectBtn = new QPushButton(QStringLiteral("断开"), this);
    auto *closeBtn = new QPushButton(QStringLiteral("关闭"), this);
    btnRow->addWidget(m_refreshBtn);
    btnRow->addWidget(m_copyBtn);
    btnRow->addWidget(m_disconnectBtn);
    btnRow->addWidget(closeBtn);
    root->addLayout(btnRow);

    connect(m_refreshBtn, &QPushButton::clicked, this, &RemoteControlDialog::onRefreshClicked);
    connect(m_copyBtn, &QPushButton::clicked, this, &RemoteControlDialog::onCopyUrlClicked);
    connect(m_disconnectBtn, &QPushButton::clicked, this, &RemoteControlDialog::onDisconnectClicked);
    connect(closeBtn, &QPushButton::clicked, this, &RemoteControlDialog::onHideClicked);
    connect(m_ipCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RemoteControlDialog::onIpChanged);
}

void RemoteControlDialog::loadIpList()
{
    const QString keepIp = m_host && m_host->isSessionActive()
        ? m_host->sessionDisplayIp()
        : selectedIp();

    QSignalBlocker blocker(m_ipCombo);
    m_ipCombo->clear();
    const QStringList ips = NetworkHelper::getLocalIPv4List();
    if (ips.isEmpty())
    {
        m_ipCombo->addItem(QStringLiteral("（未检测到局域网地址）"), QString());
        m_refreshBtn->setEnabled(false);
        return;
    }
    m_refreshBtn->setEnabled(true);
    for (const QString &ip : ips)
        m_ipCombo->addItem(ip, ip);

    const int keepIdx = m_ipCombo->findData(keepIp);
    if (keepIdx >= 0)
        m_ipCombo->setCurrentIndex(keepIdx);
    else
    {
        const int preferredIdx = m_ipCombo->findData(NetworkHelper::preferredDefaultIp());
        if (preferredIdx >= 0)
            m_ipCombo->setCurrentIndex(preferredIdx);
    }
}

QString RemoteControlDialog::selectedIp() const
{
    if (!m_ipCombo)
        return QString();
    return m_ipCombo->currentData().toString();
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
        m_statusLabel->setText(QStringLiteral("状态：无可用地址"));
        return false;
    }

    if (!m_host->startSession(ip))
    {
        m_statusLabel->setText(QStringLiteral("状态：启动失败 — %1").arg(m_host->lastError()));
        m_qrLabel->clear();
        m_urlLabel->setText(QStringLiteral("访问 URL：（启动失败）"));
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
        m_statusLabel->setText(QStringLiteral("状态：无可用地址"));
        return;
    }

    m_refreshInProgress = true;
    if (m_host->isSessionActive())
        m_host->stopSession();
    m_refreshInProgress = false;

    if (!m_host->startSession(ip))
    {
        m_statusLabel->setText(QStringLiteral("状态：启动失败 — %1").arg(m_host->lastError()));
        m_qrLabel->clear();
        m_urlLabel->setText(QStringLiteral("访问 URL：（启动失败）"));
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
    m_qrLabel->clear();
    m_urlLabel->setText(QStringLiteral("访问 URL：（已断开）"));
    m_statusLabel->setText(QStringLiteral("状态：已断开"));
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
        m_statusLabel->setText(QStringLiteral("状态：IP 已变更，须重新生成会话"));
}

void RemoteControlDialog::onSessionStarted(const QString &url)
{
    m_urlLabel->setText(QStringLiteral("访问 URL：%1").arg(url));
    updateQrAndUrl();
    updateStatusText();
}

void RemoteControlDialog::updateQrAndUrl()
{
    if (!m_host)
        return;

    const QString url = m_host->mobileUrl();
    m_urlLabel->setText(QStringLiteral("访问 URL：%1").arg(url.isEmpty() ? QStringLiteral("（无）") : url));

    const QImage qr = QrCodeHelper::generateQrImage(url);
    if (qr.isNull())
    {
        m_qrLabel->setText(QStringLiteral("二维码生成失败"));
        return;
    }
    m_qrLabel->setPixmap(QPixmap::fromImage(qr));
}

void RemoteControlDialog::updateStatusText()
{
    if (!m_host || !m_host->isSessionActive())
    {
        m_statusLabel->setText(QStringLiteral("状态：未启动"));
        return;
    }
    if (selectedIp() != m_host->sessionDisplayIp())
    {
        m_statusLabel->setText(QStringLiteral("状态：IP 已变更，须重新生成会话"));
        return;
    }
    if (m_host->isPhoneConnected())
        m_statusLabel->setText(QStringLiteral("状态：客户端已连接"));
    else
        m_statusLabel->setText(QStringLiteral("状态：等待客户端连接"));
}
