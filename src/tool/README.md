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

**双指平移**：双指左右（及任意方向）拖动是**平移**，不是缩放。路径：
1. 触屏 `WM_GESTURE` **`GID_PAN`** → `MapHwndGestures` → `apply_pan`（Views）；legacy `view_chrome` → multitouch `InputEvent` → `view.pan` / `view3d.*`
2. `WM_POINTER*` 中点 → `TouchMultitouchTracker`（Views）/ `view_chrome` midpoint（legacy）→ 带 `draft_flags::kTouchPan` 的 `kRect` Draft（CEF `shell.js` 同契约）
3. 触控板横向滚轮 `WM_MOUSEHWHEEL` / CEF `deltaX` → `input_flags::kHorizontalWheel` → always-on 转成同形状 touch-pan Draft

捏合（`GID_ZOOM` / pinch）才是缩放。`MapHwndGestures` / legacy `view_chrome` **接收并处理** `GID_PAN`，不在 pan 会话里再叠一层 pointer 平移。
