#pragma once

#include "GuideBubbleWidget.h"
#include "GuideOverlay.h"
#include "GuideStep.h"

#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVector>

// 引导流程控制器：管理步骤切换、遮罩与气泡生命周期、QSettings 完成状态。
class GuideManager : public QObject
{
    Q_OBJECT

public:
    // 构造：root 为遮罩与气泡的父控件
    explicit GuideManager(QWidget *root, QObject *parent = nullptr);
    // 析构：若仍在运行则停止引导
    ~GuideManager() override;

    // 设置产品标识，用于 QSettings 键
    void setProductId(const QString &productId);
    // 设置引导组标识
    void setGuideId(const QString &guideId);
    // 设置引导版本号，步骤变更时应递增
    void setVersion(int version);
    // 设置遮罩与气泡主题
    void setTheme(const GuideTheme &theme);

    // 清空步骤列表
    void clearSteps();
    // 追加一步
    void addStep(const GuideStep &step);
    // 替换全部步骤
    void setSteps(const QVector<GuideStep> &steps);

    // 引导是否正在显示
    bool isRunning() const { return m_running; }
    // 当前引导组是否已标记完成
    bool isCompleted() const;
    // 清除完成标记
    void resetCompletion();

    // 未完成时启动；已完成则忽略
    void startIfNeeded();
    // 强制启动，忽略完成标记
    void start();
    // 中止，不写完成标记
    void stop();
    // 跳过，写完成标记
    void skip();
    // 正常完成，写完成标记
    void complete();
    // 业务条件达成时调用，刷新「下一步」按钮
    void notifyCondition(const QString &conditionId);

signals:
    // 引导开始
    void started();
    // 引导结束；completed 为 true 表示正常完成
    void stopped(bool completed);
    // 用户点击跳过
    void skipped();
    // 切换到新步骤
    void stepChanged(const QString &stepId, int index, int total);

protected:
    // 监听窗口 resize/move，刷新遮罩与气泡
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    // 内部启动；force 为 true 时忽略完成标记
    void startInternal(bool force);
    // 切换到第 index 步
    void showStep(int index);
    // 刷新当前步的遮罩、气泡与按钮状态
    void applyCurrentStep();
    // 下一步；末步时等价于 complete
    void goNext();
    // 上一步
    void goPrevious();
    // 销毁 UI 并重置运行状态
    void teardownUi();
    // 同步遮罩几何与气泡位置
    void updateOverlayAndBubble();
    // 根据高亮区域计算气泡坐标
    void updateBubblePosition();
    // 安装 root 与窗口事件过滤器
    void installRuntimeFilters();
    // 移除事件过滤器
    void removeRuntimeFilters();
    // 监听高亮目标销毁
    void watchTarget(QWidget *target);
    // 根据 canProceed 更新「下一步」可点状态
    void updateStepInteractionState();
    // 生成默认操作提示文案
    QString buildActionHint(const GuideStep &step) const;
    // 生成 QSettings 完成键
    QString settingsKey() const;

    QPointer<QWidget> m_root;              // 遮罩父控件
    QPointer<QWidget> m_rootWindow;          // 顶层窗口
    QPointer<QWidget> m_watchedTarget;       // 当前高亮目标
    GuideOverlay *m_overlay = nullptr;     // 遮罩层
    GuideBubbleWidget *m_bubble = nullptr; // 气泡
    QVector<GuideStep> m_steps;            // 步骤列表
    GuideTheme m_theme;
    QString m_productId;
    QString m_guideId;
    int m_version = 1;
    int m_index = -1;                      // 当前步骤下标，-1 表示未运行
    bool m_running = false;
    bool m_filtersInstalled = false;
    QTimer m_conditionTimer;               // 轮询 canProceed
    QMetaObject::Connection m_targetDestroyedConnection;
};
