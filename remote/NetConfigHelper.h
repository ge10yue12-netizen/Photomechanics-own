#pragma once

#include <QtGlobal>
#include <QString>

// netconfig.ini 解析结果：认证口令、HTTP 绑定、BLE 广播名。
struct RemoteConfig
{
    QString token;
    QString httpBind;
    quint16 httpPort = 0;
    QString bleDeviceName;

    // 返回配置中的 HTTP 对外地址，格式为 bind:port。
    QString httpEndpoint() const;
};

// config/netconfig.ini 定位与解析。
// 自可执行文件目录向上最多 8 层查找配置文件。
class NetConfigHelper
{
public:
    // 返回首个存在的 config/netconfig.ini 绝对路径；未找到时返回空字符串。
    static QString configFilePath();

    // 读取并校验 netconfig.ini，结果写入 cfg；失败时通过 error 返回原因。
    static bool load(RemoteConfig &cfg, QString *error = nullptr);
};
