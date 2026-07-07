#pragma once

#include <QWidget>
#include <QImage>

// 录制预览（显示真实桌面抓帧缩略图）。
class RecorderPreviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RecorderPreviewWidget(QWidget *parent = nullptr);

    void setPreviewImage(const QImage &image);
    void clearPreview();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QImage m_image;
};
