#pragma once

#include <QImage>
#include <QString>

/**
 * @brief 将文本编码为 QR 码 QImage（Nayuki qrcodegen）。
 */
class QrCodeHelper
{
public:
    /**
     * @param text   通常为 mobile URL
     * @param scale  模块像素倍率
     * @param border 静区模块宽度
     */
    static QImage generateQrImage(const QString &text, int scale = 8, int border = 4);
};
