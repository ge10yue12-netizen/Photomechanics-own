#pragma once

#include "RemotePreviewFrameCache.h"
#include "RemoteKit.h"

#include <QJsonObject>
#include <QLabel>
#include <QObject>
#include <QStringList>
#include <functional>

class RemoteControlGuard;

/**
 * 遥控集成门面：配置加载、HTTP/BLE 启动、状态行刷新、命令转发。
 * 复制 remote/ + config/netconfig.ini 后，宿主工程只需构造 RemoteHost 并 bootstrap。
 */
class RemoteHost : public QObject
{
    Q_OBJECT

public:
    explicit RemoteHost(QObject *parent = nullptr);

    RemoteKit &kit() { return m_kit; }
    const RemoteKit &kit() const { return m_kit; }

    // 绑定日志区 BLE/HTTP 状态标签；任一可为 nullptr。
    void setStatusLabels(QLabel *bleLabel, QLabel *httpLabel);

    // 注册状态 JSON，须在 bootstrap() 前调用。
    void setStatusProvider(std::function<QJsonObject()> provider);

    /** WiFi 预览 JPEG 缓存；宿主在取帧路径调用 updateFrame，与 remote-qr 模块独立。 */
    RemotePreviewFrameCache &previewCache() { return m_preview; }
    void clearPreviewFrame() { m_preview.clear(); }

    /** 可选：覆盖默认 previewCache 提供函数。 */
    void setPreviewProvider(std::function<QByteArray()> provider);

    /** 与 remote-qr 共用同一 RemoteControlGuard；小程序 HTTP/BLE 为同组，与扫码 Web 互斥。 */
    void setControlGuard(RemoteControlGuard *guard);

    // loadConfig + start + 刷新状态行；至少一个通道成功时返回 true。
    bool bootstrap();

    void shutdown();
    void refreshStatusLabels();
    void pushBleStatus();

    // 启动失败的中文说明，供宿主写日志（成功通道不生成项）。
    QStringList startupWarnings() const;

signals:
    void commandReceived(const QString &cmd);
    void notify(const QString &message);

private:
    void onBleServerError(const QString &message);

    RemoteKit m_kit;
    RemotePreviewFrameCache m_preview;
    QLabel *m_bleLabel = nullptr;
    QLabel *m_httpLabel = nullptr;
};
