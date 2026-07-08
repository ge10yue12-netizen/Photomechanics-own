#pragma once

#include <cstdint>

class QString;

// 时长解析：录制中墙钟、停止后容器 probe，与输出列表共用取整规则。
namespace RecorderVideoProbe
{

// 100ns 时间单位 → 显示秒（四舍五入，与 MF_PD_DURATION / 输出列表一致）。
int hundredNanosecondsToRoundedSeconds(std::uint64_t hundredNanoseconds);

// 从 MP4/AVI 容器读取时长（秒）；失败或未知时返回 0。
int durationSeconds(const QString &absoluteFilePath);

// 优先容器时长；probe 失败时回退 fallbackSeconds（≥0）。
int durationSecondsWithFallback(const QString &absoluteFilePath, int fallbackSeconds);

} // namespace RecorderVideoProbe
