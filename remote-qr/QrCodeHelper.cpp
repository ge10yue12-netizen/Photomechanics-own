#include "QrCodeHelper.h"

#include "thirdparty/qrcodegen.hpp"

#include <QPainter>
#include <vector>

using qrcodegen::QrCode;
using qrcodegen::QrSegment;

// 将文本编码为 QR 码 QImage；参数非法或编码失败时返回空图像。
QImage QrCodeHelper::generateQrImage(const QString &text, int scale, int border)
{
    if (text.isEmpty() || scale < 1 || border < 0)
        return QImage();

    const QByteArray utf8 = text.toUtf8();
    const std::vector<QrSegment> segments = QrSegment::makeSegments(utf8.constData());
    const QrCode qr = QrCode::encodeSegments(segments, QrCode::Ecc::MEDIUM);

    const int dim = qr.getSize();
    const int imgSize = (dim + border * 2) * scale;
    QImage image(imgSize, imgSize, QImage::Format_RGB32);
    image.fill(Qt::white);

    QPainter painter(&image);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);
    for (int y = 0; y < dim; ++y)
    {
        for (int x = 0; x < dim; ++x)
        {
            if (qr.getModule(x, y))
            {
                painter.drawRect((x + border) * scale, (y + border) * scale, scale, scale);
            }
        }
    }
    return image;
}
