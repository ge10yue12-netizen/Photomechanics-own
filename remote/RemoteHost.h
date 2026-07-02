#pragma once

#include "RemotePreviewFrameCache.h"
#include "RemoteKit.h"

#include <QJsonObject>
#include <QLabel>
#include <QObject>
#include <QStringList>
#include <functional>

class RemoteControlGuard;

// WiFi/BLE 远程控制集成门面。
// 职责：加载配置、启动/停止双通道服务、刷新状态标签、转发遥控命令与 BLE 告警。
class RemoteHost : public QObject
{
    Q_OBJECT

public:
    explicit RemoteHost(QObject *parent = nullptr);

    RemoteKit &kit() { return m_kit; }
    const RemoteKit &kit() const { return m_kit; }

    // 绑定 BLE/HTTP 状态标签；任一指针可为 nullptr。
    void setStatusLabels(QLabel *bleLabel, QLabel *httpLabel);

    // 注册设备状态 JSON 提供函数；须在 bootstrap() 之前调用。
    void setStatusProvider(std::function<QJsonObject()> provider);

    // 返回 WiFi 预览 JPEG 缓存引用。
    RemotePreviewFrameCache &previewCache() { return m_preview; }

    // 清空 WiFi 预览 JPEG 缓存。
    void clearPreviewFrame() { m_preview.clear(); }

    // 覆盖默认预览数据提供函数；未调用时使用内部 previewCache。
    void setPreviewProvider(std::function<QByteArray()> provider);

    // 注入命令通道互斥实例；HTTP 与 BLE 绑定为同一客户端组。
    void setControlGuard(RemoteControlGuard *guard);

    // 加载配置并启动 HTTP/BLE；至少一个通道成功时返回 true。
    bool bootstrap();

    // 停止 HTTP/BLE 并刷新状态标签。
    void shutdown();

    // 根据 kit 运行状态刷新 BLE/HTTP 标签文案。
    void refreshStatusLabels();

    // 向已连接 BLE 客户端推送一次状态 Notify。
    void pushBleStatus();

    // 判断 WiFi HTTP 在 ttlMs 内是否收到 GET /api/preview.jpg 请求。
    bool hadRecentPreviewRequest(qint64 nowMs, int ttlMs) const;

    // 返回启动失败通道的中文说明；成功通道不产生条目。
    QStringList startupWarnings() const;

signals:
    // HTTP 或 BLE 收到合法遥控命令时发出。
    void commandReceived(const QString &cmd);

    // BLE 服务异常或适配器错误时发出。
    void notify(const QString &message);

private:
    // 处理 BLE serverError 并刷新状态标签。
    void onBleServerError(const QString &message);

    RemoteKit m_kit;
    RemotePreviewFrameCache m_preview;
    QLabel *m_bleLabel = nullptr;
    QLabel *m_httpLabel = nullptr;
};

