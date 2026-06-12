#pragma once

#include <QImage>
#include <QString>

// 目标采集分辨率（open 时写入相机，超出硬件上限则取 max）
const int kImageWidth = 2592;
const int kImageHeight = 2048;

// 相机 GenICam 节点可读回的曝光/增益/帧率合法范围
struct CamParamLimits
{
    double fpsMin = 1.0;
    double fpsMax = 120.0;
    double exposureMinUs = 1.0;      // 曝光下限（微秒）
    double exposureMaxUs = 1000000.0; // 曝光上限（微秒）
    double gainMinDb = 0.0;           // 增益下限（dB）
    double gainMaxDb = 24.0;          // 增益上限（dB）
};

// 阶段采集表格中的一行配置
struct StageItem
{
    QString name;           // 阶段名称
    double durationSec = 1.0; // 本阶段持续时间（秒）
    double fps = 20.0;      // 本阶段目标帧率
    bool saveImage = false; // 是否按帧率定时存图
};

// 存图任务队列元素：图像数据与目标文件路径
struct SaveTask
{
    QImage image;     // 待写入图像（入队前已完成拷贝）
    QString filePath; // BMP 目标路径
};

// 存图目录切分策略（与 saveModeCombo 索引一致）
enum class SaveFolderMode
{
    SingleFolder = 0, // 全部写入根目录
    ByCount = 1,      // 每 N 张新建 CAMERAxxx 子目录
    ByTime = 2        // 每 N 秒新建 CAMERAxxx 子目录
};
