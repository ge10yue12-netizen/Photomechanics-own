#include "BleGattServerWin.h"
#include "BleProtocol.h"

#include <QUuid>
#include <cstring>

#ifdef _WIN32
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Storage.Streams.h>

#include <mutex>
#include <vector>

using namespace winrt;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::Advertisement;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace Windows::Storage::Streams;

namespace
{
// 将协议字符串 UUID 转为 WinRT guid。
winrt::guid toGuid(const char *text)
{
    const QUuid q(text);
    return winrt::guid{q.data1,
                       q.data2,
                       q.data3,
                       {q.data4[0], q.data4[1], q.data4[2], q.data4[3], q.data4[4], q.data4[5], q.data4[6], q.data4[7]}};
}
}

struct BleGattServerWin::Impl
{
    GattServiceProvider serviceProvider{nullptr};
    GattLocalCharacteristic cmdChar{nullptr};
    GattLocalCharacteristic statusChar{nullptr};
    event_token writeToken{};
    event_token advStatusToken{};
    QString advHint;
    bool running = false;
    QString lastError;
    BleGattServerWin::CommandHandler onCommand;
    BleGattServerWin::ErrorHandler onError;
    std::mutex mutex;
};

BleGattServerWin::BleGattServerWin() = default;

BleGattServerWin::~BleGattServerWin()
{
    stop();
}

bool BleGattServerWin::start(const QString &deviceName, CommandHandler onCommand, ErrorHandler onError)
{

    stop();
    m_impl = new Impl();
    m_impl->onCommand = std::move(onCommand);
    m_impl->onError = std::move(onError);

    try
    {
        // --- WinRT GATT 服务与特征创建 ---
        const winrt::guid serviceId = toGuid(BleProtocol::serviceUuid());
        const auto providerResult = GattServiceProvider::CreateAsync(serviceId).get();
        if (providerResult.Error() != BluetoothError::Success || !providerResult.ServiceProvider())
        {
            m_impl->lastError = QStringLiteral("create BLE service failed");
            if (m_impl->onError)
                m_impl->onError(m_impl->lastError);
            stop();
            return false;
        }

        m_impl->serviceProvider = providerResult.ServiceProvider();
        auto service = m_impl->serviceProvider.Service();

        GattLocalCharacteristicParameters cmdParams;
        cmdParams.CharacteristicProperties(GattCharacteristicProperties::Write
                                             | GattCharacteristicProperties::WriteWithoutResponse);
        const auto cmdResult = service.CreateCharacteristicAsync(toGuid(BleProtocol::cmdCharacteristicUuid()),
                                                                 cmdParams)
                                   .get();
        if (cmdResult.Error() != BluetoothError::Success || !cmdResult.Characteristic())
        {
            m_impl->lastError = QStringLiteral("create command characteristic failed");
            if (m_impl->onError)
                m_impl->onError(m_impl->lastError);
            stop();
            return false;
        }
        m_impl->cmdChar = cmdResult.Characteristic();

        GattLocalCharacteristicParameters statusParams;
        statusParams.CharacteristicProperties(GattCharacteristicProperties::Read
                                              | GattCharacteristicProperties::Notify);
        const auto statusResult = service.CreateCharacteristicAsync(toGuid(BleProtocol::statusCharacteristicUuid()),
                                                                    statusParams)
                                      .get();
        if (statusResult.Error() != BluetoothError::Success || !statusResult.Characteristic())
        {
            m_impl->lastError = QStringLiteral("create status characteristic failed");
            if (m_impl->onError)
                m_impl->onError(m_impl->lastError);
            stop();
            return false;
        }
        m_impl->statusChar = statusResult.Characteristic();

        // --- 命令特征写入处理：在 WinRT 回调线程中读取载荷并转发 onCommand ---
        Impl *impl = m_impl;
        m_impl->writeToken = m_impl->cmdChar.WriteRequested(
            [impl](GattLocalCharacteristic const &, GattWriteRequestedEventArgs const &args) {
                auto deferral = args.GetDeferral();
                try
                {
                    const auto request = args.GetRequestAsync().get();
                    const auto buffer = request.Value();
                    if (buffer && buffer.Length() > 0)
                    {
                        std::vector<uint8_t> data(buffer.Length());
                        DataReader reader = DataReader::FromBuffer(buffer);
                        reader.ReadBytes(data);
                        const QByteArray bytes(reinterpret_cast<const char *>(data.data()),
                                               static_cast<int>(data.size()));
                        BleGattServerWin::CommandHandler handler;
                        {
                            std::lock_guard<std::mutex> lock(impl->mutex);
                            handler = impl->onCommand;
                        }
                        if (handler)
                            handler(bytes);
                    }
                }
                catch (...)
                {
                }
                deferral.Complete();
            });

        // --- 开始可连接、可发现的 BLE 广播 ---
        GattServiceProviderAdvertisingParameters advParams;
        advParams.IsConnectable(true);
        advParams.IsDiscoverable(true);
        m_impl->serviceProvider.StartAdvertising(advParams);

        m_impl->running = true;
        return true;
    }
    catch (const winrt::hresult_error &ex)
    {
        if (m_impl)
        {
            m_impl->lastError = QString::fromWCharArray(ex.message().c_str());
            if (m_impl->onError)
                m_impl->onError(m_impl->lastError);
        }
        stop();
        return false;
    }
    catch (...)
    {
        if (m_impl)
        {
            m_impl->lastError = QStringLiteral("BLE start unknown error");
            if (m_impl->onError)
                m_impl->onError(m_impl->lastError);
        }
        stop();
        return false;
    }
}

void BleGattServerWin::stop()
{
    if (!m_impl)
        return;

    if (m_impl->cmdChar && m_impl->writeToken)
    {
        m_impl->cmdChar.WriteRequested(m_impl->writeToken);
        m_impl->writeToken = {};
    }
    if (m_impl->serviceProvider)
    {
        m_impl->serviceProvider.StopAdvertising();
        m_impl->serviceProvider = nullptr;
    }
    m_impl->cmdChar = nullptr;
    m_impl->statusChar = nullptr;
    m_impl->running = false;
    delete m_impl;
    m_impl = nullptr;
}

bool BleGattServerWin::isRunning() const
{
    return m_impl && m_impl->running;
}

QString BleGattServerWin::lastError() const
{
    return m_impl ? m_impl->lastError : QString();
}

// 向 STATUS 特征推送 Notify；须在 WinRT 工作线程调用。
bool BleGattServerWin::notifyStatus(const QByteArray &payload)
{
    if (!m_impl || !m_impl->running || !m_impl->statusChar || payload.isEmpty())
        return false;

    try
    {
        std::vector<uint8_t> data(static_cast<size_t>(payload.size()));
        if (!data.empty())
            memcpy(data.data(), payload.constData(), data.size());
        DataWriter writer;
        writer.WriteBytes(data);
        const auto buffer = writer.DetachBuffer();
        m_impl->statusChar.NotifyValueAsync(buffer).get();
        return true;
    }
    catch (...)
    {
        return false;
    }
}

#else

BleGattServerWin::BleGattServerWin() = default;
BleGattServerWin::~BleGattServerWin() = default;

bool BleGattServerWin::start(const QString &, CommandHandler, ErrorHandler onError)
{
    if (onError)
        onError(QStringLiteral("BLE remote supports Windows only"));
    return false;
}

void BleGattServerWin::stop() {}

bool BleGattServerWin::isRunning() const { return false; }

QString BleGattServerWin::lastError() const { return QStringLiteral("BLE remote supports Windows only"); }

bool BleGattServerWin::notifyStatus(const QByteArray &) { return false; }

#endif