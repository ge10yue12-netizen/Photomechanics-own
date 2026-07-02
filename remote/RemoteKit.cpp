#include "RemoteKit.h"

// 构造：将 HTTP/BLE 的 commandReceived 统一转发至本对象。
RemoteKit::RemoteKit(QObject *parent)
    : QObject(parent)
{
    connect(&m_http, &RemoteControlServer::commandReceived, this, &RemoteKit::commandReceived);
    connect(&m_ble, &BleControlServer::commandReceived, this, &RemoteKit::commandReceived);
}

// 定位 netconfig.ini 并解析至 m_cfg。
bool RemoteKit::loadConfig()
{
    m_lastError.clear();
    m_configPath = NetConfigHelper::configFilePath();
    return NetConfigHelper::load(m_cfg, &m_lastError);
}

// 并行启动 HTTP 与 BLE；任一路成功即返回 true。
bool RemoteKit::start()
{
    m_lastError.clear();
    const bool httpOk = m_http.start(m_cfg);
    const bool bleOk = m_ble.start(m_cfg);
    if (!httpOk && !bleOk)
    {
        m_lastError = m_http.lastError().isEmpty() ? m_ble.lastError() : m_http.lastError();
        return false;
    }
    return true;
}

// 停止双通道并释放 HTTP/BLE 命令占用。
void RemoteKit::stop()
{
    m_ble.stop();
    m_http.stop();
    if (m_controlGuard)
    {
        m_controlGuard->release(RemoteControlSource::MiniProgramHttp);
        m_controlGuard->release(RemoteControlSource::MiniProgramBle);
    }
}

// 向 HTTP/BLE 注册同一状态 JSON 提供函数。
void RemoteKit::setStatusProvider(std::function<QJsonObject()> provider)
{
    m_http.setStatusProvider(provider);
    m_ble.setStatusProvider(std::move(provider));
}

// 向 HTTP 注册命令互斥及来源标识。
void RemoteKit::setControlGuard(RemoteControlGuard *guard)
{
    m_controlGuard = guard;
    m_http.setControlGuard(guard, RemoteControlSource::MiniProgramHttp);
    m_ble.setControlGuard(guard, RemoteControlSource::MiniProgramBle);
}

// 向 HTTP 注册预览 JPEG 提供函数。
void RemoteKit::setPreviewProvider(std::function<QByteArray()> provider)
{
    m_http.setPreviewProvider(std::move(provider));
}

// BLE 运行中时触发一次状态 Notify。
void RemoteKit::pushBleStatus()
{
    if (m_ble.isRunning())
        m_ble.pushStatus();
}
