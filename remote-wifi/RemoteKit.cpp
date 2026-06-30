#include "RemoteKit.h"

RemoteKit::RemoteKit(QObject *parent)
    : QObject(parent)
{
    connect(&m_http, &RemoteControlServer::commandReceived, this, &RemoteKit::commandReceived);
}

bool RemoteKit::loadConfig()
{
    m_lastError.clear();
    m_configPath = NetConfigHelper::configFilePath();
    return NetConfigHelper::load(m_cfg, &m_lastError);
}

bool RemoteKit::start()
{
    m_lastError.clear();
    if (!m_http.start(m_cfg))
    {
        m_lastError = m_http.lastError();
        return false;
    }
    return true;
}

void RemoteKit::stop()
{
    m_http.stop();
}

void RemoteKit::setStatusProvider(std::function<QJsonObject()> provider)
{
    m_http.setStatusProvider(std::move(provider));
}
