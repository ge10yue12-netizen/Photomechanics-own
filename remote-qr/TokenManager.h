#pragma once

#include <QDateTime>
#include <QString>

/**
 * @brief 扫码会话 token：生成、校验、失效。
 *
 * 默认有效期 10 分钟；由 MobileConfig::tokenLifetimeSec 配置。
 */
class TokenManager
{
public:
    void setLifetimeMs(qint64 ms);

    void refreshToken();
    void invalidate();

    QString currentToken() const;
    bool hasValidToken() const;
    bool verifyToken(const QString &token) const;

private:
    QString m_token;
    QDateTime m_expiresAt;
    qint64 m_lifetimeMs = 600000;
};
