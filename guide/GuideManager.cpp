#include "GuideManager.h"

#include <QCoreApplication>
#include <QEvent>
#include <QSettings>
#include <QtGlobal>

namespace
{
constexpr int kConditionPollMs = 350;    // canProceed 轮询周期（毫秒）
constexpr int kBubbleMinWidth = 280;     // 气泡最小宽度
constexpr int kBubbleMinHeight = 120;    // 气泡最小高度
constexpr int kBubbleEdgeMargin = 12;    // 气泡距 root 边缘最小留白
}

// 构造函数：保存遮罩父控件 root，并启动条件轮询定时器。
GuideManager::GuideManager(QWidget *root, QObject *parent)
    : QObject(parent), m_root(root)
{
    m_conditionTimer.setInterval(kConditionPollMs);
    connect(&m_conditionTimer, &QTimer::timeout, this, &GuideManager::updateStepInteractionState);
}

// 析构函数：若引导仍在运行则调用 stop 释放遮罩与气泡。
GuideManager::~GuideManager()
{
    stop();
}

// 设置产品标识 productId，写入 QSettings 完成键时作为第一段路径。
void GuideManager::setProductId(const QString &productId)
{
    m_productId = productId.trimmed();
}

// 设置引导组标识 guideId，同一产品下可配置多组引导。
void GuideManager::setGuideId(const QString &guideId)
{
    m_guideId = guideId.trimmed();
}

// 设置引导版本号；步骤内容变更时应递增，使用户重新看到引导。
void GuideManager::setVersion(int version)
{
    m_version = qMax(1, version);
}

// 设置遮罩与气泡主题；引导运行中调用时同步刷新已创建的控件。
void GuideManager::setTheme(const GuideTheme &theme)
{
    m_theme = theme;
    if (m_overlay)
        m_overlay->setTheme(m_theme);
    if (m_bubble)
        m_bubble->setTheme(m_theme);
}

// 在步骤列表末尾追加一步。
void GuideManager::addStep(const GuideStep &step)
{
    m_steps.push_back(step);
}

// 用 steps 整体替换当前步骤列表。
void GuideManager::setSteps(const QVector<GuideStep> &steps)
{
    m_steps = steps;
}

// 清空步骤列表；引导运行中不应调用。
void GuideManager::clearSteps()
{
    m_steps.clear();
}

// 查询当前引导组是否已标记为完成；guideId 为空时返回 false。
bool GuideManager::isCompleted() const
{
    if (m_guideId.isEmpty())
        return false;
    return QSettings().value(settingsKey(), false).toBool();
}

// 删除 QSettings 中的完成标记，使用户再次看到该引导组。
void GuideManager::resetCompletion()
{
    if (!m_guideId.isEmpty())
        QSettings().remove(settingsKey());
}

// 若未完成则启动引导；已完成则直接返回。
void GuideManager::startIfNeeded()
{
    startInternal(false);
}

// 忽略完成标记，强制启动引导。
void GuideManager::start()
{
    startInternal(true);
}

// 释放遮罩、气泡与事件过滤器，重置步骤下标与运行标志；不发射信号。
void GuideManager::teardownUi()
{
    m_conditionTimer.stop();
    removeRuntimeFilters();
    watchTarget(nullptr);

    if (m_index >= 0 && m_index < m_steps.size() && m_steps[m_index].afterLeave)
        m_steps[m_index].afterLeave();

    if (m_bubble)
    {
        m_bubble->deleteLater();
        m_bubble = nullptr;
    }
    if (m_overlay)
    {
        m_overlay->deleteLater();
        m_overlay = nullptr;
    }

    m_index = -1;
    m_running = false;
}

// 中止引导：不写完成标记，发射 stopped(false)。
void GuideManager::stop()
{
    if (!m_running && !m_overlay && !m_bubble)
        return;

    const bool wasRunning = m_running;
    teardownUi();
    if (wasRunning)
        emit stopped(false);
}

// 跳过引导：写入完成标记，发射 skipped。
void GuideManager::skip()
{
    if (!m_running)
        return;

    if (!m_guideId.isEmpty())
        QSettings().setValue(settingsKey(), true);

    teardownUi();
    emit skipped();
}

// 用户点击末步「完成」：写入完成标记，发射 stopped(true)。
void GuideManager::complete()
{
    if (!m_running)
        return;

    if (!m_guideId.isEmpty())
        QSettings().setValue(settingsKey(), true);

    teardownUi();
    emit stopped(true);
}

// 业务侧条件达成时调用；仅当当前步 conditionId 匹配时刷新「下一步」按钮状态。
void GuideManager::notifyCondition(const QString &conditionId)
{
    if (!m_running || m_index < 0 || m_index >= m_steps.size())
        return;

    const GuideStep &step = m_steps[m_index];
    if (step.requireCanProceed && !step.conditionId.isEmpty() && step.conditionId == conditionId)
        updateStepInteractionState();
}

// 监听 root 与顶层窗口的尺寸、位置变化，触发遮罩与气泡重定位。
bool GuideManager::eventFilter(QObject *watched, QEvent *event)
{
    if (!m_running)
        return QObject::eventFilter(watched, event);

    if (watched == m_root || watched == m_rootWindow)
    {
        if (event->type() == QEvent::Resize || event->type() == QEvent::Move ||
            event->type() == QEvent::WindowStateChange || event->type() == QEvent::Show)
        {
            QTimer::singleShot(0, this, &GuideManager::updateOverlayAndBubble);
        }
    }

    return QObject::eventFilter(watched, event);
}

// 内部启动入口：创建遮罩与气泡，安装过滤器，显示第一步。
// force 为 false 且已完成时直接返回。
void GuideManager::startInternal(bool force)
{
    if (!m_root || m_steps.isEmpty() || m_running)
        return;
    if (!force && isCompleted())
        return;

    m_rootWindow = m_root->window();

    m_overlay = new GuideOverlay(m_root);
    m_overlay->setTheme(m_theme);
    m_overlay->refreshGeometry();
    m_overlay->show();

    m_bubble = new GuideBubbleWidget(m_root);
    m_bubble->setTheme(m_theme);
    m_bubble->hide();

    connect(m_bubble, &GuideBubbleWidget::previousRequested, this, &GuideManager::goPrevious);
    connect(m_bubble, &GuideBubbleWidget::nextRequested, this, &GuideManager::goNext);
    connect(m_bubble, &GuideBubbleWidget::skipRequested, this, &GuideManager::skip);
    connect(m_bubble, &GuideBubbleWidget::finishRequested, this, &GuideManager::complete);

    m_running = true;
    installRuntimeFilters();
    emit started();
    showStep(0);
}

// 切换到指定下标步骤：执行上一步 afterLeave、本步 beforeShow，再延迟刷新 UI。
void GuideManager::showStep(int index)
{
    if (!m_running || index < 0 || index >= m_steps.size())
        return;

    if (m_index >= 0 && m_index < m_steps.size() && m_steps[m_index].afterLeave)
        m_steps[m_index].afterLeave();

    m_index = index;
    if (m_steps[m_index].beforeShow)
        m_steps[m_index].beforeShow();

    QTimer::singleShot(0, this, &GuideManager::applyCurrentStep);
}

// 根据当前步骤刷新高亮目标、气泡文案、导航按钮与条件轮询状态。
void GuideManager::applyCurrentStep()
{
    if (!m_running || m_index < 0 || m_index >= m_steps.size() || !m_overlay || !m_bubble)
        return;

    const GuideStep &step = m_steps[m_index];
    QWidget *target = step.resolveTarget(m_root);
    watchTarget(target);

    m_overlay->setTarget(target, step.margin);
    m_bubble->setMaximumContentWidth(step.maxBubbleWidth);
    m_bubble->setContent(step.title, step.description, m_index, m_steps.size());
    m_bubble->setActionHint(buildActionHint(step));
    m_bubble->setNavigationState(m_index == 0, m_index == m_steps.size() - 1, step.allowPrevious, step.allowSkip);

    updateBubblePosition();
    m_overlay->raise();
    m_bubble->show();
    m_bubble->raise();

    m_conditionTimer.stop();
    updateStepInteractionState();
    if (step.requireCanProceed && step.canProceed)
        m_conditionTimer.start();

    emit stepChanged(step.id, m_index, m_steps.size());
}

// 进入下一步；已是最后一步时调用 complete。
void GuideManager::goNext()
{
    if (!m_running)
        return;
    if (m_index + 1 >= m_steps.size())
        complete();
    else
        showStep(m_index + 1);
}

// 返回上一步；第一步时无操作。
void GuideManager::goPrevious()
{
    if (m_running && m_index > 0)
        showStep(m_index - 1);
}

// 窗口尺寸变化后同步遮罩铺满父控件并重算气泡位置。
void GuideManager::updateOverlayAndBubble()
{
    if (!m_running || !m_overlay)
        return;
    m_overlay->refreshGeometry();
    updateBubblePosition();
    if (m_bubble)
        m_bubble->raise();
}

// 计算气泡位置：优先靠高亮区域右侧，否则尝试左、下、上，最后居中并限制在 root 内。
void GuideManager::updateBubblePosition()
{
    if (!m_root || !m_overlay || !m_bubble)
        return;

    const QRect rootRect = m_root->rect();
    const QRect targetRect = m_overlay->targetRect();
    const QSize bubbleSize = m_bubble->sizeHint().expandedTo(QSize(kBubbleMinWidth, kBubbleMinHeight));
    const int spacing = m_theme.bubbleSpacing;

    auto bounded = [&](QPoint pos) {
        const int maxX = qMax(kBubbleEdgeMargin, rootRect.width() - bubbleSize.width() - kBubbleEdgeMargin);
        const int maxY = qMax(kBubbleEdgeMargin, rootRect.height() - bubbleSize.height() - kBubbleEdgeMargin);
        pos.setX(qBound(kBubbleEdgeMargin, pos.x(), maxX));
        pos.setY(qBound(kBubbleEdgeMargin, pos.y(), maxY));
        return pos;
    };

    QPoint pos((rootRect.width() - bubbleSize.width()) / 2, (rootRect.height() - bubbleSize.height()) / 2);
    if (targetRect.isValid())
    {
        const QVector<QPoint> candidates = {
            QPoint(targetRect.right() + spacing, targetRect.top()),
            QPoint(targetRect.left() - bubbleSize.width() - spacing, targetRect.top()),
            QPoint(targetRect.left(), targetRect.bottom() + spacing),
            QPoint(targetRect.left(), targetRect.top() - bubbleSize.height() - spacing)};

        for (const QPoint &candidate : candidates)
        {
            if (rootRect.contains(QRect(candidate, bubbleSize)))
            {
                pos = candidate;
                break;
            }
        }
    }

    m_bubble->resize(bubbleSize);
    m_bubble->move(bounded(pos));
}

// 在 root 与顶层窗口上安装事件过滤器，用于响应 resize/move。
void GuideManager::installRuntimeFilters()
{
    if (m_filtersInstalled)
        return;
    if (m_root)
        m_root->installEventFilter(this);
    if (m_rootWindow && m_rootWindow != m_root)
        m_rootWindow->installEventFilter(this);
    m_filtersInstalled = true;
}

// 移除 installRuntimeFilters 安装的过滤器。
void GuideManager::removeRuntimeFilters()
{
    if (!m_filtersInstalled)
        return;
    if (m_root)
        m_root->removeEventFilter(this);
    if (m_rootWindow && m_rootWindow != m_root)
        m_rootWindow->removeEventFilter(this);
    m_filtersInstalled = false;
}

// 监听高亮目标控件销毁；销毁后取消洞区高亮并刷新布局。
void GuideManager::watchTarget(QWidget *target)
{
    if (m_targetDestroyedConnection)
    {
        disconnect(m_targetDestroyedConnection);
        m_targetDestroyedConnection = QMetaObject::Connection();
    }

    m_watchedTarget = target;
    if (m_watchedTarget)
    {
        m_targetDestroyedConnection = connect(m_watchedTarget, &QObject::destroyed, this, [this]() {
            m_watchedTarget = nullptr;
            QTimer::singleShot(0, this, &GuideManager::updateOverlayAndBubble);
        });
    }
}

// 根据当前步 canProceed 结果设置「下一步」是否可点；最后一步的「完成」始终可点。
void GuideManager::updateStepInteractionState()
{
    if (!m_running || !m_bubble || m_index < 0 || m_index >= m_steps.size())
        return;

    const GuideStep &step = m_steps[m_index];
    const bool isLast = m_index == m_steps.size() - 1;
    bool ready = true;
    if (step.requireCanProceed && step.canProceed)
        ready = step.canProceed();

    m_bubble->setNextEnabled(isLast || ready);
}

// 生成操作提示文案；步骤未配置 actionHint 时使用默认句式。
QString GuideManager::buildActionHint(const GuideStep &step) const
{
    if (!step.actionHint.isEmpty())
        return step.actionHint;
    if (step.requireCanProceed)
        return QStringLiteral("您可继续操作；完成本步建议内容后点击「下一步」。");
    return QStringLiteral("您可继续操作；准备好后点击「下一步」。");
}

// 生成 QSettings 完成状态键：GuideKit/<productId>/<guideId>/v<version>/Completed
QString GuideManager::settingsKey() const
{
    QString product = m_productId;
    if (product.isEmpty())
        product = QCoreApplication::applicationName();
    if (product.isEmpty())
        product = QStringLiteral("DefaultProduct");

    return QStringLiteral("GuideKit/%1/%2/v%3/Completed")
        .arg(product, m_guideId)
        .arg(m_version);
}
