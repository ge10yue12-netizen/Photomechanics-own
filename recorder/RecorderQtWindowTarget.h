#pragma once

#include "include/IRecorderWindowTarget.h"
#include "include/VisualFrameCache.h"
#include "RecorderQtVisualSource.h"

#include <QPointer>
#include <QWidget>

// Qt 集成：绑定 QWidget，经 VisualFrameCache 供预览/录制零阻塞读取。
class QWidgetRecorderTarget final : public recorder::IRecorderWindowTarget
{
public:
    explicit QWidgetRecorderTarget(QWidget *widget = nullptr);

    void setWidget(QWidget *widget);
    QWidget *widget() const;

    std::uintptr_t windowHandle() const override;
    bool isAvailable() const override;
    recorder::VisualFrameCache *visualCache() override;
    void refreshVisualCache(bool force = false) override;
    void setVisualConsumerFlags(int flags) override;

private:
    void bindVisualSource(QWidget *widget);

    QPointer<QWidget> m_widget;
    recorder::VisualFrameCache m_cache;
};
