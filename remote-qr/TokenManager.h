#pragma once

#include <QDateTime>
#include <QString>

// 扫码 HTTP 会话 token 管理。
// 职责：生成 UUID token、记录 UTC 过期时间、校验与失效。
class TokenManager
{
public:
    // 设置 token 有效时长（毫秒）。
    void setLifetimeMs(qint64 ms);

    // 生成新 token 并刷新 UTC 过期时间。
    void refreshToken();

    // 清空 token 与过期时间。
    void invalidate();

    // 返回当前 token 字符串。
    QString currentToken() const;

    // 返回 token 是否存在且未过期。
    bool hasValidToken() const;

    // 校验传入 token 是否与当前有效 token 一致。
    bool verifyToken(const QString &token) const;

private:
    QString m_token;
    QDateTime m_expiresAt;
    qint64 m_lifetimeMs = 600000;
};
