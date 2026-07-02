#include "MobileConfig.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSettings>

namespace
{

const QString kConfigRel = QStringLiteral("config/mobile.ini");

// 写入 error 并返回 false
bool fail(QString *error, const QString &msg)
{
    if (error)
        *error = msg;
    return false;
}

// 越界时回退 fallback
int clampInt(int value, int minVal, int maxVal, int fallback)
{
    if (value < minVal || value > maxVal)
        return fallback;
    return value;
}

// 写入 MobileConfig 默认值
void applyDefaults(MobileConfig &cfg)
{
    cfg.httpPort = 8080;
    cfg.tokenLifetimeSec = 600;
    cfg.previewMaxWidth = 480;
    cfg.previewMaxHeight = 360;
    cfg.previewJpegQuality = 55;
    cfg.previewPollMs = 120;
    cfg.statusPollMs = 800;
}

} // namespace

// 自 exe 目录向上最多 8 层查找 mobile.ini
QString MobileConfigHelper::configFilePath()
{
    QDir dir(QCoreApplication::applicationDirPath());
    for (int depth = 0; depth < 8; ++depth)
    {
        const QString path = dir.absoluteFilePath(kConfigRel);
        if (QFile::exists(path))
            return path;
        if (!dir.cdUp())
            break;
    }
    return QString();
}

// 解析 ini 并校验各字段范围
bool MobileConfigHelper::load(MobileConfig &cfg, QString *error)
{
    applyDefaults(cfg);

    const QString path = configFilePath();
    if (path.isEmpty())
        return true; // 无配置文件时使用默认值

    QSettings settings(path, QSettings::IniFormat);

    const int port = settings.value(QStringLiteral("mobile/port"), cfg.httpPort).toInt();
    if (port < 1 || port > 65535)
        return fail(error, QStringLiteral("mobile/port 无效（1-65535）"));

    cfg.httpPort = static_cast<quint16>(port);
    cfg.tokenLifetimeSec = clampInt(settings.value(QStringLiteral("mobile/token_lifetime_sec"),
                                                 cfg.tokenLifetimeSec).toInt(),
                                    60, 86400, cfg.tokenLifetimeSec);

    cfg.previewMaxWidth = clampInt(settings.value(QStringLiteral("preview/max_width"),
                                                   cfg.previewMaxWidth).toInt(),
                                   160, 1280, cfg.previewMaxWidth);
    cfg.previewMaxHeight = clampInt(settings.value(QStringLiteral("preview/max_height"),
                                                    cfg.previewMaxHeight).toInt(),
                                    120, 960, cfg.previewMaxHeight);
    cfg.previewJpegQuality = clampInt(settings.value(QStringLiteral("preview/jpeg_quality"),
                                                     cfg.previewJpegQuality).toInt(),
                                      30, 90, cfg.previewJpegQuality);
    cfg.previewPollMs = clampInt(settings.value(QStringLiteral("preview/poll_ms"),
                                                cfg.previewPollMs).toInt(),
                                   80, 2000, cfg.previewPollMs);
    cfg.statusPollMs = clampInt(settings.value(QStringLiteral("preview/status_poll_ms"),
                                                 cfg.statusPollMs).toInt(),
                                  300, 5000, cfg.statusPollMs);
    return true;
}
