<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/tool`（终局 dispatch）

SmartGIS 工具终局树：只保留 session 作用域的 Command / Interaction / Workspace。2010 leftover `SmtIATool` / `SmtGroupTool` 已迁到 [`src/legacy_tool/`](../legacy_tool/)。

## 目录

| 路径 | 角色 |
| --- | --- |
| `command.*` | CommandCatalog / CommandDispatcher（稳定 string id） |
| `interaction.*` | Interaction / Stack / InputRouter（鼠标状态机） |
| `workspace.*` | 每视图组合根 |
| `gestures.*` | 内置手势（如 `wheel.zoom`） |
| `legacy_msg.*` | `GT_MSG_*` → command id 适配 |

GN：`//src/tool:dispatch` 进日常 `src_all`。leftover DLL 另编 `//src/legacy_tool:legacy_tool_all`（默认不进 `src_all`）。

## 依赖方向

- 新代码 / chrome → `content::ViewHost` / `ToolRouter` → `tool::Workspace`
- 文档写入 → `sdb::EditSession`；域事件 → `content::EventBus`（不在本目录）
- `tool` 终局 → `legacy_tool` **禁止**；leftover 可依赖 dispatch 适配

设计：[`docs/superpowers/specs/2026-09-13-tool-event-dispatch-design.md`](../../docs/superpowers/specs/2026-09-13-tool-event-dispatch-design.md)、[`docs/superpowers/specs/2026-09-13-tool-legacy-split-design.md`](../../docs/superpowers/specs/2026-09-13-tool-legacy-split-design.md)、[`docs/build/src-layout.md`](../../docs/build/src-layout.md)。
