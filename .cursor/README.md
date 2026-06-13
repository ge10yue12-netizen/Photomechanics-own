# QtProject_1 Cursor 配置

本项目配置与全局 `~/.cursor/` 分层加载：全局自动生效，本节仅项目增量。

## 规则（`.cursor/rules/`）

| 文件 | alwaysApply | 职责 |
|------|-------------|------|
| `project-core.mdc` | true | 项目标识、模块、README 章节 |
| `git-project.mdc` | true | 仓库 URL、分支、排除项 |
| `skills-project.mdc` | true | Skill 路径、Context7 库名 |
| `cpp-qt-standards.mdc` | false（`*.cpp,h,hpp`） | 存图与 C++ 约定 |
| `test-standards.mdc` | false（`tests/**`） | Qt Test 规范 |

已移除（上收全局）：`agent-core.mdc`、`git-workflow.mdc`、`skills-workflow.mdc`。

## Skill（`.cursor/skills/`）

覆盖全局同名 Skill，优先于 `~/.cursor/skills/`。

| Skill | 用途 |
|-------|------|
| `code-reviewer` | 单文件/改码后审查 |
| `project-audit` | 全项目量化审计（含 G 类存图/相机专项） |
| `debug-helper` | VS/Qt/Pylon 排错 |
| `test-writer` | Qt Test 单元测试 |

## 测试

规范见 `test-standards.mdc`；工程 `tests/QtProject_1_tests.vcxproj`。

## 元数据

`project-git.json` — 仅供人工参考，Agent 遵 `git-project.mdc`。

## 新项目

其他仓库只需复制并改写 `project-core.mdc`、`git-project.mdc`、`skills-project.mdc`；全局（含 `project-audit`）无需重配。
