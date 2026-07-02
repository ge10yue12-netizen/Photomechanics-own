#pragma once

#include <QDialog>

class MobileHost;
class RemoteHost;

namespace Ui {
class RemoteControlCenter;
}

// 远程控制管理对话框。
// 职责：远程总闸 UI、WiFi/BLE 状态展示、扫码会话与二维码管理。
class RemoteControlCenter : public QDialog
{
    Q_OBJECT

public:
    explicit RemoteControlCenter(QWidget *parent = nullptr);
    ~RemoteControlCenter() override;

    // 绑定 WiFi/BLE 遥控门面；未绑定时隐藏对应页签。
    void bindRemoteHost(RemoteHost *host);

    // 绑定扫码 HTTP 门面；未绑定时隐藏对应页签。
    void bindMobileHost(MobileHost *host);

    // 返回远程总闸是否处于开启状态。
    bool isRemoteEnabled() const;

    // 同步总闸 UI 状态；不触发 remoteEnabledChanged 信号。
    void setRemoteEnabled(bool enabled);

signals:
    // 用户在弹窗内切换远程总闸时发出。
    void remoteEnabledChanged(bool enabled);

protected:
    // 显示时刷新各页信息与扫码 IP 列表。
    void showEvent(QShowEvent *event) override;

    // 关闭窗口仅隐藏，不停止远程服务。
    void closeEvent(QCloseEvent *event) override;

private slots:
    // 处理远程总闸单选切换。
    void onRemoteToggleChanged();

    // 复制 WiFi HTTP 地址至剪贴板。
    void onWifiCopyClicked();

    // 刷新扫码 HTTP 会话。
    void onQrRefreshClicked();

    // 复制扫码 URL 至剪贴板。
    void onQrCopyClicked();

    // 结束当前扫码 HTTP 会话。
    void onQrDisconnectClicked();

    // 隐藏弹窗。
    void onCloseWindowClicked();

    // 扫码 IP 下拉变更时更新状态提示。
    void onQrIpChanged(int index);

    // 扫码会话启动后更新 URL 与二维码。
    void onQrSessionStarted(const QString &url);

private:
    // 连接 UI 控件信号。
    void wireUi();

    // 按门面绑定结果设置页签可见性。
    void applyTabVisibility();

    // 刷新 WiFi 页 endpoint、token 与 HTTP 状态。
    void refreshWifiPage();

    // 刷新 BLE 页设备名与运行状态。
    void refreshBlePage();

    // 加载本机 IPv4 列表至扫码 IP 下拉框。
    void loadQrIpList();

    // 返回扫码页当前选中的 IP。
    QString selectedQrIp() const;

    // 远程已开启且 IP 有效时启动扫码会话。
    bool ensureQrSessionStarted();

    // 更新扫码页二维码与 URL 标签。
    void updateQrDisplay();

    // 更新扫码页连接状态文案。
    void updateQrStatusText();

    // 刷新顶栏与各页状态摘要。
    void refreshAllPages();

    // 远程已开启且当前为扫码页时尝试启动扫码会话。
    void tryEnsureQrSessionOnQrTab();

    Ui::RemoteControlCenter *ui = nullptr;
    RemoteHost *m_remoteHost = nullptr;
    MobileHost *m_mobileHost = nullptr;
    bool m_remoteEnabled = false;
    bool m_syncingRemoteUi = false;
    bool m_qrRefreshInProgress = false;
};

