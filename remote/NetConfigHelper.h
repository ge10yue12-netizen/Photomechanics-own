#pragma once

#include <QtGlobal>
#include <QString>

struct RemoteConfig
{
    QString token;
    QString httpBind;
    quint16 httpPort = 0;
    QString bleDeviceName;

    // 返回 netconfig.ini 中 http/bind 与 http/port，供日志与手机填写。
    QString httpEndpoint() const;
};

class NetConfigHelper
{
public:
    // 从 exe 所在目录向上查找 config/netconfig.ini，返回首个存在的绝对路径；未找到返回空字符串。
    static QString configFilePath();

    // 读取 netconfig.ini 填入 cfg；校验必填项与 bind 格式，失败返回 false 并通过 error 说明原因。
    static bool load(RemoteConfig &cfg, QString *error = nullptr);
};