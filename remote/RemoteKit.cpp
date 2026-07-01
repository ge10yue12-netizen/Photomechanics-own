#include "RemoteKit.h"

RemoteKit::RemoteKit(QObject *parent)
    : QObject(parent)
{
    // HTTP 与 BLE 合法命令统一转发至 commandReceived。
    connect(&m_http, &RemoteControlServer::commandReceived, this, &RemoteKit::commandReceived);
    connect(&m_ble, &BleControlServer::commandReceived, this, &RemoteKit::commandReceived);
}

bool RemoteKit::loadConfig()
{
    m_lastError.clear();
    m_configPath = NetConfigHelper::configFilePath();
    return NetConfigHelper::load(m_cfg, &m_lastError);
}

// 并行启动 HTTP 与 BLE；任一成功即返回 true。
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

void RemoteKit::setStatusProvider(std::function<QJsonObject()> provider)
{
    m_http.setStatusProvider(provider);
    m_ble.setStatusProvider(std::move(provider));
}

void RemoteKit::setControlGuard(RemoteControlGuard *guard)
{
    m_controlGuard = guard;
    m_http.setControlGuard(guard, RemoteControlSource::MiniProgramHttp);
    m_ble.setControlGuard(guard, RemoteControlSource::MiniProgramBle);
}

void RemoteKit::setPreviewProvider(std::function<QByteArray()> provider)
{
    m_http.setPreviewProvider(std::move(provider));
}

void RemoteKit::pushBleStatus()
{
    if (m_ble.isRunning())
        m_ble.pushStatus();
}