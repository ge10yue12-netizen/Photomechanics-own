#pragma once

#include "core/AppTypes.h"
#include <QDateTime>
#include <QDir>
#include <QString>

// SavePathHelper — 生成 PicNNN.bmp 路径，并按模式切分 CAMERAxxx 子目录
class SavePathHelper
{
public:
    static QString defaultRootPath(); // 可执行文件上两级目录下的 Images

    void setRootPath(const QString &path) { m_rootPath = path.trimmed(); }
    void setMode(SaveFolderMode mode) { m_mode = mode; }
    void setPicsPerFolder(int count) { m_picsPerFolder = count > 0 ? count : 1; }
    void setSecondsPerFolder(int seconds) { m_secondsPerFolder = seconds > 0 ? seconds : 1; }
    void setMaxSaveCount(int maxCount) { m_maxSaveCount = maxCount; } // 0 表示不限制

    void resetSession();              // 新一次阶段采集开始前清零计数
    void resetForNewLoop() { m_picIndex = 1; } // 新一轮阶段：Pic 序号从 1 重计
    bool isSaveLimitReached() const { return m_maxSaveCount > 0 && m_totalSaved >= m_maxSaveCount; }
    QString nextFilePath(bool *ok = nullptr); // 分配下一张 BMP 路径（尚未写盘）
    void onFileSaved() { ++m_totalSaved; ++m_picsInCurrentFolder; }

private:
    QString activeFolderPath();       // 当前应写入的目录（必要时新建 CAMERA 文件夹）
    bool ensureFolderExists(const QString &dir) { return QDir().mkpath(dir); }
    bool needNewCameraFolder() const;
    bool createNewCameraFolder();

    QString m_rootPath;
    SaveFolderMode m_mode = SaveFolderMode::SingleFolder;
    int m_picsPerFolder = 100;
    int m_secondsPerFolder = 10;
    int m_maxSaveCount = 0;
    int m_picIndex = 1;               // 当前目录内下一张 Pic 序号
    int m_totalSaved = 0;             // 本次 session 已成功写盘总数
    int m_picsInCurrentFolder = 0;    // 当前 CAMERA 子目录内已写张数
    int m_cameraFolderIndex = 1;      // 下一个 CAMERA 子目录编号
    QString m_currentFolder;          // 当前活跃子目录绝对路径
    QDateTime m_folderStartTime;      // ByTime 模式：当前子目录创建时刻
};
