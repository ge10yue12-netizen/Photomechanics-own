#pragma once

#include <QtGlobal>
#include <QString>

// config/mobile.ini 解析结果：HTTP 端口、会话有效期与预览参数。
struct MobileConfig
{
    quint16 httpPort = 8080;
    int tokenLifetimeSec = 600;

    int previewMaxWidth = 480;
    int previewMaxHeight = 360;
    int previewJpegQuality = 55;
    int previewPollMs = 120;
    int statusPollMs = 800;
};

// config/mobile.ini 定位与解析。
// 配置文件缺失时使用结构体默认值。
class MobileConfigHelper
{
public:
    // 返回首个存在的 config/mobile.ini 绝对路径；未找到时返回空字符串。
    static QString configFilePath();

    // 读取 mobile.ini 并校验数值范围；失败时通过 error 返回原因。
    static bool load(MobileConfig &cfg, QString *error = nullptr);
};
