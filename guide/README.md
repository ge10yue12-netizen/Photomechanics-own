# GuideKit 新手引导 — 开发说明（本目录）

> **读者**：要在本工程或移植工程中**自己改步骤、改文案、改高亮、改条件**的开发者。  
> **库位置**：`guide/`（可复制整目录到其他 Qt Widgets 工程）。  
> **本仓库业务样例**：`QtProject_1.cpp` → `setupStartupGuide()`、`populateBasicCaptureGuide()`。  
> **补充文档**：根目录 [`docs/GUIDEKIT_DEVELOPMENT_GUIDE.md`](../docs/GUIDEKIT_DEVELOPMENT_GUIDE.md)（契约与测试清单）。

---

## 1. 一分钟理解

GuideKit 做三件事：

1. **半透明遮罩** + **高亮洞**（指向某个控件）
2. **气泡**（标题 / 说明 / 操作提示 + 上一步 / 下一步 / 跳过 / 完成）
3. **完成记录**（QSettings，用户点「完成」或「跳过」后不再自动弹出）

**重要**：库**不拦截**鼠标，用户可以在引导进行时照常点界面；高亮只是「建议看这里」。  
若某步必须「先操作再下一步」，用 `requireCanProceed` + `canProceed` + `notifyCondition` 只**灰掉「下一步」**，不禁用其它控件。

```text
用户首次启动
    → showEvent 布局完成后 startIfNeeded()
    → 显示第 1 步气泡
    → 用户点「下一步」手动切步（不会自动跳）
    → 末步点「完成」→ 写 QSettings → 下次不再弹出
```

---

## 2. 目录与文件职责

```text
guide/
  GuideKit.h              ← 对外唯一 #include 入口
  GuideStep.h             ← 单步配置结构体（改步骤主要改这里填的字段）
  GuideTheme.h            ← 遮罩/高亮/气泡颜色字体
  GuideManager.h/.cpp     ← 流程：切步、完成记录、notifyCondition
  GuideOverlay.h/.cpp     ← 遮罩 + 高亮洞绘制（一般不用改）
  GuideBubbleWidget.h/.cpp← 气泡 UI（改按钮文案/布局才动这里）
  README.md               ← 本文件
```

| 你想改… | 改哪里 |
|--------|--------|
| 步骤顺序、文案、高亮控件 | 业务工程里 `addStep()`（见 §5） |
| 颜色、字体、圆角 | `GuideTheme` 或 `setTheme()`（§8） |
| 「下一步」何时可点 | `canProceed` + 业务里 `notifyCondition()`（§6） |
| 用户重新看到引导 | `setVersion()` 递增（§7） |
| 气泡按钮文字/布局 | `GuideBubbleWidget.cpp`（少见） |

---

## 3. 接入工程（Visual Studio + Qt 5.15）

### 3.1 复制与编译

1. 复制整个 `guide/` 到目标工程。
2. **ClCompile** 加入：
   - `guide/GuideManager.cpp`
   - `guide/GuideOverlay.cpp`
   - `guide/GuideBubbleWidget.cpp`
3. **Qt Moc** 加入：
   - `guide/GuideManager.h`
   - `guide/GuideBubbleWidget.h`
4. C++ 标准：**C++14 及以上**（库内未用 C++17 特性）。

本仓库已在 `QtProject_1.vcxproj` 中配置，可直接对照。

### 3.2 头文件

业务代码只需：

```cpp
#include "guide/GuideKit.h"
```

---

## 4. 本仓库接入点（改引导从这里下手）

| 位置 | 作用 |
|------|------|
| `QtProject_1.h` | 成员 `GuideManager *m_startupGuide` |
| `QtProject_1.cpp` 构造函数 | 调用 `setupStartupGuide()` |
| `QtProject_1.cpp` `showEvent` | 布局完成后 `startIfNeeded()`（**必须等布局完成**，否则高亮坐标错） |
| `QtProject_1.cpp` `setupStartupGuide()` | 创建 `GuideManager`、设 `productId` |
| `QtProject_1.cpp` `populateBasicCaptureGuide()` | **7 步样例**，移植时整段替换为你的步骤 |
| `onOpenCamera()` 等 | 业务成功后 `notifyCondition("xxx")` |

当前样例配置：

```cpp
m_startupGuide->setProductId(QStringLiteral("QtProject_1"));
m_startupGuide->setGuideId(QStringLiteral("basic_capture"));
m_startupGuide->setVersion(4);   // 改步骤内容后请 +1
```

完成键（Windows 注册表 / QSettings）形如：

```text
GuideKit/QtProject_1/basic_capture/v4/Completed = true
```

---

## 5. 如何新增 / 修改一步（逐步操作）

### 5.1 最小一步（无门控）

在 `populateBasicCaptureGuide()`（或你的注册函数）里：

```cpp
GuideStep step;
step.id = QStringLiteral("my_step");           // 唯一 ID，用于 stepChanged 信号
step.title = QStringLiteral("步骤标题");
step.description = QStringLiteral("正文说明，可多行语义。");
step.actionHint = QStringLiteral("您可在此操作；完成后点击「下一步」。");  // 可留空，库会生成默认句

// 高亮哪个控件（二选一，推荐 targetGetter）
step.targetGetter = [this]() -> QWidget * { return ui.myButton; };
// step.targetObjectName = QStringLiteral("myButton");  // 需控件 setObjectName

m_startupGuide->addStep(step);
```

### 5.2 高亮前先滚到可见区域

本仓库工作流在 `QScrollArea` 里，切步时用 `ensureWidgetVisible`：

```cpp
auto ensureVisible = [this](QWidget *widget) {
    if (widget)
        ui.workflowScroll->ensureWidgetVisible(widget, 24, 24);
};

step.beforeShow = [this, ensureVisible]() {
    ensureVisible(ui.cameraConnectGroup);
};
```

### 5.3 要求用户先操作，再允许「下一步」

以「打开相机」步为例（见 `QtProject_1.cpp`）：

**步骤侧：**

```cpp
openCamera.requireCanProceed = true;
openCamera.conditionId = QStringLiteral("camera_opened");
openCamera.canProceed = [this]() { return m_camera.isOpen(); };
```

**业务侧**（相机打开成功后）：

```cpp
if (m_startupGuide)
    m_startupGuide->notifyCondition(QStringLiteral("camera_opened"));
```

说明：

- `conditionId` 与 `notifyCondition` 参数字符串**必须一致**。
- 库每 **350ms** 轮询一次 `canProceed`；`notifyCondition` 会立刻刷新按钮。
- 条件不满足时仅 **「下一步」变灰**，用户仍可点高亮按钮去操作。

### 5.4 删除一步

在 `populateBasicCaptureGuide()` 里删掉对应 `addStep(...)` 块，**并** `setVersion(n+1)`。

### 5.5 调整顺序

调整多个 `addStep` 的先后顺序即可，**并** `setVersion(n+1)`。

### 5.6 末步

最后一步气泡按钮自动显示 **「完成」**（不是「下一步」），点击后写完成标记。

---

## 6. GuideStep 字段速查

| 字段 | 类型 | 说明 |
|------|------|------|
| `id` | `QString` | 步骤 ID |
| `title` | `QString` | 气泡标题 |
| `description` | `QString` | 正文 |
| `actionHint` | `QString` | 蓝色操作提示行；空则用库默认 |
| `targetObjectName` | `QString` | 按 objectName 找控件 |
| `targetGetter` | `function<QWidget*()>` | **推荐**，动态返回要高亮的控件 |
| `beforeShow` | `function<void()>` | 进入本步前：切 Tab、滚屏、临时 disable 等 |
| `afterLeave` | `function<void()>` | 离开本步后：恢复 enable 等 |
| `conditionId` | `QString` | 与 `notifyCondition` 配对 |
| `canProceed` | `function<bool()>` | 返回 true 时「下一步」可点 |
| `requireCanProceed` | `bool` | true 时启用上门控 |
| `allowSkip` | `bool` | 是否显示「跳过」，默认 true |
| `allowPrevious` | `bool` | 是否显示「上一步」，默认 true |
| `margin` | `int` | 高亮洞相对控件外扩像素，默认 8 |
| `maxBubbleWidth` | `int` | 气泡最大宽度，默认 380 |

---

## 7. GuideManager API（业务常用）

| 方法 | 何时调用 |
|------|----------|
| `setProductId(name)` | 创建后一次，如 `"QtProject_1"` |
| `setGuideId(name)` | 每组引导一个 ID，如 `"basic_capture"` |
| `setVersion(n)` | **每次改步骤内容后递增** |
| `clearSteps()` + `addStep()` | 注册步骤前清空 |
| `startIfNeeded()` | 首次启动且未完成时显示（本仓库在 `showEvent`） |
| `start()` | 调试：强制显示（忽略完成标记） |
| `stop()` | 中止，**不**写完成标记 |
| `skip()` | 用户跳过，写完成标记 |
| `complete()` | 正常完成（一般由气泡「完成」触发） |
| `notifyCondition(id)` | 业务动作成功后刷新「下一步」 |
| `isCompleted()` | 是否已完成 |
| `resetCompletion()` | 清除完成标记（调试） |
| `setTheme(theme)` | 换肤 |

**信号**（可选连接）：

- `started()` — 引导开始
- `stopped(bool completed)` — 结束
- `skipped()` — 跳过
- `stepChanged(stepId, index, total)` — 切步

---

## 8. 主题 GuideTheme

```cpp
GuideTheme theme;
theme.maskColor = QColor(0, 0, 0, 155);           // 遮罩透明度
theme.highlightColor = QColor(64, 158, 255);      // 高亮边框
theme.hintColor = QColor(24, 144, 255);           // 操作提示行
theme.titleFont.setPointSize(11);
m_startupGuide->setTheme(theme);
```

字段定义见 `GuideTheme.h`；引导运行中也可 `setTheme` 热更新。

---

## 9. 本仓库 7 步样例对照表

| 序号 | `id` | 高亮目标 | 门控 |
|------|------|----------|------|
| 1 | `select_camera` | `cameraSelectCombo` | 无 |
| 2 | `open_camera` | `openCameraBtn` | `camera_opened` ← `m_camera.isOpen()` |
| 3 | `camera_params` | `cameraParamGroup` | 无 |
| 4 | `start_capture` | `startGrabBtn` | `capture_started` ← `m_acquisitionActive` |
| 5 | `save_one` | `saveOneBmpBtn` | 无（介绍型） |
| 6 | `stage_capture` | `stageGroup` | 无 |
| 7 | `save_path` | `saveSettingGroup` | 无（末步） |

改 UI 控件名时，同步改 `targetGetter` 里返回的 `ui.xxx`。

---

## 10. 调试与测试

### 10.1 强制再次显示引导

**方法一（推荐）**：`setVersion(5)`（或当前 +1），重新编译运行。

**方法二**：代码里临时 `m_startupGuide->resetCompletion();` 再 `start()`。

**方法三**：删注册表/QSettings 键（见 §4 完成键路径）。

### 10.2 自检清单

| 操作 | 预期 |
|------|------|
| 首次启动 | 出现第 1 步 |
| 已完成后再开 | 不出现 |
| 引导中点击高亮按钮 | 与无引导时相同 |
| 门控步未满足条件 | 「下一步」灰，其它可操作 |
| 满足条件 / notifyCondition | 「下一步」亮 |
| 拖窗口大小 | 遮罩与气泡跟随 |
| 高亮控件被销毁 | 不崩溃（库监听 destroy） |

### 10.3 常见问题

| 现象 | 原因 | 处理 |
|------|------|------|
| 高亮框位置错/在 (0,0) | 布局未完成就 `start` | 像本仓库一样在 `showEvent` + `singleShot(0)` |
| 改了文案用户看不到 | 未升 `version` | `setVersion(n+1)` |
| 「下一步」一直灰 | `canProceed` 一直 false 或 `conditionId` 不一致 | 检查 lambda 与 `notifyCondition` 字符串 |
| 找不到高亮控件 | `targetGetter` 返回 nullptr | 确认指针、objectName、是否已 create |
| 滚动区里看不到高亮 | 控件在可视区外 | `beforeShow` 里 `ensureWidgetVisible` |

---

## 11. 移植到新工程（替换样例）

1. 复制 `guide/` + 按 §3 加入工程。
2. 主窗口持有 `GuideManager *`，构造里 `new GuideManager(this, this)`（root 一般为窗口自身或 central widget）。
3. 写 `populateYourGuide()` 替换 `populateBasicCaptureGuide()` 内容。
4. `showEvent` 末尾 `startIfNeeded()`。
5. 在业务成功回调里 `notifyCondition`。
6. **不要**把业务步骤写进 `guide/` 库目录；库保持通用，步骤全在宿主 `.cpp`。

---

## 12. 行为契约（改库前必读）

1. **不挡鼠标**：`GuideOverlay` 使用 `WA_TransparentForMouseEvents`。
2. **不自动跳步**：只有用户点「下一步」/「完成」才前进。
3. **门控只灰「下一步」**：不禁用目标控件（除非你在 `beforeShow` 自己 disable 别的面板）。
4. **末步「完成」始终可点**：即使写了 `requireCanProceed`。
5. **C++14**：`guide/` 内勿用 C++17 语法。

---

## 13. 改库源码时

| 文件 | 可改场景 |
|------|----------|
| `GuideBubbleWidget.cpp` | 按钮文案、布局、进度格式 |
| `GuideOverlay.cpp` | 高亮形状、动画 |
| `GuideManager.cpp` | 轮询间隔 `kConditionPollMs`、QSettings 键规则 |
| `GuideStep.h` | 新增步骤字段（需同步 Manager） |

改库内注释须符合项目 `code-comments.mdc`（函数上一行中文说明）。

---

## 14. 相关链接

- 项目总则：`README.md` → 一分钟了解 / 架构
- 开发者时序：`docs/DEVELOPER_GUIDE.md`
- GuideKit 契约与测试：`docs/GUIDEKIT_DEVELOPMENT_GUIDE.md`

---

**维护提示**：每次修改 `populateBasicCaptureGuide()` 中的步骤定义或文案，请同步 **递增 `setVersion`**，并在提交说明里写清引导变更，便于测试与用户重新学习。
