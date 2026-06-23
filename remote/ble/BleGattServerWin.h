#pragma once

#include <QByteArray>
#include <QString>
#include <functional>

class BleGattServerWin
{
public:
    using CommandHandler = std::function<void(const QByteArray &)>;
    using ErrorHandler = std::function<void(const QString &)>;

    BleGattServerWin();
    ~BleGattServerWin();

    bool start(const QString &deviceName, CommandHandler onCommand, ErrorHandler onError);
    void stop();
    bool isRunning() const;
    QString lastError() const;
    bool notifyStatus(const QByteArray &payload);

private:
    struct Impl;
    Impl *m_impl = nullptr;
};

