---
name: project-audit
description: QtProject_1 全项目审计：在全局六维+H 基础上，追加采集、存图、相机与 Qt 线程专项。
---

# Project Audit — QtProject_1

## 触发

同全局 `project-audit`；本项目优先使用本文件。

## 继承

执行全局 `~/.cursor/skills/project-audit/SKILL.md` 全部流程与输出格式，并追加 **G 类专项**。

## 审计范围（默认）

| 路径 | 说明 |
|------|------|
| `*.cpp` `*.h` | 核心逻辑 |
| `docs/` | 开发文档 |
| `README.md` | 入口摘要 |
| `.cursor/` | 项目配置 |
| `Images/` | 仅检查是否误纳入 Git，不审计图片内容 |

排除：`.vs/`、`x64/`、`Debug/`、`Release/`、`.git/`。

## G 类：业务专项

| 编号 | 检查项 | 期望 |
|------|--------|------|
| G1 | 存图入队 | `enqueueCurrentFrame` → `copyLatestImage` → `trySubmit`；队列满返回 false 且 `queueFull` 告警 |
| G2 | 队列与线程 | `ImageSaveThread::m_taskQueue` 上限 48；`run()` 消费；禁止静默丢帧 |
| G3 | 图像拷贝 | `Grayscale8` 深拷贝一次；跨线程 QImage 用法安全 |
| G4 | 路径 | `SavePathHelper` 路径格式、Pic 递增、目录创建 |
| G5 | 相机生命周期 | Pylon 打开/关闭/异常；`CameraController` 与采集线程 |
| G6 | 阶段机 | `StageManager` 帧数 `round(时长×fps)`；fps 为 0；阶段切换与存图协调 |
| G7 | 预览定时器 | `m_displayTimer` 50ms 与 `m_frameTickTimer` 不互相干扰 |
| G8 | 文档对齐 | `README`、`docs/DEVELOPER_GUIDE.md` 与 `cpp-qt-standards.mdc` 模块表一致 |

对照 `~/.cursor/rules/cpp-qt-standards.mdc` 与 `project-core.mdc` 模块表。

## 本项目 H 维补充

| 项 | 期望 |
|----|------|
| 项目 rules | 含 `project-core`、`git-project`、`skills-project`、`cpp-qt-standards` |
| 无冗余 | 不存在 `agent-core`、`git-workflow`、`skills-workflow`（已上收全局） |
| Skill 覆盖 | `skills-project.mdc` 列出 code-reviewer、debug-helper、test-writer、**project-audit** |
| Git 信息 | 仅在 `git-project.mdc`，远程为 Photomechanics-own |

## 输出补充

在全局报告末尾追加：

### G 类专项结果

每项 G1～G8：通过 / 部分通过 / 不通过 + 发现列表。

### 模块风险热力（可选）

对 `CameraController`、`ImageSaveThread`、`StageManager`、`QtProject_1` 标高/中/低关注等级。
