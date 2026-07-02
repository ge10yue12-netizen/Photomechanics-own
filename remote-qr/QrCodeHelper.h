#pragma once

#include <QImage>
#include <QString>

// * QR 码图像生成（Nayuki qrcodegen）。
class QrCodeHelper
{
public:
    // 将 text 编码为 QR 码 QImage；失败时返回空 QImage。
    static QImage generateQrImage(const QString &text, int scale = 8, int border = 4);
};
