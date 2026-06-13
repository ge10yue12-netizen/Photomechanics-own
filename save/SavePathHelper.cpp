// SavePathHelper.cpp：存图路径与 CAMERA/Pic 命名规则

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

// 在 root 下查找编号最大的 CAMERA 子目录；找到时写入 outIndex/outDir 并返回 true
bool findLatestCameraDir(const QDir &root, int &outIndex, QString &outDir)
{
    outIndex = 0;
    outDir.clear();
    const QStringList subs = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &name : subs)
    {
        if (name.length() < 7 || !name.startsWith(QStringLiteral("CAMERA"), Qt::CaseInsensitive))
            continue;
        bool ok = false;
        const int idx = name.mid(6).toInt(&ok);
        if (!ok || idx <= 0)
            continue;
        if (idx >= outIndex)
        {
            outIndex = idx;
            outDir = root.filePath(name);
        }
    }
    return outIndex > 0 && !outDir.isEmpty();
}
}

// 默认 Images 目录：可执行文件上两级（Debug 构建下为项目根/Images）
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

// 根据磁盘已有 Pic*.bmp 恢复 Pic/CAMERA 计数，避免覆盖已有序号
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
    if (!findLatestCameraDir(root, maxCameraIndex, latestCameraDir))
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

// 过滤阶段名中的 Windows 非法路径字符
QString SavePathHelper::sanitizeFolderName(const QString &name)
{
    QString s = name.trimmed();
    if (s.isEmpty())
        return QStringLiteral("Stage");

    QString out;
    out.reserve(s.size());
    for (const QChar c : s)
    {
        if (c == QLatin1Char('\\') || c == QLatin1Char('/') || c == QLatin1Char(':')
            || c == QLatin1Char('*') || c == QLatin1Char('?') || c == QLatin1Char('"')
            || c == QLatin1Char('<') || c == QLatin1Char('>') || c == QLatin1Char('|'))
            out += QLatin1Char('_');
        else
            out += c;
    }
    return out;
}

void SavePathHelper::beginStageCapture()
{
    m_stageCaptureActive = true;
    m_loopIndex = 0;
    m_stageName.clear();
    m_stagePicIndex = 1;
}

void SavePathHelper::endStageCapture()
{
    m_stageCaptureActive = false;
    m_loopIndex = 0;
    m_stageName.clear();
    m_stagePicIndex = 1;
}

// 每阶段/每轮切换独立子目录，Pic 序号在该目录内从 001 续接
void SavePathHelper::setStageContext(int loopIndex, const QString &stageName)
{
    m_loopIndex = loopIndex < 1 ? 1 : loopIndex;
    m_stageName = stageName.trimmed();
    m_stagePicIndex = 1;

    if (m_rootPath.isEmpty())
        return;

    const QString dir = stageFolderPath();
    if (QDir(dir).exists())
        m_stagePicIndex = maxPicInDir(QDir(dir)) + 1;
}

QString SavePathHelper::stageFolderPath() const
{
    const QString loopDir = QStringLiteral("Loop%1").arg(m_loopIndex, 3, 10, QChar('0'));
    return QDir(m_rootPath).filePath(loopDir + QLatin1Char('/') + sanitizeFolderName(m_stageName));
}

void SavePathHelper::onFileSaved()
{
    ++m_totalSaved;
    if (m_stageCaptureActive)
        ++m_stagePicIndex;
    else
    {
        ++m_picsInCurrentFolder;
        ++m_picIndex;
    }
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

    // 阶段采集：{root}/Loop001/阶段名/Pic001.bmp，每阶段独立目录、Pic 从 001 起
    if (m_stageCaptureActive)
    {
        if (m_loopIndex < 1 || m_stageName.isEmpty())
            return QString();

        const QString dir = stageFolderPath();
        if (!ensureFolderExists(dir))
            return QString();

        // 使用当前序号预分配路径；写盘成功后在 onFileSaved 递增，入队失败可重试同一路径
        const QString path = QDir(dir).filePath(
            QStringLiteral("Pic%1.bmp").arg(m_stagePicIndex, 3, 10, QChar('0')));
        if (ok)
            *ok = true;
        return path;
    }

    // 手动存图：使用 CAMERA 分目录或单目录逻辑
    const QString dir = activeFolderPath();
    if (dir.isEmpty())
        return QString();

    // 使用当前序号预分配路径；写盘成功后在 onFileSaved 递增
    const QString path = QDir(dir).filePath(
        QStringLiteral("Pic%1.bmp").arg(m_picIndex, 3, 10, QChar('0')));
    if (ok)
        *ok = true;
    return path;
}
