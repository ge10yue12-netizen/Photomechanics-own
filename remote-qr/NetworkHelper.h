#pragma once

#include <QStringList>

// 枚举可用于二维码 URL 的本机 IPv4；过滤回环、APIPA、虚拟网卡
class NetworkHelper
{
public:
    // 返回 IPv4 列表；Wi-Fi 地址排在前面
    static QStringList getLocalIPv4List();
    // 返回列表首项作为默认 IP
    static QString preferredDefaultIp();
};
