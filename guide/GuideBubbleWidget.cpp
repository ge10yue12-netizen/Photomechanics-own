#include "GuideBubbleWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtGlobal>

// 构造函数：创建标题、说明、操作提示、进度标签与导航按钮，并连接点击信号。
GuideBubbleWidget::GuideBubbleWidget(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("GuideBubbleWidget"));
    setFrameShape(QFrame::NoFrame);
    setWindowFlags(Qt::Widget);

    m_title = new QLabel(this);
    m_title->setWordWrap(true);

    m_description = new QLabel(this);
    m_description->setWordWrap(true);

    m_actionHint = new QLabel(this);
    m_actionHint->setWordWrap(true);

    m_progress = new QLabel(this);

    m_previous = new QPushButton(QStringLiteral("上一步"), this);
    m_next = new QPushButton(QStringLiteral("下一步"), this);
    m_skip = new QPushButton(QStringLiteral("跳过"), this);
    m_skip->setToolTip(QStringLiteral("跳过本组引导"));

    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->addWidget(m_skip);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_previous);
    buttonLayout->addWidget(m_next);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->addWidget(m_title);
    mainLayout->addWidget(m_description);
    mainLayout->addWidget(m_actionHint);
    mainLayout->addWidget(m_progress);
    mainLayout->addLayout(buttonLayout);

    connect(m_previous, &QPushButton::clicked, this, &GuideBubbleWidget::previousRequested);
    connect(m_skip, &QPushButton::clicked, this, &GuideBubbleWidget::skipRequested);
    connect(m_next, &QPushButton::clicked, this, [this]() {
        if (m_next->text() == QStringLiteral("完成"))
            emit finishRequested();
        else
            emit nextRequested();
    });

    applyTheme();
}

// 保存主题并应用到子控件样式。
void GuideBubbleWidget::setTheme(const GuideTheme &theme)
{
    m_theme = theme;
    applyTheme();
}

// 设置标题、正文与进度文本（格式：当前步/总步数）。
void GuideBubbleWidget::setContent(const QString &title, const QString &description, int index, int total)
{
    m_title->setText(title);
    m_description->setText(description);
    m_progress->setText(QStringLiteral("%1 / %2").arg(index + 1).arg(total));
    adjustSize();
}

// 设置操作提示行；hint 为空时隐藏该行。
void GuideBubbleWidget::setActionHint(const QString &hint)
{
    const bool visible = !hint.trimmed().isEmpty();
    m_actionHint->setVisible(visible);
    m_actionHint->setText(visible ? QStringLiteral("操作提示：%1").arg(hint) : QString());
    adjustSize();
}

// 设置导航按钮可见性与文案：首步禁用「上一步」，末步将「下一步」改为「完成」。
void GuideBubbleWidget::setNavigationState(bool first, bool last, bool allowPrevious, bool allowSkip)
{
    m_previous->setVisible(allowPrevious);
    m_previous->setEnabled(allowPrevious && !first);
    m_skip->setVisible(allowSkip);
    m_next->setVisible(true);
    m_next->setText(last ? QStringLiteral("完成") : QStringLiteral("下一步"));
}

// 设置「下一步」或「完成」按钮是否可点击。
void GuideBubbleWidget::setNextEnabled(bool enabled)
{
    m_next->setEnabled(enabled);
}

// 限制气泡与内部标签的最大宽度，并重新计算 sizeHint。
void GuideBubbleWidget::setMaximumContentWidth(int width)
{
    const int safeWidth = qMax(220, width);
    const int contentWidth = qMax(120, safeWidth - 2 * m_theme.bubblePadding);
    setMaximumWidth(safeWidth);
    m_title->setMaximumWidth(contentWidth);
    m_description->setMaximumWidth(contentWidth);
    m_actionHint->setMaximumWidth(contentWidth);
    adjustSize();
}

// 将 m_theme 中的字体、颜色、边距应用到布局与各子控件。
void GuideBubbleWidget::applyTheme()
{
    auto *layout = qobject_cast<QVBoxLayout *>(this->layout());
    if (layout)
        layout->setContentsMargins(m_theme.bubblePadding, m_theme.bubblePadding, m_theme.bubblePadding, m_theme.bubblePadding);

    QFont titleFont = m_theme.titleFont;
    if (titleFont.pointSize() <= 0)
        titleFont.setPointSize(11);
    titleFont.setBold(true);

    QFont textFont = m_theme.textFont;
    if (textFont.pointSize() <= 0)
        textFont.setPointSize(9);

    m_title->setFont(titleFont);
    m_description->setFont(textFont);
    m_actionHint->setFont(textFont);
    m_progress->setFont(textFont);
    m_title->setStyleSheet(QStringLiteral("color: rgb(%1,%2,%3);")
                               .arg(m_theme.titleColor.red())
                               .arg(m_theme.titleColor.green())
                               .arg(m_theme.titleColor.blue()));
    m_actionHint->setStyleSheet(QStringLiteral("color: rgb(%1,%2,%3);")
                                    .arg(m_theme.hintColor.red())
                                    .arg(m_theme.hintColor.green())
                                    .arg(m_theme.hintColor.blue()));

    setStyleSheet(QStringLiteral(
                      "#GuideBubbleWidget { background: rgb(%1,%2,%3); border: 1px solid rgb(%4,%5,%6); border-radius: %7px; }"
                      "QLabel { color: rgb(%8,%9,%10); }")
                      .arg(m_theme.bubbleBackground.red())
                      .arg(m_theme.bubbleBackground.green())
                      .arg(m_theme.bubbleBackground.blue())
                      .arg(m_theme.bubbleBorder.red())
                      .arg(m_theme.bubbleBorder.green())
                      .arg(m_theme.bubbleBorder.blue())
                      .arg(m_theme.bubbleRadius)
                      .arg(m_theme.textColor.red())
                      .arg(m_theme.textColor.green())
                      .arg(m_theme.textColor.blue()));
}
