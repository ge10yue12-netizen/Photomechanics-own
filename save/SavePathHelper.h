#pragma once

#include "core/AppTypes.h"
#include <QDateTime>
#include <QDir>
#include <QString>

// SavePathHelper — 生成 PicNNN.bmp 路径；阶段采集按 Loop/阶段名 分目录，手动存图走 CAMERA 模式
class SavePathHelper
{
public:
    static QString defaultRootPath(); // 可执行文件上两级目录下的 Images

    void setRootPath(const QString &path) { m_rootPath = path.trimmed(); }
    void setMode(SaveFolderMode mode) { m_mode = mode; }
    void setPicsPerFolder(int count) { m_picsPerFolder = count > 0 ? count : 1; }
    void setSecondsPerFolder(int seconds) { m_secondsPerFolder = seconds > 0 ? seconds : 1; }
    void setMaxSaveCount(int maxCount) { m_maxSaveCount = maxCount; } // 0 表示不限制

    void resetSession();              // 显式清零（切换目录等场景）
    void resumeFromDisk();            // 扫描磁盘，初始化 Pic/CAMERA 计数（仅启动/换目录）
    void syncIndexWithDisk();         // 手动存图：只把序号往上对齐磁盘，绝不回退
    void beginStageCapture();         // 进入阶段采集：启用 Loop/阶段 分目录
    void endStageCapture();           // 退出阶段采集：恢复手动存图路径逻辑
    void setStageContext(int loopIndex, const QString &stageName); // 新阶段/新轮：切换子目录，Pic 从 001 续接
    bool isStageCaptureActive() const { return m_stageCaptureActive; }
    bool isSaveLimitReached() const { return m_maxSaveCount > 0 && m_totalSaved >= m_maxSaveCount; }
    QString nextFilePath(bool *ok = nullptr); // 分配下一张 BMP 路径（尚未写盘）
    void onFileSaved();               // 写盘成功后更新计数

private:
    QString activeFolderPath();       // 手动存图：当前应写入的目录（必要时新建 CAMERA 文件夹）
    QString stageFolderPath() const;  // 阶段存图：{root}/LoopNNN/阶段名/
    static QString sanitizeFolderName(const QString &name);
    bool ensureFolderExists(const QString &dir) { return QDir().mkpath(dir); }
    bool needNewCameraFolder() const;
    bool createNewCameraFolder();

    QString m_rootPath;
    SaveFolderMode m_mode = SaveFolderMode::SingleFolder;
    int m_picsPerFolder = 100;
    int m_secondsPerFolder = 10;
    int m_maxSaveCount = 0;
    int m_picIndex = 1;               // 手动存图：当前目录内下一张 Pic 序号
    int m_totalSaved = 0;             // 已成功写盘总数（阶段+手动共用上限）
    int m_picsInCurrentFolder = 0;    // 手动存图：当前 CAMERA 子目录内已写张数
    int m_cameraFolderIndex = 1;      // 手动存图：下一个 CAMERA 子目录编号
    QString m_currentFolder;          // 手动存图：当前活跃子目录绝对路径
    QDateTime m_folderStartTime;      // ByTime 模式：当前子目录创建时刻
    bool m_stageCaptureActive = false; // 是否处于阶段采集（分 Loop/阶段 目录）
    int m_loopIndex = 0;              // 当前轮次（1-based）
    QString m_stageName;              // 当前阶段名称（用于子目录名）
    int m_stagePicIndex = 1;          // 当前阶段子目录内下一张 Pic 序号
};
