#pragma once

#include <QStringList>

// 本机 IPv4 地址枚举。
// 过滤回环、APIPA 与常见虚拟网卡；无线网卡地址排在前列。
class NetworkHelper
{
public:
    // 返回可用于 HTTP URL 的本机 IPv4 列表。
    static QStringList getLocalIPv4List();

    // 返回列表首项作为默认 IP；列表为空时返回空字符串。
    static QString preferredDefaultIp();
};
