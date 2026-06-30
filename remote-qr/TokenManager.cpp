#include "TokenManager.h"

#include <QUuid>

void TokenManager::setLifetimeMs(qint64 ms)
{
    if (ms > 0)
        m_lifetimeMs = ms;
}

/** 生成新 token 并刷新 UTC 过期时间。 */
void TokenManager::refreshToken()
{
    QString raw = QUuid::createUuid().toString(QUuid::WithoutBraces);
    raw.remove(QLatin1Char('-'));
    m_token = raw;
    m_expiresAt = QDateTime::currentDateTimeUtc().addMSecs(m_lifetimeMs);
}

void TokenManager::invalidate()
{
    m_token.clear();
    m_expiresAt = QDateTime();
}

QString TokenManager::currentToken() const
{
    return m_token;
}

bool TokenManager::hasValidToken() const
{
    return !m_token.isEmpty() && QDateTime::currentDateTimeUtc() < m_expiresAt;
}

bool TokenManager::verifyToken(const QString &token) const
{
    if (token.isEmpty() || m_token.isEmpty())
        return false;
    if (token != m_token)
        return false;
    return QDateTime::currentDateTimeUtc() < m_expiresAt;
}
