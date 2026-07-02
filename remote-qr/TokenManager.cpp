#include "TokenManager.h"

#include <QUuid>

// 设置 token 有效时长；ms 须大于 0。
void TokenManager::setLifetimeMs(qint64 ms)
{
    if (ms > 0)
        m_lifetimeMs = ms;
}

// 生成 UUID token 并写入 UTC 过期时间。
void TokenManager::refreshToken()
{
    QString raw = QUuid::createUuid().toString(QUuid::WithoutBraces);
    raw.remove(QLatin1Char('-'));
    m_token = raw;
    m_expiresAt = QDateTime::currentDateTimeUtc().addMSecs(m_lifetimeMs);
}

// 清空 token 与过期时间。
void TokenManager::invalidate()
{
    m_token.clear();
    m_expiresAt = QDateTime();
}

// 返回当前 token 字符串。
QString TokenManager::currentToken() const
{
    return m_token;
}

// 判断 token 非空且未过期。
bool TokenManager::hasValidToken() const
{
    return !m_token.isEmpty() && QDateTime::currentDateTimeUtc() < m_expiresAt;
}

// 校验 token 字符串与有效期。
bool TokenManager::verifyToken(const QString &token) const
{
    if (token.isEmpty() || m_token.isEmpty())
        return false;
    if (token != m_token)
        return false;
    return QDateTime::currentDateTimeUtc() < m_expiresAt;
}
