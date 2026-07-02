#pragma once

#include <QDateTime>
#include <QString>

// 会话 token：生成、校验、失效；有效期由 MobileConfig::tokenLifetimeSec 配置
class TokenManager
{
public:
    // 设置 token 有效时长（毫秒）
    void setLifetimeMs(qint64 ms);
    // 生成新 UUID token 并刷新 UTC 过期时间
    void refreshToken();
    // 清空 token 与过期时间
    void invalidate();
    // 返回当前 token 字符串
    QString currentToken() const;
    // token 非空且未过期
    bool hasValidToken() const;
    // 校验传入 token 与当前 token 一致且未过期
    bool verifyToken(const QString &token) const;

private:
    QString m_token;
    QDateTime m_expiresAt;
    qint64 m_lifetimeMs = 600000;
};
