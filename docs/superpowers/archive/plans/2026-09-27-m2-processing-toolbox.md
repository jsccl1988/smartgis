<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# M2 — Processing 工具箱 Implementation Plan

**Status:** landed (archived 2026-10-03 — checkboxes complete)

> **For agentic workers:** Implement task-by-task. Stay on **master**. Do **not** `git commit` unless the user asks. Do **not** create branches.

**Goal:** Views「处理」面板列出 ≥10 个算子；`buffer` / `clip` 批跑写回图层；`--self-test` 含 `m2-*` marks；不破坏 M0/M1。

**Architecture:** 复用 `content::PluginHost` + `ProcessingContribution` + `plugin::ProcessingPool`；新增进程内算子注册表（包装 `gis/geo` GEOS/OGR），Views 侧列表面板触发 `run_processing`；结果写回 `MapScene` 活动层（GeoJSON 往返或内存 feature）。

**Tech Stack:** C++23、`gis/geo`、`content::PluginHost`、Views、GN/`build.bat`.

## Path ownership（并行硬约束）

**MAY edit:**
- `src/gis/geo/**`（仅必要时扩 buffer/clip 辅助）
- `src/plugin/processing/**`（新建：内置算子贡献）
- `src/ui/gis/processing_panel.*`（新建）
- `src/app/views/shell/plugin/plugin_shell.*`（注册算子）
- `src/app/views/browser_view.*` / `browser_view.h`（挂面板 + 菜单「处理」）
- `src/app/views/BUILD.gn`、`src/ui/views/BUILD.gn`、相关 test
- `docs/superpowers/plans/2026-09-27-m2-processing-toolbox.md`
- `docs/superpowers/ui-testing.md`（M2 退出码节）

**MUST NOT edit:**
- `src/app/views/main.cc`（父 agent 统一加 `m2-*` marks）
- `src/app/views/map_scene.*`（除非绝对必要；优先通过 BrowserView API）
- `src/gis/scene/world/**`、`src/render/**`、`src/gis/edit/**`、`src/content/public/**`（除已有 PluginHost 调用）
- M3/M4 计划文件

## Acceptance

1. 面板列出 ≥10 个 `processing_id`（含 `native.buffer`、`native.clip`）。
2. buffer/clip 对活动层或临时 GeoJSON 跑通并写回 ≥1 feature。
3. 单元测试或 `processing_*_test` PASS；`build.bat views` 绿。
4. 在 `BrowserView` 暴露 `bool run_m2_self_test_hooks(std::string* err)`（或等价）供父 agent 在 `main.cc` 调用。

## Tasks

- [x] Task 1: 内置 ProcessingContribution 注册 ≥10 算子（buffer/clip/centroid/envelope/… 可用 OGR/GEOS 薄包装；无后端则返回清晰失败）。
- [x] Task 2: Views `ProcessingPanel` + BrowserView 挂接。
- [x] Task 3: buffer/clip 批跑写回 MapScene；测试绿。
- [x] Task 4: 文档 ui-testing M2 码预留 80–82；不改 main.cc。

## Constraints

- 禁止 Qt；禁止 `#include "legacy/…"` 新依赖。
- 新函数 `snake_case`；注释英文；版权 2026 Mogu Authors。
- YAGNI：不做完整 QGIS Processing 模型器。
