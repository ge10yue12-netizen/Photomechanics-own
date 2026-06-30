#pragma once

#include <QStringList>

/**
 * @brief 枚举可用于二维码 URL 的本机 IPv4。
 *
 * 过滤回环、APIPA、常见虚拟网卡；Wi-Fi 网卡地址排在前列。
 */
class NetworkHelper
{
public:
    static QStringList getLocalIPv4List();
    static QString preferredDefaultIp();
};
