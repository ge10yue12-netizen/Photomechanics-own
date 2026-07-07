# GuideKit 使用说明（新手版）

## 这是什么？

复制 `guide` 文件夹到你的 Qt 工程里，在主窗口加几段代码，软件**第一次打开**时会出现：

- 屏幕变暗（遮罩）
- 中间有个**气泡**写说明文字
- 某个控件外面有**蓝色高亮框**
- 底部有：**上一步 / 下一步 / 跳过**（最后一步是 **完成**）

用户点 **完成** 或 **跳过** 之后，下次打开**不再自动弹出**。

**你要写的**：每一步写什么字、高亮哪个按钮。  
**不用改的**：`guide` 文件夹里的库代码（遮罩、气泡怎么画，库已经写好）。

---

## 一共 4 件事

```text
① 把 guide 文件夹拷进工程
② Visual Studio 里加入 3 个 cpp、2 个头文件做 Moc
③ 主窗口 .h / .cpp 里粘贴下面「小 Demo」代码
④ 编译运行
```

小 Demo 跑通 = 移植成功。后面「完整 Demo」是可选进阶。

---

## ① 复制 guide 文件夹

把整个 `guide` 目录复制到你的工程根目录（和 `main.cpp` 放一起即可）。

---

## ② Visual Studio 设置

**加进工程编译的文件（3 个 cpp）：**

- `guide/GuideManager.cpp`
- `guide/GuideOverlay.cpp`
- `guide/GuideBubbleWidget.cpp`

**要做 Moc 的头文件（2 个）：**

- `guide/GuideManager.h`
- `guide/GuideBubbleWidget.h`

**项目属性里还要做 2 件事：**

1. C++ 标准选 **C++14** 或更高  
2. **C/C++ → 命令行 → 其他选项** 里填 **`/utf-8`**（Debug 和 Release 都要填，否则中文会编译报错）

---

## ③ 小 Demo：粘贴代码（2 步引导，最容易测）

下面代码假设你的主窗口类叫 `MainWindow`（如果你叫别的，全局替换即可）。

### 第 1 步：改 .h 文件

在文件最上面加：

```cpp
#include "guide/GuideKit.h"
```

在 `class MainWindow` 里面加：

```cpp
GuideManager *m_guide = nullptr;
bool m_guideStarted = false;

void setupGuide();
```

如果还没有 `showEvent`，加上：

```cpp
void showEvent(QShowEvent *event) override;
```

---

### 第 2 步：改 .cpp 文件

文件顶部加：

```cpp
#include <QShowEvent>
#include <QTimer>
```

**在构造函数最后加一行：**

```cpp
setupGuide();
```

**把下面整个函数复制到 .cpp 里：**

```cpp
void MainWindow::setupGuide()
{
    if (m_guide)
        return;

    // 创建引导管理器（两个 this 都传主窗口即可）
    m_guide = new GuideManager(this, this);

    // ↓↓↓ 只改这一行：换成你的产品名，随便起，英文即可 ↓↓↓
    m_guide->setProductId(QStringLiteral("MyApp"));

    m_guide->setGuideId(QStringLiteral("first_run"));
    m_guide->setVersion(1);
    m_guide->clearSteps();

    // ---------- 第 1 步引导 ----------
    GuideStep step1;
    step1.title = QStringLiteral("欢迎");
    step1.description = QStringLiteral("这是第 1 步。点「下一步」继续。");
    // 高亮哪里：centralWidget 是主窗口中间区域，任何 QMainWindow 都有，不用找按钮名
    step1.targetGetter = [this]() -> QWidget * {
        return centralWidget();
    };
    m_guide->addStep(step1);

    // ---------- 第 2 步引导（最后一步，按钮会自动变成「完成」）----------
    GuideStep step2;
    step2.title = QStringLiteral("结束");
    step2.description = QStringLiteral("点「完成」。下次启动不会再弹出。");
    step2.targetGetter = [this]() -> QWidget * {
        return centralWidget();
    };
    m_guide->addStep(step2);
}
```

**如果 .cpp 里还没有 showEvent，再复制下面这个函数：**

```cpp
void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);

    // 必须等窗口布局画完再启动，否则高亮框位置会错
    if (m_guide && !m_guideStarted)
    {
        m_guideStarted = true;
        QTimer::singleShot(0, m_guide, &GuideManager::startIfNeeded);
    }
}
```

> **注意**：如果你本来就有 `showEvent`，不要整段覆盖，只把 `if (m_guide && !m_guideStarted) { ... }` 那几行**合并进去**。

---

### 第 3 步：编译运行，对照下面检查

| 你做的操作 | 应该看到 |
|------------|----------|
| 第一次打开软件 | 屏幕变暗，出现气泡「欢迎」 |
| 点「下一步」 | 变成「结束」 |
| 点「完成」 | 引导消失 |
| 关掉软件再打开 | **不再**自动弹出引导 |

以上 4 条都对 = **小 Demo 成功**。

---

### 想高亮某个按钮怎么办？

把上面两处 `return centralWidget();` 改成，例如：

```cpp
return ui->openButton;   // openButton 换成你 Designer 里的按钮名
```

---

## ④ 以后怎么改引导？

### 加一步

在 `setupGuide()` 里，在最后一个 `addStep` **前面**再写一段：

```cpp
GuideStep stepX;
stepX.title = QStringLiteral("新步骤标题");
stepX.description = QStringLiteral("新步骤说明");
stepX.targetGetter = [this]() -> QWidget * {
    return ui->你的控件;
};
m_guide->addStep(stepX);
```

然后找到 `setVersion(1)`，数字 **加 1**（改成 2、3、4…）。  
**不改 version，用户看不到新文案。**

### 改文字

改对应步骤的 `title`、`description`，`setVersion` 数字 **加 1**。

### 删一步

删掉那一段 `GuideStep` 和后面的 `addStep`，`setVersion` **加 1**。

---

## 进阶：完整 Demo（小 Demo 跑通后再看）

这一节把库提供的功能**都写进示例**，你用不上的整段删掉即可。

### 额外功能有哪些？

| 功能 | 干什么 | 小 Demo 有没有 |
|------|--------|----------------|
| `setTheme` | 改颜色、字体 | 无 |
| 信号 connect | 引导开始/切步/结束时打日志 | 无 |
| 门控 | 必须先点某个按钮，「下一步」才亮 | 无 |
| `notifyCondition` | 按钮点完后通知引导 | 无 |
| `beforeShow` | 进入某步前滚屏、切页 | 无 |
| `actionHint` | 气泡里多一行蓝色提示 | 无 |

### 头文件再多加

```cpp
bool m_buttonClicked = false;
void onGuideButtonClicked();
```

### 用下面函数替换小 Demo 的 setupGuide

**还要在构造函数里加**（把 `ui->pushButton` 换成你界面上的真实按钮）：

```cpp
connect(ui->pushButton, &QPushButton::clicked, this, &MainWindow::onGuideButtonClicked);
```

```cpp
void MainWindow::setupGuide()
{
    if (m_guide)
        return;

    m_guide = new GuideManager(this, this);
    m_guide->setProductId(QStringLiteral("MyApp"));
    m_guide->setGuideId(QStringLiteral("full_demo"));
    m_guide->setVersion(1);

    // --- 主题（整段不需要就删掉）---
    GuideTheme theme;
    theme.maskColor = QColor(0, 0, 0, 155);
    theme.highlightColor = QColor(64, 158, 255);
    m_guide->setTheme(theme);

    // --- 信号（整段不需要就删掉）---
    connect(m_guide, &GuideManager::stepChanged, this,
            [](const QString &, int index, int total) {
        qDebug() << "当前第" << (index + 1) << "步，共" << total << "步";
    });

    m_guide->clearSteps();

    // 第 1 步：普通介绍
    GuideStep s1;
    s1.title = QStringLiteral("第 1 步");
    s1.description = QStringLiteral("直接点下一步即可。");
    s1.actionHint = QStringLiteral("");  // 留空用默认提示
    s1.targetGetter = [this]() -> QWidget * { return centralWidget(); };
    m_guide->addStep(s1);

    // 第 2 步：必须先点按钮（门控）
    GuideStep s2;
    s2.title = QStringLiteral("第 2 步");
    s2.description = QStringLiteral("请先点高亮的按钮。");
    s2.targetGetter = [this]() -> QWidget * { return ui->pushButton; };
    s2.requireCanProceed = true;
    s2.conditionId = QStringLiteral("btn_clicked");
    s2.canProceed = [this]() { return m_buttonClicked; };
    m_guide->addStep(s2);

    // 第 3 步：最后一步
    GuideStep s3;
    s3.title = QStringLiteral("第 3 步");
    s3.description = QStringLiteral("点完成。");
    s3.targetGetter = [this]() -> QWidget * { return centralWidget(); };
    m_guide->addStep(s3);
}

void MainWindow::onGuideButtonClicked()
{
    m_buttonClicked = true;
    if (m_guide)
        m_guide->notifyCondition(QStringLiteral("btn_clicked"));
}
```

门控原理就三句：

1. 步骤里写 `requireCanProceed = true`  
2. `conditionId` 和下面 `notifyCondition` 里的字符串**完全一样**  
3. 用户点按钮成功后调用 `notifyCondition`

`showEvent` 代码和小 Demo **相同**，不用改。

---

## 出错了怎么办？

| 报错或现象 | 原因 | 解决办法 |
|------------|------|----------|
| C4819、C2001 | 没加 utf-8 | 项目属性命令行加 `/utf-8` |
| 高亮框在左上角 | 启动太早 | 必须在 `showEvent` 里调 `startIfNeeded` |
| 改了文字没变化 | 没升版本 | `setVersion` 数字加 1 |
| 「下一步」一直灰色 | 门控没配对 | 检查 `conditionId` 和 `notifyCondition` 是否一字不差 |
| 编译说找不到 GuideKit.h | 路径不对 | 确认 `guide` 文件夹位置，或加包含目录 |

---

## 调试：想再看一次引导

任选一种：

```cpp
m_guide->setVersion(2);          // 把 version 改大，重新编译（推荐）
```

或临时：

```cpp
m_guide->resetCompletion();
m_guide->start();
```

---

## 词义表（查 API 用）

### GuideManager 常用方法

| 方法 | 一句话 |
|------|--------|
| `new GuideManager(this, this)` | 创建 |
| `setProductId` | 产品名，记完成状态用 |
| `setGuideId` | 这组引导的名字 |
| `setVersion` | 版本号，改步骤要加 1 |
| `clearSteps` / `addStep` | 清空 / 添加一步 |
| `setSteps` | 一次性设置多步（和 addStep 二选一） |
| `setTheme` | 改颜色字体 |
| `startIfNeeded` | 第一次未完成才显示 |
| `start` | 强制显示（调试用） |
| `notifyCondition` | 门控：操作完成后调用 |
| `resetCompletion` | 清除「已完成」记录 |

### GuideStep 常用字段

| 字段 | 一句话 |
|------|--------|
| `title` | 标题 |
| `description` | 正文 |
| `targetGetter` | 高亮哪个控件 |
| `actionHint` | 蓝色小字提示，可留空 |
| `beforeShow` | 进入这步之前干点什么 |
| `afterLeave` | 离开这步之后干点什么 |
| `requireCanProceed` | true = 必须满足条件才能点下一步 |
| `conditionId` | 条件名字，和 notifyCondition 配对 |
| `canProceed` | 返回 true 表示条件满足了 |

### 信号（可选 connect）

- `started` — 引导开始了  
- `stepChanged` — 换了一步  
- `skipped` — 用户点了跳过  
- `stopped` — 引导结束了  

---

## 记住 4 句话

1. 步骤代码写在你自己的主窗口 `.cpp`，**不要**写进 `guide` 文件夹。  
2. 第一次启动引导写在 **`showEvent`** 里，不要写在构造函数。  
3. 改了引导内容，**version 数字加 1**。  
4. 先跑通 **小 Demo**，再按需从 **完整 Demo** 里抄功能。
