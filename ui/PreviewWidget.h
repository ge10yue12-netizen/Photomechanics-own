#pragma once

#include "../recorder/RecorderQtVisualSource.h"

#include <QImage>
#include <QPointF>
#include <QWidget>

// PreviewWidget：相机预览区，支持滚轮缩放、拖拽平移、双击切换适应/1:1、悬停像素读数
class PreviewWidget : public QWidget, public IRecorderQtVisualSource
{
    Q_OBJECT

public:
    explicit PreviewWidget(QWidget *parent = nullptr);

    void setImage(const QImage &image);
    void clearImage();
    void setPlaceholderText(const QString &text);

    QImage recorderVisualSnapshot() const override;
    quint64 visualRevision() const { return m_visualRevision; }

signals:
    // 预览视觉内容变化（图像/视图/占位）；录屏按需刷新缓存。
    void visualRevisionChanged();
    // 鼠标位于有效图像像素上时上报原图坐标与灰度值（非图像区域时 valid=false）
    void pixelInfoChanged(int x, int y, int gray, bool valid);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    enum class ViewMode
    {
        Fit,       // 适应窗口
        OneToOne,  // 1 图像像素对应 1 屏幕像素
        Manual     // 用户滚轮缩放或拖拽后的自由视图
    };

    double fitScale() const;
    void applyFitView();
    void applyOneToOneView();
    QPointF imageTopLeft() const;
    QSizeF displayedImageSize() const;
    bool mapToImage(const QPoint &widgetPos, int *outX, int *outY) const;
    int sampleGrayAt(int x, int y) const;
    void emitPixelInfoAt(const QPoint &widgetPos);
    void clampScale();
    void bumpVisualRevision();

    QImage m_image;
    bool m_hasImage = false;
    QString m_placeholder;
    ViewMode m_viewMode = ViewMode::Fit;
    double m_viewScale = 1.0;   // 屏幕像素 / 图像像素
    QPointF m_viewTopLeft;      // 图像 (0,0) 在控件坐标系中的位置
    bool m_dragging = false;
    QPoint m_lastDragPos;
    quint64 m_visualRevision = 0;
};
