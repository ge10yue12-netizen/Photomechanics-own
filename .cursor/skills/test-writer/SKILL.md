---
name: test-writer
description: 为 QtProject_1 补充 Qt Test 单元测试。遵循 test-standards.mdc。
---

# Test Writer — QtProject_1

## 触发

写测试、补测试、单元测试、边界测试。

## 规范

Read `.cursor/rules/test-standards.mdc` 与 `~/.cursor/rules/test-standards.mdc`。

## 框架

**Qt Test** — 工程 `tests/QtProject_1_tests.vcxproj`，聚合入口 `tst_core_modules.cpp`。

## 优先覆盖

| 模块 | 场景 |
|------|------|
| `SavePathHelper` | 路径格式、Pic 递增、上限、阶段目录 |
| `ImageSaveThread` | 队列满、`trySubmit`、BMP 落盘 |
| `StageManager` | 目标帧数、模拟 `saveFrameRequested` 回调 |
| `CameraController` | 须 mock，默认不测；用户明确要求再设计 |

## 流程

1. Read 被测代码
2. 不引入 Google Test 等新框架
3. 用 `QTemporaryDir` 隔离文件 IO
4. 用 `QSignalSpy` / 模拟槽验证信号
5. 在 `tst_core_modules.cpp` 扩展或新建 `tst_*.cpp`

## 输出格式

测试目标 → 测试场景 → 测试代码 → 运行方式（`x64\Debug\QtProject_1_tests.exe`）
