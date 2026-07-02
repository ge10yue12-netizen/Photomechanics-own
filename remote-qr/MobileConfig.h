#pragma once

#include <QtGlobal>
#include <QString>

// config/mobile.ini 解析结果：[mobile] 通信参数、[preview] 预览参数
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

// 自可执行文件目录向上查找 config/mobile.ini 并加载
class MobileConfigHelper
{
public:
    // 返回首个存在的 config/mobile.ini 绝对路径；未找到时为空
    static QString configFilePath();
    // 读取配置并写入 cfg；文件缺失时使用默认值
    static bool load(MobileConfig &cfg, QString *error = nullptr);
};
