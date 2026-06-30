# GuideKit — 移植说明

Qt 5.15+ Widgets 新手引导：**遮罩 + 高亮 + 气泡 + 步骤导航**。库源码 **C++14**，不拦截界面操作。

## 注释约定

库内注释遵循项目 `code-comments.mdc`：

- **每个 `.cpp` 函数实现**上方须有一行中文注释，说明用途、前置条件或副作用
- **头文件**中类、成员、对外方法须有简短说明
- 书面、客观；避免与标识符重复及口语化表述

## 1. 复制

将整个 `guide/` 目录复制到目标工程。

## 2. 加入工程（Visual Studio + Qt）

**编译**

- `GuideManager.cpp`
- `GuideOverlay.cpp`
- `GuideBubbleWidget.cpp`

**Moc**

- `GuideManager.h`
- `GuideBubbleWidget.h`

**头文件**

- `GuideKit.h`（对外唯一入口）
- `GuideStep.h`、`GuideTheme.h`、`GuideOverlay.h`

语言标准：`/std:c++14` 或更高（库内不使用 C++17 特性）。

## 3. 最小接入

```cpp
#include "guide/GuideKit.h"

// 构造函数
m_guide = new GuideManager(this, this);
m_guide->setProductId(QStringLiteral("MyApp"));
m_guide->setGuideId(QStringLiteral("intro"));
m_guide->setVersion(1);

GuideStep step;
step.id = QStringLiteral("step1");
step.title = QStringLiteral("标题");
step.description = QStringLiteral("说明");
step.actionHint = QStringLiteral("您可在此操作；完成后点下一步");
step.targetGetter = [this]() { return ui.myButton; };
m_guide->addStep(step);

// 首次 show 且布局完成后
m_guide->startIfNeeded();
```

## 4. GuideStep 字段

| 字段 | 说明 |
|------|------|
| `id` | 步骤 ID |
| `title` / `description` / `actionHint` | 文案；`actionHint` 空则用库内默认 |
| `targetObjectName` / `targetGetter` | 高亮参考（仅绘制） |
| `beforeShow` / `afterLeave` | 切 Tab、滚屏、enable 等（业务实现） |
| `conditionId` + `canProceed` + `requireCanProceed` | 可选：条件满足后「下一步」才可点 |
| `allowSkip` / `allowPrevious` | 导航按钮 |
| `margin` / `maxBubbleWidth` | 高亮边距、气泡宽度 |

## 5. GuideManager 接口

| 方法 | 说明 |
|------|------|
| `startIfNeeded()` | 未完成才显示 |
| `start()` | 强制显示 |
| `stop()` | 中止，不写完成标记 |
| `skip()` | 跳过，写完成标记 |
| `complete()` | 正常完成 |
| `notifyCondition(id)` | 刷新「下一步」可点状态 |
| `isCompleted()` / `resetCompletion()` | QSettings |

**信号**：`started`、`stopped(bool)`、`skipped`、`stepChanged`

**完成键**：`GuideKit/<productId>/<guideId>/v<version>/Completed`

改步骤内容时 **升 version**，用户会重新看到引导。

## 6. 行为契约

- 遮罩不接收鼠标；界面照常操作
- 不自动跳步；用户点「下一步」/「完成」才前进
- `requireCanProceed` 只灰「下一步」，不禁用其它控件
- 末步「完成」始终可点

## 7. 限制操作（业务侧）

GuideKit 不封界面。若需只让用户动局部：

```cpp
step.beforeShow = [this]() { ui.otherPanel->setEnabled(false); };
step.afterLeave = [this]() { ui.otherPanel->setEnabled(true); };
```

详细说明见 `docs/GUIDEKIT_DEVELOPMENT_GUIDE.md`。
