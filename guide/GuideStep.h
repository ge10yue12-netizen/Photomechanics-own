#pragma once

#include <QString>
#include <QWidget>

#include <functional>

// 单步引导配置。仅描述文案与高亮参考，不包含 UI 生命周期。
// 界面可操作性由宿主工程在 beforeShow / afterLeave 中实现。
struct GuideStep
{
    QString id;              // 步骤唯一标识，用于 stepChanged 信号
    QString title;           // 气泡标题
    QString description;     // 气泡正文说明
    QString actionHint;      // 操作提示正文；为空时由 GuideManager 生成默认文案

    QString targetObjectName;                   // 高亮控件 objectName
    std::function<QWidget *()> targetGetter;    // 动态解析高亮控件；优先于 targetObjectName

    std::function<void()> beforeShow;  // 进入本步前回调（切页、滚屏等）
    std::function<void()> afterLeave;  // 离开本步后回调（恢复 enable 等）

    QString conditionId;                 // 与 GuideManager::notifyCondition 配对
    std::function<bool()> canProceed;    // 是否满足进入下一步的前置条件
    bool requireCanProceed = false;      // 为 true 时，条件未满足则禁用「下一步」

    bool allowSkip = true;      // 是否显示「跳过」
    bool allowPrevious = true;  // 是否显示「上一步」
    int margin = 8;             // 高亮区域相对目标控件的外扩像素
    int maxBubbleWidth = 380;   // 气泡最大宽度

    // 查找本步高亮控件：优先调用 targetGetter，否则在 root 子树中按 objectName 查找
    QWidget *resolveTarget(QWidget *root) const
    {
        if (targetGetter)
            return targetGetter();
        if (!targetObjectName.isEmpty() && root)
            return root->findChild<QWidget *>(targetObjectName);
        return nullptr;
    }
};
