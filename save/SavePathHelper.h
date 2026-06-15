#pragma once

#include "core/AppTypes.h"
#include <QDateTime>
#include <QDir>
#include <QString>

// SavePathHelper：PicNNN.bmp 路径生成；阶段采集按阶段名分目录，手动存图使用 CAMERA 分目录
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
    void resumeFromDisk();            // 扫描磁盘，同步 Pic/CAMERA 序号与 m_totalSaved
    int savedCountOnDisk() const;     // 实时统计磁盘 Pic*.bmp 总数（含阶段/CAMERA 子目录）
    void beginStageCapture();         // 进入阶段采集：启用 {root}/阶段名/ 分目录
    void endStageCapture();           // 退出阶段采集：恢复手动存图路径逻辑
    void setStageContext(int loopIndex, const QString &stageName); // 切换阶段子目录；新目录 Pic 从 001，已有则 max+1 续接
    bool isStageCaptureActive() const { return m_stageCaptureActive; }
    bool isSaveLimitReached() const;  // 按磁盘实际张数与 maxSaveCount 比较
    int totalSaved() const { return savedCountOnDisk(); } // 与磁盘一致的总保存张数（供 UI 显示）
    QString nextFilePath(bool *ok = nullptr); // 预分配 BMP 路径；Pic 序号在 onFileSaved 成功后递增
    void onFileSaved();               // 写盘成功后递增 totalSaved 与 Pic 序号

private:
    QString activeFolderPath();       // 手动存图：当前应写入的目录（必要时新建 CAMERA 文件夹）
    QString stageFolderPath() const;  // 阶段存图：{root}/阶段名/
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
    bool m_stageCaptureActive = false; // 是否处于阶段采集（按阶段名分目录）
    int m_loopIndex = 0;              // 当前轮次（1-based，仅日志/UI，不参与路径）
    QString m_stageName;              // 当前阶段名称（用于子目录名）
    int m_stagePicIndex = 1;          // 当前阶段子目录内下一张 Pic 序号
};
