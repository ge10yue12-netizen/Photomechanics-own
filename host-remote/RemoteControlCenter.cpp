#include "RemoteControlCenter.h"
#include "ui_RemoteControlCenter.h"

#include "remote/RemoteHost.h"
#include "remote/RemoteStatusText.h"
#include "remote-qr/MobileHost.h"
#include "remote-qr/NetworkHelper.h"
#include "remote-qr/QrCodeHelper.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QRadioButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QTabWidget>

// 构造：初始化 UI、连接信号并刷新各页。
RemoteControlCenter::RemoteControlCenter(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::RemoteControlCenter)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose, false);
    wireUi();
    applyTabVisibility();
    refreshAllPages();
}

// 析构：释放 ui 资源。
RemoteControlCenter::~RemoteControlCenter()
{
    delete ui;
}

// 绑定 WiFi/BLE 门面并刷新 WiFi 页。
void RemoteControlCenter::bindRemoteHost(RemoteHost *host)
{
    m_remoteHost = host;
    applyTabVisibility();
    refreshWifiPage();
}

// 绑定扫码门面并连接会话/连接状态信号。
void RemoteControlCenter::bindMobileHost(MobileHost *host)
{
    if (m_mobileHost)
        disconnect(m_mobileHost, nullptr, this, nullptr);

    m_mobileHost = host;
    if (m_mobileHost)
    {
        connect(m_mobileHost, &MobileHost::sessionStarted, this, &RemoteControlCenter::onQrSessionStarted);
        connect(m_mobileHost, &MobileHost::sessionStopped, this, [this]() {
            if (!m_qrRefreshInProgress)
                updateQrStatusText();
        });
        connect(m_mobileHost, &MobileHost::phoneConnected, this, [this]() { updateQrStatusText(); });
        connect(m_mobileHost, &MobileHost::phoneDisconnected, this, [this]() { updateQrStatusText(); });
    }
    applyTabVisibility();
    updateQrDisplay();
    updateQrStatusText();
}

// 返回远程总闸状态。
bool RemoteControlCenter::isRemoteEnabled() const
{
    return m_remoteEnabled;
}

// 同步总闸 UI 并刷新各页；开启时尝试启动扫码会话。
void RemoteControlCenter::setRemoteEnabled(bool enabled)
{
    m_remoteEnabled = enabled;
    m_syncingRemoteUi = true;
    ui->remoteOffRadio->setChecked(!enabled);
    ui->remoteOnRadio->setChecked(enabled);
    m_syncingRemoteUi = false;
    ui->serviceSummaryLabel->setText(enabled ? QStringLiteral("运行中") : QStringLiteral("未启用"));
    refreshAllPages();
    if (enabled)
        tryEnsureQrSessionOnQrTab();
}

// 对话框显示时刷新 IP 列表与各页状态。
void RemoteControlCenter::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    loadQrIpList();
    refreshAllPages();
    if (m_remoteEnabled && m_mobileHost && ui->modeTabWidget->currentWidget() == ui->qrTab)
        tryEnsureQrSessionOnQrTab();
}

// 关闭事件：忽略关闭并隐藏窗口。
void RemoteControlCenter::closeEvent(QCloseEvent *event)
{
    event->ignore();
    hide();
}

// 用户切换远程总闸单选时发出 remoteEnabledChanged。
void RemoteControlCenter::onRemoteToggleChanged()
{
    if (m_syncingRemoteUi)
        return;
    const bool enabled = ui->remoteOnRadio->isChecked();
    if (enabled == m_remoteEnabled)
        return;
    emit remoteEnabledChanged(enabled);
}

// 复制 WiFi HTTP endpoint 至系统剪贴板。
void RemoteControlCenter::onWifiCopyClicked()
{
    if (!m_remoteHost || !m_remoteHost->kit().http().isListening())
        return;
    const QString endpoint = m_remoteHost->kit().httpEndpoint();
    if (endpoint.isEmpty())
        return;
    QApplication::clipboard()->setText(endpoint);
}

// 停止旧会话并在选中 IP 上启动新扫码 HTTP 会话。
void RemoteControlCenter::onQrRefreshClicked()
{
    if (!m_mobileHost || !m_remoteEnabled)
        return;

    const QString ip = selectedQrIp();
    if (ip.isEmpty())
    {
        ui->qrStatusLabel->setText(QStringLiteral("扫码状态：无可用地址"));
        return;
    }

    m_qrRefreshInProgress = true;
    if (m_mobileHost->isSessionActive())
        m_mobileHost->stopSession();
    m_qrRefreshInProgress = false;

    if (!m_mobileHost->startSession(ip))
    {
        ui->qrStatusLabel->setText(QStringLiteral("扫码状态：启动失败 — %1").arg(m_mobileHost->lastError()));
        ui->qrImageLabel->clear();
        ui->qrUrlLabel->setText(QStringLiteral("访问 URL：（启动失败）"));
        return;
    }

    updateQrDisplay();
    updateQrStatusText();
}

// 复制当前扫码 URL 至系统剪贴板。
void RemoteControlCenter::onQrCopyClicked()
{
    const QString url = m_mobileHost ? m_mobileHost->mobileUrl() : QString();
    if (url.isEmpty())
        return;
    QApplication::clipboard()->setText(url);
}

// 结束当前扫码 HTTP 会话并清空二维码显示。
void RemoteControlCenter::onQrDisconnectClicked()
{
    if (m_mobileHost && m_mobileHost->isSessionActive())
        m_mobileHost->stopSession();
    ui->qrImageLabel->clear();
    ui->qrUrlLabel->setText(QStringLiteral("访问 URL：（已结束）"));
    ui->qrStatusLabel->setText(QStringLiteral("扫码状态：已结束"));
}

// 隐藏远程控制对话框。
void RemoteControlCenter::onCloseWindowClicked()
{
    hide();
}

// IP 下拉变更且与会话 IP 不一致时提示须刷新会话。
void RemoteControlCenter::onQrIpChanged(int /*index*/)
{
    if (!m_mobileHost || !m_mobileHost->isSessionActive())
        return;
    if (selectedQrIp() != m_mobileHost->sessionDisplayIp())
        ui->qrStatusLabel->setText(QStringLiteral("扫码状态：IP 已变更，须刷新会话"));
}

// 会话启动后更新 URL 标签与二维码图像。
void RemoteControlCenter::onQrSessionStarted(const QString &url)
{
    ui->qrUrlLabel->setText(QStringLiteral("访问 URL：%1").arg(url));
    updateQrDisplay();
    updateQrStatusText();
}

// 连接 UI 控件信号与页签切换处理。
void RemoteControlCenter::wireUi()
{
    connect(ui->remoteOffRadio, &QRadioButton::toggled, this, &RemoteControlCenter::onRemoteToggleChanged);
    connect(ui->remoteOnRadio, &QRadioButton::toggled, this, &RemoteControlCenter::onRemoteToggleChanged);
    connect(ui->wifiCopyBtn, &QPushButton::clicked, this, &RemoteControlCenter::onWifiCopyClicked);
    connect(ui->qrRefreshBtn, &QPushButton::clicked, this, &RemoteControlCenter::onQrRefreshClicked);
    connect(ui->qrCopyBtn, &QPushButton::clicked, this, &RemoteControlCenter::onQrCopyClicked);
    connect(ui->qrDisconnectBtn, &QPushButton::clicked, this, &RemoteControlCenter::onQrDisconnectClicked);
    connect(ui->closeWindowBtn, &QPushButton::clicked, this, &RemoteControlCenter::onCloseWindowClicked);
    connect(ui->qrIpCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RemoteControlCenter::onQrIpChanged);
    connect(ui->modeTabWidget, &QTabWidget::currentChanged, this, [this](int index) {
        if (!m_remoteEnabled || !m_mobileHost)
            return;
        if (index == ui->modeTabWidget->indexOf(ui->qrTab))
            tryEnsureQrSessionOnQrTab();
    });
}

// 未绑定门面时隐藏对应页签。
void RemoteControlCenter::applyTabVisibility()
{
    const int mpIdx = ui->modeTabWidget->indexOf(ui->wifiTab);
    const int qrIdx = ui->modeTabWidget->indexOf(ui->qrTab);
    if (mpIdx >= 0)
        ui->modeTabWidget->setTabVisible(mpIdx, m_remoteHost != nullptr);
    if (qrIdx >= 0)
        ui->modeTabWidget->setTabVisible(qrIdx, m_mobileHost != nullptr);
}

// 刷新 WiFi 页 endpoint、token 与 HTTP/BLE 状态标签。
void RemoteControlCenter::refreshWifiPage()
{
    if (!m_remoteHost)
        return;

    const bool listening = m_remoteEnabled && m_remoteHost->kit().http().isListening();
    ui->wifiEndpointLabel->setText(listening ? m_remoteHost->kit().httpEndpoint()
                                             : QStringLiteral("（须先开启远程服务）"));
    ui->wifiTokenLabel->setText(m_remoteHost->kit().config().token.isEmpty()
                                    ? QStringLiteral("（未配置）")
                                    : m_remoteHost->kit().config().token);
    ui->wifiStatusLabel->setText(
        QStringLiteral("HTTP 状态：%1")
            .arg(RemoteStatusText::httpSummary(listening, m_remoteHost->kit().http().lastError())));
    ui->wifiCopyBtn->setEnabled(listening);

    const bool running = m_remoteEnabled && m_remoteHost->kit().ble().isRunning();
    ui->bleDeviceLabel->setText(m_remoteHost->kit().config().bleDeviceName.isEmpty()
                                    ? QStringLiteral("（未配置）")
                                    : m_remoteHost->kit().config().bleDeviceName);
    ui->bleStatusLabel->setText(
        QStringLiteral("BLE 状态：%1")
            .arg(RemoteStatusText::bleSummary(running, m_remoteHost->kit().ble().lastError())));
}

// 刷新 BLE 页（当前与 WiFi 页共用 refreshWifiPage）。
void RemoteControlCenter::refreshBlePage()
{
    refreshWifiPage();
}

// 枚举本机 IPv4 并填充扫码 IP 下拉框。
void RemoteControlCenter::loadQrIpList()
{
    if (!ui->qrIpCombo)
        return;

    const QString keepIp = m_mobileHost && m_mobileHost->isSessionActive()
        ? m_mobileHost->sessionDisplayIp()
        : selectedQrIp();

    QSignalBlocker blocker(ui->qrIpCombo);
    ui->qrIpCombo->clear();
    const QStringList ips = NetworkHelper::getLocalIPv4List();
    if (ips.isEmpty())
    {
        ui->qrIpCombo->addItem(QStringLiteral("（未检测到局域网地址）"), QString());
        ui->qrRefreshBtn->setEnabled(false);
        return;
    }
    ui->qrRefreshBtn->setEnabled(m_remoteEnabled);
    for (const QString &ip : ips)
        ui->qrIpCombo->addItem(ip, ip);

    const int keepIdx = ui->qrIpCombo->findData(keepIp);
    if (keepIdx >= 0)
        ui->qrIpCombo->setCurrentIndex(keepIdx);
    else
    {
        const int preferredIdx = ui->qrIpCombo->findData(NetworkHelper::preferredDefaultIp());
        if (preferredIdx >= 0)
            ui->qrIpCombo->setCurrentIndex(preferredIdx);
    }
}

// 返回扫码页当前选中的 IP 字符串。
QString RemoteControlCenter::selectedQrIp() const
{
    if (!ui->qrIpCombo)
        return QString();
    return ui->qrIpCombo->currentData().toString();
}

// 远程已开启且 IP 有效时启动或恢复扫码会话。
bool RemoteControlCenter::ensureQrSessionStarted()
{
    if (!m_mobileHost || !m_remoteEnabled)
        return false;

    m_mobileHost->syncExpiredSession();

    if (m_mobileHost->isSessionActive())
    {
        updateQrDisplay();
        updateQrStatusText();
        return true;
    }

    const QString ip = selectedQrIp();
    if (ip.isEmpty())
    {
        ui->qrStatusLabel->setText(QStringLiteral("扫码状态：无可用地址"));
        return false;
    }

    if (!m_mobileHost->startSession(ip))
    {
        ui->qrStatusLabel->setText(QStringLiteral("扫码状态：启动失败 — %1").arg(m_mobileHost->lastError()));
        ui->qrImageLabel->clear();
        ui->qrUrlLabel->setText(QStringLiteral("访问 URL：（启动失败）"));
        return false;
    }

    updateQrDisplay();
    updateQrStatusText();
    return true;
}

// 根据 mobileUrl 生成并显示 QR 码图像。
void RemoteControlCenter::updateQrDisplay()
{
    if (!m_mobileHost)
        return;

    const QString url = m_mobileHost->mobileUrl();
    ui->qrUrlLabel->setText(QStringLiteral("访问 URL：%1").arg(url.isEmpty() ? QStringLiteral("（无）") : url));

    if (url.isEmpty())
    {
        ui->qrImageLabel->clear();
        return;
    }

    const QImage qr = QrCodeHelper::generateQrImage(url);
    if (qr.isNull())
    {
        ui->qrImageLabel->setText(QStringLiteral("二维码生成失败"));
        return;
    }
    ui->qrImageLabel->setPixmap(QPixmap::fromImage(qr));
}

// 更新扫码页连接状态文案。
void RemoteControlCenter::updateQrStatusText()
{
    if (!m_mobileHost)
    {
        ui->qrStatusLabel->setText(QStringLiteral("扫码状态：模块未接入"));
        return;
    }
    if (!m_remoteEnabled)
    {
        ui->qrStatusLabel->setText(QStringLiteral("扫码状态：须先开启远程服务"));
        return;
    }
    if (!m_mobileHost->isSessionActive())
    {
        ui->qrStatusLabel->setText(QStringLiteral("扫码状态：未启动"));
        return;
    }
    if (selectedQrIp() != m_mobileHost->sessionDisplayIp())
    {
        ui->qrStatusLabel->setText(QStringLiteral("扫码状态：IP 已变更，须刷新会话"));
        return;
    }
    if (m_mobileHost->isPhoneConnected())
        ui->qrStatusLabel->setText(QStringLiteral("扫码状态：客户端已连接"));
    else
        ui->qrStatusLabel->setText(QStringLiteral("扫码状态：等待客户端连接"));
}

// 刷新 WiFi/BLE/扫码各页与刷新按钮可用状态。
void RemoteControlCenter::refreshAllPages()
{
    refreshWifiPage();
    refreshBlePage();
    updateQrDisplay();
    updateQrStatusText();
    if (ui->qrRefreshBtn)
    {
        ui->qrRefreshBtn->setEnabled(m_remoteEnabled && ui->qrIpCombo && ui->qrIpCombo->count() > 0
                                     && !ui->qrIpCombo->currentData().toString().isEmpty());
    }
}

// 远程已开启且当前页为扫码页时调用 ensureQrSessionStarted。
void RemoteControlCenter::tryEnsureQrSessionOnQrTab()
{
    if (!m_remoteEnabled || !m_mobileHost)
        return;
    if (ui->modeTabWidget->currentWidget() != ui->qrTab)
        return;
    ensureQrSessionStarted();
}
