<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/tool`（终局 dispatch）

SmartGIS 工具终局树：只保留 session 作用域的 Command / Interaction / Workspace。2010 leftover `SmtIATool` / `SmtGroupTool` 已迁到 [`src/legacy/tool/`](../legacy/tool/)。

## 目录

| 路径 | 角色 |
| --- | --- |
| `command.*` | CommandCatalog / CommandDispatcher（稳定 string id） |
| `interaction.*` | Interaction / Stack / InputRouter（鼠标状态机） |
| `workspace.*` | 每视图组合根 |
| `gestures.*` | 内置手势（如 `wheel.zoom`） |
| `legacy_msg.*` | `GT_MSG_*` → command id 适配 |

GN：`//src/tool:dispatch` 进日常 `src_all`。leftover DLL 另编 `//src/legacy/tool:legacy_tool_all`（默认不进 `src_all`）。

## 依赖方向

- 新代码 / chrome → `content::ViewHost` / `ToolRouter` → `tool::Workspace`
- 文档写入 → `gis::EditSession`；域事件 → `content::EventBus`（不在本目录）
- `tool` 终局 → `legacy_tool` **禁止**；leftover 可依赖 dispatch 适配

设计：[`docs/superpowers/specs/2026-09-13-tool-event-dispatch-design.md`](../../docs/superpowers/specs/2026-09-13-tool-event-dispatch-design.md)、[`docs/superpowers/specs/2026-09-13-tool-legacy-split-design.md`](../../docs/superpowers/specs/2026-09-13-tool-legacy-split-design.md)、[`docs/build/src-layout.md`](../../docs/build/src-layout.md)。

**SP1b（行为搬迁）**：bound 时指针只走 `Workspace::dispatch_input` → Interaction → `Draft`（含 `Draft.flags` 细类型）；leftover ViewCtrl / Select / Append / **3DViewCtrl** 仅 bind + notify 同步 + `apply_draft` 副作用。`Input*` 保留 `GTT_*` 工厂 ABI，几何仅 `apply_draft`（无指针状态机）；bound 数字化由 `draw.*` 覆盖。见 [`docs/superpowers/specs/2026-09-19-tool-behavior-migration-design.md`](../../docs/superpowers/specs/2026-09-19-tool-behavior-migration-design.md)。

**双指平移**：`content::InputEvent::pointer_count >= 2` 时，`(x_px,y_px)` 为触点中点；`view.pan` / `view3d.*` 发带 `draft_flags::kTouchPan` 的 `kRect` Draft。CEF `shell.js` 与 Views `MapViewport`（`WM_POINTER*` → `TouchMultitouchTracker`）均已转发。触控板 / 鼠标横向滚轮走 `WM_MOUSEHWHEEL`（或 CEF `deltaX`）+ `input_flags::kHorizontalWheel`，由 always-on `wheel.zoom` 转成同形状的 touch-pan Draft。`MapHwndGestures` 只消费 `GID_ZOOM` 并 **block** `GID_PAN`，避免吞掉双指平移。
