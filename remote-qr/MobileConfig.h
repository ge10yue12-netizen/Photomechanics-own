#pragma once

#include <QtGlobal>
#include <QString>

/**
 * @brief 扫码遥控通信与预览参数（独立配置文件 config/mobile.ini）。
 *
 * 与 remote/netconfig.ini 分离：本模块仅读取 [mobile] 与 [preview] 段。
 */
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

/**
 * @brief 自 exe 目录向上查找 config/mobile.ini 并解析。
 */
class MobileConfigHelper
{
public:
    static QString configFilePath();
    static bool load(MobileConfig &cfg, QString *error = nullptr);
};
