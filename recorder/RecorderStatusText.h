#pragma once

#include <QString>

// 录屏模块 UI 文案（header-only）。
namespace RecorderStatusText
{

// 将核心状态标签转为 QString。
inline QString stateSummary(const char *label)
{
    return label ? QString::fromUtf8(label) : QStringLiteral("—");
}

// 录制摘要：状态 + 可选错误。
inline QString sessionSummary(const char *stateLabel, const QString &lastError)
{
    if (!lastError.isEmpty())
        return lastError;
    return stateSummary(stateLabel);
}

// 格式化时长 HH:MM:SS。
inline QString formatDuration(int seconds)
{
    if (seconds < 0)
        seconds = 0;
    const int h = seconds / 3600;
    const int m = (seconds % 3600) / 60;
    const int s = seconds % 60;
    return QStringLiteral("%1:%2:%3")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'));
}

} // namespace RecorderStatusText
