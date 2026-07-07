# 屏幕录制模块（recorder）

Windows 桌面录屏套件。**复制 `recorder/` 文件夹 → 改宿主工程配置 → 接宿主接口**，三步完成集成。

---

## 第一步：复制文件夹

将 **`recorder/`** 整个目录复制到目标 Qt/VS 工程根目录（与主程序 `.vcxproj` 同级）。

模块内已包含：界面（`ui/`）、对话框逻辑（`dialog/`）、Qt 集成层、采集与编码核心（`src/`）。**无需再从其他目录拷贝文件。**

---

## 第二步：修改宿主工程配置

在目标工程的 **`.vcxproj`** 中增加录屏模块的编译项。最省事的做法：

> 打开本仓库 **`QtProject_1.vcxproj`**，搜索 `recorder\`，将同类条目（QtUic / QtMoc / ClCompile / ClInclude）**原样加入**目标工程。

另需确认以下工程级设置：

| 配置项 | 值 |
|--------|-----|
| C++ 语言标准 | C++14 |
| Qt 模块 | core、gui、widgets |
| 附加包含目录 | `$(ProjectDir)` |
| 附加链接库 | `windowscodecs.lib` `ole32.lib` `d3d11.lib` `dxgi.lib` `mfplat.lib` `mfreadwrite.lib` `mfuuid.lib` |

**界面文件（Qt Designer 可编辑，共 5 个，均在 `recorder/ui/`）：**

- `ScreenRecorderDialog.ui` — 主面板
- `ScreenRecorderSettingsDialog.ui` — 参数设置
- `RecorderNavSidebar.ui` — 左侧导航
- `RecorderControlBar.ui` — 底部控制条
- `RecorderOutputListWidget.ui` — 输出列表

在 VS「解决方案资源管理器」中，上述文件位于 **recorder → ui** 筛选器下；`.vcxproj.filters` 可参考本仓库 `QtProject_1.vcxproj.filters`。

---

## 第三步：修改宿主接口

宿主工程**只包含一个头文件**，不要散落引用 `recorder/` 子路径。

**宿主头文件（例：`MainWindow.h`）**

```cpp
#include "recorder/RecorderQtKit.h"

RecorderHost m_recorderHost;
ScreenRecorderDialog *m_recorderDialog = nullptr;
```

**宿主实现（例：菜单/按钮槽函数）**

```cpp
void MainWindow::onOpenScreenRecorder()
{
    if (!m_recorderDialog) {
        m_recorderDialog = new ScreenRecorderDialog(this);
        m_recorderDialog->bindRecorderHost(&m_recorderHost);
    }
    m_recorderDialog->show();
    m_recorderDialog->raise();
    m_recorderDialog->activateWindow();
}
```

无 Qt 界面、只要录制能力时，改用 `recorder/RecorderKit.h`（纯 C++ API）。

---

## 对外接口

| 头文件 | 用途 |
|--------|------|
| `recorder/RecorderQtKit.h` | **Qt 宿主唯一入口**（推荐） |
| `recorder/RecorderKit.h` | 无 Qt / 自绘 UI 时的纯 C++ 入口 |

录制文件默认保存在可执行文件旁 **`Record/`** 目录（含 `output_history.json`）。

---

## 目录结构

```
recorder/
├── RecorderQtKit.h      ← 宿主唯一 #include
├── RecorderKit.h        ← 纯 C++ 入口
├── RecorderHost.*       ← Qt 门面（信号槽）
├── ui/                  ← Qt Designer 界面（5 个 .ui）
├── dialog/              ← 主对话框逻辑
├── include/             ← 对外 C++ API
└── src/                 ← 采集、编码、状态机
    ├── Capture/
    └── Encoder/
```

预览画面（`RecorderPreviewWidget`）与区域选择（`RegionSelectorWidget`）为代码自绘，无 `.ui` 文件。

---

## 本仓库参考

| 项 | 位置 |
|----|------|
| 打开录屏 | `QtProject_1::onScreenRecorder()` |
| 宿主成员 | `QtProject_1.h` |
| 工程配置样例 | `QtProject_1.vcxproj`（搜索 `recorder\`） |
