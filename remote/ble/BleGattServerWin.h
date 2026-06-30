#pragma once

#include <QByteArray>
#include <QString>
#include <functional>

// WinRT GATT Server 封装：在调用线程上创建服务、处理写入并推送 Notify。
// 须在已初始化 WinRT MTA 的线程中构造与调用（由 BleWinRtWorker 保证）。
class BleGattServerWin
{
public:
    using CommandHandler = std::function<void(const QByteArray &)>;
    using ErrorHandler = std::function<void(const QString &)>;

    BleGattServerWin();
    ~BleGattServerWin();

    // 创建 GATT 服务、注册写入回调并开始广播；失败时调用 onError。
    bool start(const QString &deviceName, CommandHandler onCommand, ErrorHandler onError);
    void stop();
    bool isRunning() const;
    QString lastError() const;
    // 向 STATUS 特征发送 Notify，payload 通常为 compactStatusJson 结果。
    bool notifyStatus(const QByteArray &payload);

private:
    struct Impl;
    Impl *m_impl = nullptr;
};