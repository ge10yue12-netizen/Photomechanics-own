// SavePathHelper.cpp — 存图路径与 CAMERA/Pic 命名规则

#include "SavePathHelper.h"
#include <QCoreApplication>
#include <QDir>

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
