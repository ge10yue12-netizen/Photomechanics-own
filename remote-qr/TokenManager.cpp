#include "TokenManager.h"

#include <QUuid>

// 设置 token 有效时长
void TokenManager::setLifetimeMs(qint64 ms)
{
    if (ms > 0)
        m_lifetimeMs = ms;
}

// 生成无连字符 UUID 作为 token
void TokenManager::refreshToken()
{
    QString raw = QUuid::createUuid().toString(QUuid::WithoutBraces);
    raw.remove(QLatin1Char('-'));
    m_token = raw;
    m_expiresAt = QDateTime::currentDateTimeUtc().addMSecs(m_lifetimeMs);
}

// 清空 token 使校验失败
void TokenManager::invalidate()
{
    m_token.clear();
    m_expiresAt = QDateTime();
}

// 返回当前 token
QString TokenManager::currentToken() const
{
    return m_token;
}

// 非空且 UTC 时间早于过期时间
bool TokenManager::hasValidToken() const
{
    return !m_token.isEmpty() && QDateTime::currentDateTimeUtc() < m_expiresAt;
}

// 字符串相等且未过期
bool TokenManager::verifyToken(const QString &token) const
{
    if (token.isEmpty() || m_token.isEmpty())
        return false;
    if (token != m_token)
        return false;
    return QDateTime::currentDateTimeUtc() < m_expiresAt;
}
