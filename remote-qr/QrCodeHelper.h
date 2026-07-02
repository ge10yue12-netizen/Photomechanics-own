#pragma once

#include <QImage>
#include <QString>

// 将文本编码为 QR 码 QImage（Nayuki qrcodegen）
class QrCodeHelper
{
public:
    // 生成 QR 码图像；text 为空或 scale 非法时返回空 QImage
    static QImage generateQrImage(const QString &text, int scale = 8, int border = 4);
};
