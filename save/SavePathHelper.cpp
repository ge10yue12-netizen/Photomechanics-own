// SavePathHelper.cpp — 存图路径与 CAMERA/Pic 命名规则

#include "SavePathHelper.h"
#include <QCoreApplication>
#include <QDir>
#include <QRegularExpression>

namespace
{
// 从 Pic001.bmp 解析序号，非法文件名返回 -1
int parsePicNumber(const QString &fileName)
{
    static const QRegularExpression re(QStringLiteral("^Pic(\\d+)\\.bmp$"), QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = re.match(fileName);
    if (!m.hasMatch())
        return -1;
    bool ok = false;
    const int n = m.captured(1).toInt(&ok);
    return ok ? n : -1;
}

// 统计目录内 Pic*.bmp 的最大序号
int maxPicInDir(const QDir &dir)
{
    int maxN = 0;
    const QStringList files = dir.entryList(QStringList() << QStringLiteral("Pic*.bmp"), QDir::Files);
    for (const QString &f : files)
    {
        const int n = parsePicNumber(f);
        if (n > maxN)
            maxN = n;
    }
    return maxN;
}

// 统计根目录及所有 CAMERA 子目录中的 BMP 文件总数（用于 maxSaveCount）
int countSavedBmpUnderRoot(const QDir &root)
{
    int total = root.entryList(QStringList() << QStringLiteral("Pic*.bmp"), QDir::Files).size();
    const QStringList subs = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &name : subs)
    {
        if (!name.startsWith(QStringLiteral("CAMERA"), Qt::CaseInsensitive))
            continue;
        total += QDir(root.filePath(name)).entryList(QStringList() << QStringLiteral("Pic*.bmp"), QDir::Files).size();
    }
    return total;
}
}

// 默认 Images：与 exe 同级的上上级（Debug 下为项目根/Images）
QString SavePathHelper::defaultRootPath()
{
    QDir d(QCoreApplication::applicationDirPath());
    d.cdUp();
    d.cdUp();
    return d.absoluteFilePath(QStringLiteral("Images"));
}

void SavePathHelper::resetSession()
{
    m_picIndex = 1;
    m_totalSaved = 0;
    m_picsInCurrentFolder = 0;
    m_cameraFolderIndex = 1;
    m_currentFolder.clear();
    m_folderStartTime = QDateTime();
}

// 根据磁盘已有文件恢复 Pic/CAMERA 计数，避免阶段存图覆盖 Pic001
void SavePathHelper::resumeFromDisk()
{
    if (m_rootPath.isEmpty())
        return;

    QDir root(m_rootPath);
    if (!root.exists())
    {
        resetSession();
        return;
    }

    m_totalSaved = countSavedBmpUnderRoot(root);

    if (m_mode == SaveFolderMode::SingleFolder)
    {
        m_currentFolder = m_rootPath;
        const int maxPic = maxPicInDir(root);
        m_picIndex = maxPic + 1;
        m_picsInCurrentFolder = maxPic;
        m_cameraFolderIndex = 1;
        m_folderStartTime = QDateTime();
        return;
    }

    int maxCameraIndex = 0;
    QString latestCameraDir;
    const QStringList subs = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &name : subs)
    {
        if (name.length() < 7 || !name.startsWith(QStringLiteral("CAMERA"), Qt::CaseInsensitive))
            continue;
        bool ok = false;
        const int idx = name.mid(6).toInt(&ok);
        if (!ok || idx <= 0)
            continue;
        if (idx >= maxCameraIndex)
        {
            maxCameraIndex = idx;
            latestCameraDir = root.filePath(name);
        }
    }

    if (maxCameraIndex <= 0 || latestCameraDir.isEmpty())
    {
        resetSession();
        return;
    }

    const int maxPic = maxPicInDir(QDir(latestCameraDir));
    const bool folderFull = (m_mode == SaveFolderMode::ByCount && maxPic >= m_picsPerFolder);
    if (folderFull)
    {
        m_currentFolder.clear();
        m_picIndex = 1;
        m_picsInCurrentFolder = 0;
        m_cameraFolderIndex = maxCameraIndex + 1;
        m_folderStartTime = QDateTime();
        return;
    }

    m_currentFolder = latestCameraDir;
    m_picIndex = maxPic + 1;
    m_picsInCurrentFolder = maxPic;
    m_cameraFolderIndex = maxCameraIndex + 1;
    m_folderStartTime = QDateTime::currentDateTime();
}

// 仅当内存序号落后于磁盘时上调，避免阶段开始前回扫磁盘把 Pic 序号改小导致覆盖
void SavePathHelper::syncIndexWithDisk()
{
    if (m_rootPath.isEmpty())
        return;

    QDir root(m_rootPath);
    if (!root.exists())
        return;

    const int diskTotal = countSavedBmpUnderRoot(root);
    if (diskTotal > m_totalSaved)
        m_totalSaved = diskTotal;

    if (m_mode == SaveFolderMode::SingleFolder)
    {
        m_currentFolder = m_rootPath;
        const int diskMax = maxPicInDir(root);
        if (m_picIndex <= diskMax)
            m_picIndex = diskMax + 1;
        if (m_picsInCurrentFolder < diskMax)
            m_picsInCurrentFolder = diskMax;
        return;
    }

    int maxCameraIndex = 0;
    QString latestCameraDir;
    const QStringList subs = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &name : subs)
    {
        if (name.length() < 7 || !name.startsWith(QStringLiteral("CAMERA"), Qt::CaseInsensitive))
            continue;
        bool ok = false;
        const int idx = name.mid(6).toInt(&ok);
        if (!ok || idx <= 0)
            continue;
        if (idx >= maxCameraIndex)
        {
            maxCameraIndex = idx;
            latestCameraDir = root.filePath(name);
        }
    }

    if (maxCameraIndex <= 0 || latestCameraDir.isEmpty())
        return;

    const int diskMaxPic = maxPicInDir(QDir(latestCameraDir));
    if (m_currentFolder.isEmpty())
        m_currentFolder = latestCameraDir;
    if (m_picIndex <= diskMaxPic)
        m_picIndex = diskMaxPic + 1;
    if (m_picsInCurrentFolder < diskMaxPic)
        m_picsInCurrentFolder = diskMaxPic;
    if (m_cameraFolderIndex <= maxCameraIndex)
        m_cameraFolderIndex = maxCameraIndex + 1;
}

// 按张数或按时间判断是否需要新建 CAMERA 子目录
bool SavePathHelper::needNewCameraFolder() const
{
    if (m_mode == SaveFolderMode::SingleFolder)
        return false;
    if (m_currentFolder.isEmpty())
        return true;
    if (m_mode == SaveFolderMode::ByCount)
        return m_picsInCurrentFolder >= m_picsPerFolder;
    if (!m_folderStartTime.isValid())
        return true;
    return m_folderStartTime.secsTo(QDateTime::currentDateTime()) >= m_secondsPerFolder;
}

bool SavePathHelper::createNewCameraFolder()
{
    m_currentFolder = QDir(m_rootPath).filePath(
        QStringLiteral("CAMERA%1").arg(m_cameraFolderIndex++, 3, 10, QChar('0')));
    if (!ensureFolderExists(m_currentFolder))
    {
        m_currentFolder.clear();
        return false;
    }
    m_picIndex = 1;
    m_picsInCurrentFolder = 0;
    m_folderStartTime = QDateTime::currentDateTime();
    return true;
}

QString SavePathHelper::activeFolderPath()
{
    if (m_rootPath.isEmpty() || !ensureFolderExists(m_rootPath))
        return QString();

    if (m_mode == SaveFolderMode::SingleFolder)
        return m_currentFolder = m_rootPath;

    if (m_currentFolder.isEmpty() || needNewCameraFolder())
    {
        if (!createNewCameraFolder())
            return QString();
    }
    return m_currentFolder;
}

QString SavePathHelper::nextFilePath(bool *ok)
{
    if (ok)
        *ok = false;
    if (m_rootPath.isEmpty() || isSaveLimitReached())
        return QString();

    const QString dir = activeFolderPath();
    if (dir.isEmpty())
        return QString();

    const QString path = QDir(dir).filePath(
        QStringLiteral("Pic%1.bmp").arg(m_picIndex++, 3, 10, QChar('0')));
    if (ok)
        *ok = true;
    return path;
}
