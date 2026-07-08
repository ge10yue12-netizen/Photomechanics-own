# 屏幕录制模块（recorder）

Windows 桌面录屏独立套件。**复制 `recorder/` 目录 → 配置工程 → 接入公开 API**，即可集成。

## 平台与依赖

| 项 | 要求 |
|----|------|
| 操作系统 | Windows 10+ |
| 编译器 | MSVC 2017+（C++14） |
| Qt | 5.12+（core / gui / widgets） |
| 采集 | DXGI Desktop Duplication（录制）+ GDI（预览回退） |
| 编码 | Media Foundation H.264（MP4）、MJPEG（AVI） |

模块内**无**对特定业务工程的依赖；集成方只需 `#include` 公开头文件。

---

## 架构分层

```
┌─────────────────────────────────────────┐
│  RecorderQtKit.h  — Qt 集成唯一入口      │
│  ScreenRecorderDialog / RecorderController│
├─────────────────────────────────────────┤
│  RecorderKit.h    — 纯 C++ 核心 API      │
│  recorder::ScreenRecorder               │
├─────────────────────────────────────────┤
│  src/Capture   DXGI / GDI 采集           │
│  src/Encoder   MP4(H.264) / AVI(MJPEG)  │
└─────────────────────────────────────────┘
```

| 头文件 | 用途 |
|--------|------|
| `recorder/RecorderQtKit.h` | **带 Qt 界面**时的唯一入口 |
| `recorder/RecorderKit.h` | **无 Qt / 自绘 UI**时的纯 C++ 入口 |

录制文件默认保存在**项目根目录** **`Record/`** 下（与 `Log/` 同级）。**参数设置**里配置的是**保存目录**；每次点「开始」自动生成新文件 `record_yyyyMMdd_HHmmss.mp4`（EV 同款，不会覆盖上一段）。

### 界面布局

| 页 | 内容 |
|----|------|
| **常规** | 全宽预览；底行同一行：采集模式下拉（主屏 / 区域 / 可选固定窗口如「相机预览录制」）+ 重选区域 + 录制控制条 + 右下角状态文字 |
| **参数设置** | EV 式：**视频帧率 (fps)** / **画质级别** / **编码级别**；格式 `.mp4`/`.avi`；**实际输出**摘要实时预览 |
| **录制输出** | **扫描保存目录**下的全部 `.mp4`/`.avi`，按时间排序 |

**列表与磁盘（EV 逻辑）**

- **列表 = 保存目录里的视频文件**（打开「录制输出」页时扫描）
- **时长**从 MP4/AVI **容器读取**（Media Foundation，四舍五入到整秒），无 sidecar JSON
- **录制中**控制条显示墙钟已录秒数（不含暂停）；**停止后**与列表均用同一容器 probe
- 列表「删除」= 删磁盘文件；资源管理器删改文件 → **保存目录监视**自动同步（无需切页或手动刷新）

录制进行中会禁用区域选择与参数页控件；停止后 2 秒 toast「录制已保存。」，无确认框。

---

## 第一步：复制目录

将 **`recorder/`** 整个文件夹复制到目标工程根目录（与 `.vcxproj` 同级）。

目录内已包含：界面（`ui/`）、对话框（`dialog/`）、Qt 集成层、采集与编码核心（`src/`）。**无需从其他位置拷贝文件。**

---

## 第二步：工程配置

在目标 `.vcxproj` 中增加以下条目，并确认工程级设置。

### 工程级设置

| 配置项 | 值 |
|--------|-----|
| C++ 语言标准 | C++14 或更高 |
| Qt 模块 | core、gui、widgets |
| 附加包含目录 | `$(ProjectDir)` |
| 附加链接库 | `windowscodecs.lib` `ole32.lib` `d3d11.lib` `dxgi.lib` `mfplat.lib` `mfreadwrite.lib` `mfuuid.lib` |

### QtUic（5 个界面）

各子面板独立 `.ui`，可在 Qt Designer 分别编辑；主窗通过 Promote 嵌入子控件。

```xml
<QtUic Include="recorder\ui\ScreenRecorderDialog.ui" />
<QtUic Include="recorder\ui\ScreenRecorderSettingsDialog.ui" />
<QtUic Include="recorder\ui\RecorderNavSidebar.ui" />
<QtUic Include="recorder\ui\RecorderControlBar.ui" />
<QtUic Include="recorder\ui\RecorderOutputListWidget.ui" />
```

### QtMoc

```xml
<QtMoc Include="recorder\RecorderControlBar.h" />
<QtMoc Include="recorder\RecorderNavSidebar.h" />
<QtMoc Include="recorder\RecorderController.h" />
<QtMoc Include="recorder\RegionSelectorWidget.h" />
<QtMoc Include="recorder\RecorderPreviewWidget.h" />
<QtMoc Include="recorder\RecorderOutputListWidget.h" />
<QtMoc Include="recorder\dialog\ScreenRecorderDialog.h" />
<QtMoc Include="recorder\dialog\ScreenRecorderSettingsDialog.h" />
```

### ClCompile

```xml
<ClCompile Include="recorder\dialog\ScreenRecorderDialog.cpp" />
<ClCompile Include="recorder\dialog\ScreenRecorderSettingsDialog.cpp" />
<ClCompile Include="recorder\RecorderControlBar.cpp" />
<ClCompile Include="recorder\RecorderNavSidebar.cpp" />
<ClCompile Include="recorder\RecorderController.cpp" />
<ClCompile Include="recorder\RecorderUiBinder.cpp" />
<ClCompile Include="recorder\RecorderPathHelper.cpp" />
<ClCompile Include="recorder\RecorderOutputStore.cpp" />
<ClCompile Include="recorder\RecorderPreviewCapture.cpp" />
<ClCompile Include="recorder\RecorderPreviewWidget.cpp" />
<ClCompile Include="recorder\RecorderOutputListWidget.cpp" />
<ClCompile Include="recorder\RecorderQtWindowTarget.cpp" />
<ClCompile Include="recorder\RegionSelectorWidget.cpp" />
<ClCompile Include="recorder\src\ScreenRecorder.cpp" />
<ClCompile Include="recorder\src\RecorderError.cpp" />
<ClCompile Include="recorder\src\RecorderWindowTarget.cpp" />
<ClCompile Include="recorder\src\Capture\CaptureOpen.cpp" />
<ClCompile Include="recorder\src\Capture\GdiScreenCapture.cpp" />
<ClCompile Include="recorder\src\Capture\GdiWindowCapture.cpp" />
<ClCompile Include="recorder\src\Capture\DxgiScreenCapture.cpp" />
<ClCompile Include="recorder\src\Encoder\MjpegAviEncoder.cpp" />
<ClCompile Include="recorder\src\Encoder\Mp4MfEncoder.cpp" />
```

### ClInclude（公开与内部头文件）

```xml
<ClInclude Include="recorder\RecorderKit.h" />
<ClInclude Include="recorder\RecorderQtKit.h" />
<ClInclude Include="recorder\RecorderStatusText.h" />
<ClInclude Include="recorder\RecorderPathHelper.h" />
<ClInclude Include="recorder\RecorderOutputStore.h" />
<ClInclude Include="recorder\RecorderPreviewCapture.h" />
<ClInclude Include="recorder\RecorderUiBinder.h" />
<ClInclude Include="recorder\include\RecorderTypes.h" />
<ClInclude Include="recorder\include\RecorderError.h" />
<ClInclude Include="recorder\include\IRecorderWindowTarget.h" />
<ClInclude Include="recorder\include\RecorderWindowTarget.h" />
<ClInclude Include="recorder\RecorderQtWindowTarget.h" />
<ClInclude Include="recorder\include\ScreenRecorder.h" />
<ClInclude Include="recorder\src\Capture\IScreenCapture.h" />
<ClInclude Include="recorder\src\Capture\GdiScreenCapture.h" />
<ClInclude Include="recorder\src\Capture\GdiWindowCapture.h" />
<ClInclude Include="recorder\src\Capture\DxgiScreenCapture.h" />
<ClInclude Include="recorder\src\Capture\CaptureOpen.h" />
<ClInclude Include="recorder\src\Capture\CaptureCommon.h" />
<ClInclude Include="recorder\src\Encoder\IVideoEncoder.h" />
<ClInclude Include="recorder\src\Encoder\MjpegAviEncoder.h" />
<ClInclude Include="recorder\src\Encoder\Mp4MfEncoder.h" />
```

---

## 第三步：接入 API

集成方**只应** `#include "recorder/RecorderQtKit.h"`（或纯 C++ 时 `RecorderKit.h`），**不要**直接引用 `src/` 内部头文件。

### 带 Qt 界面

```cpp
#include "recorder/RecorderQtKit.h"

// 成员变量（生命周期须覆盖对话框）
RecorderController m_recorder;
QWidgetRecorderTarget m_cameraPreviewTarget;
ScreenRecorderDialog *m_recorderDialog = nullptr;

// 构造函数中绑定要录制的 Qt 控件（如主界面预览区）
m_cameraPreviewTarget.setWidget(ui.imageLabel);
connect(ui.imageLabel, &PreviewWidget::visualRevisionChanged, this, [this]() {
    m_cameraPreviewTarget.notifyVisualChanged();
});

void MainWindow::openScreenRecorder()
{
    if (!m_recorderDialog) {
        m_recorderDialog = new ScreenRecorderDialog(this);
        m_recorderDialog->setFixedWindowTarget(&m_cameraPreviewTarget,
                                               QStringLiteral("相机预览录制"));
        m_recorderDialog->bindController(&m_recorder);
    }
    m_recorderDialog->show();
    m_recorderDialog->raise();
    m_recorderDialog->activateWindow();
}
```

可选：连接 `RecorderController` 信号做日志或状态栏更新：

```cpp
connect(&m_recorder, &RecorderController::logMessage, this, &MainWindow::appendLog);
connect(&m_recorder, &RecorderController::stateChanged, this, &MainWindow::onRecorderState);
```

### 相机预览录制（Qt 视觉缓存）

`PreviewWidget` 等 **QPainter 绘制**的控件无法靠 `PrintWindow` 可靠抓到画面。模块改为：

| 环节 | 行为 |
|------|------|
| 主线程 | `recorderVisualSnapshot()` / 按需 `refreshVisualCache()` 写入 `VisualFrameCache` |
| 录制线程 | `QtWidgetVisualCapture` 只读缓存（mutex 拷贝），**不**调用 `grab()` / GDI |
| 性能 | `VisualConsumerFlag`：无消费者时 **零额外渲染**；`LivePreview` 按预览定时器强制刷新；`Recording` 随源 `notifyVisualChanged` 更新 |
| 预览 | 窗口模式走视觉缓存直读（每 tick `refreshVisualCache(true)`），与 OBS 源预览轮询同类逻辑 |

逻辑：录的就是预览框里当前显示的一切（缩放、黑边、占位文字、静止或变化画面均可）。

### 纯 C++（无 Qt 界面）

```cpp
#include "recorder/RecorderKit.h"

recorder::ScreenRecorder recorder;
recorder::RecorderConfig cfg;
cfg.mode = recorder::CaptureMode::FullScreen;
cfg.output.format = recorder::VideoFormat::Mp4;
cfg.output.filePath = "D:/Record/test.mp4";

const recorder::ResolvedVideoParams video = recorder::resolveVideoPresets(
    recorder::QualityLevel::UltraClear,
    recorder::EncodeLevel::Default,
    recorder::FrameRatePreset::Standard,
    1920,
    1080,
    recorder::VideoFormat::Mp4);
cfg.video.fps = video.fps;
cfg.video.bitrateKbps = video.bitrateKbps;
cfg.video.encodeLevel = video.encodeLevel;
cfg.video.outputWidth = video.outputWidth;
cfg.video.outputHeight = video.outputHeight;

recorder.init(cfg);
recorder.start();
// ...
recorder.stop();
```

---

## 核心 API 速查

### `recorder::ScreenRecorder`

| 方法 | 说明 |
|------|------|
| `init(config)` | 初始化采集与编码 |
| `start()` | 开始录制 |
| `pause()` / `resume()` | 暂停 / 继续 |
| `stop()` | 停止并写入文件 |
| `state()` | 当前状态 |
| `setCallback(cb)` | 注册状态/时长/错误/日志回调 |

### `RecorderController`（Qt 封装）

与 `ScreenRecorder` 方法一一对应，额外提供 Qt 信号：`stateChanged`、`durationChanged`、`errorOccurred`、`logMessage`。回调已在内部通过 `QTimer::singleShot` 切到主线程。

---

## 性能设计要点

- **录制**：DXGI 1:1 物理像素，H.264 原生分辨率编码
- **静止桌面**：DXGI 超时 → 不写 MP4 样本，合并为 VFR 时间轴（EV 同款思路）；有变化时才编码
- **不追帧**：worker 落后时丢弃中间帧，避免 CPU 突发
- **预览**：GDI StretchBlt 长边 ≤640，与录制线程独立；主面板可见且在「常规」页时录制/暂停也继续刷新；隐藏或非常规页自动停预览
- **默认 20 FPS**：与常见录屏工具一致，可在设置中调整
- **码率**：默认 5000 kbps，VBR + 峰值上限；静止段靠 VFR + 像素差分过滤，实际体积远低于均值

---

## 目录结构

```
recorder/
├── RecorderQtKit.h        ← Qt 集成唯一 #include
├── RecorderKit.h          ← 纯 C++ 入口
├── RecorderController.*   ← Qt 信号槽封装
├── ui/                    ← Qt Designer（5 个 .ui）
├── dialog/                ← 主对话框逻辑
├── include/               ← 对外 C++ API
└── src/                   ← 采集、编码实现（集成方勿直接引用）
    ├── Capture/
    └── Encoder/
```

预览（`RecorderPreviewWidget`）与区域选择（`RegionSelectorWidget`）为代码自绘，无 `.ui` 文件。

---

## 线程与生命周期

- 录制在**后台 worker 线程**执行；`RecorderCallback` 可能在非 UI 线程触发
- **UI 时长与成片时长一致**：录制中按墙钟；停止后与输出列表均用 `RecorderVideoProbe`（100ns 四舍五入到秒）
- `RecorderController` 已将回调转主线程；析构时自动 `stop()`；若直接使用 `ScreenRecorder`，须自行 `QMetaObject::invokeMethod` 或等价机制
- 录制中关闭主面板仅隐藏窗口，录制由悬浮球继续；停止后自动恢复主面板
- 应用退出时 `shutdownAll` → `RecorderController::stop()` 落盘；不在 close/析构中停录
- 悬浮球（`RecorderFloatBall`）：开录后自动显示并隐藏主面板；拖拽跟手自由定位（松手停哪儿是哪儿），位置写入 `QSettings`；单击延迟展开迷你控制条（避开双击）、双击打开主面板
- `bindController` 传入的指针**须由调用方持有**，生命周期不短于对话框

---

## 已知限制

- 仅 Windows；非 Windows 平台编译时 `RecorderKit.h` 将 `#error`
- WMV / RM / RMVB 枚举保留但未实现，UI 中已 disabled
- DXGI 每显示器输出仅允许一个 Duplication 会话；开始录制前会自动释放预览占用
- H.264 4:2:0 对 ClearType 文字可能有轻微色度损失，属编码格式特性而非采集缺陷
