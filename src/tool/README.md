<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/tool`（终局 dispatch）

SmartGIS 工具终局树：session 作用域的 Command / Interaction / Workspace。2010 leftover `SmtIATool` / `SmtGroupTool` 在 [`src/legacy/tool/{iatool,group}/`](../legacy/tool/)。`GT_MSG_*` → command id 桥接在 [`src/legacy/tool/adapter/`](../legacy/tool/adapter/)（`namespace tool` API 不变）。

布局规格：[`docs/superpowers/specs/2026-09-27-tool-subdirectory-layout-design.md`](../../docs/superpowers/specs/2026-09-27-tool-subdirectory-layout-design.md)。

## 目录

| 路径 | 角色 | Include |
| --- | --- | --- |
| `command/` | CommandCatalog / CommandDispatcher | `tool/command/command.h` |
| `interaction/` | Interaction / Stack / InputRouter | `tool/interaction/interaction.h` |
| `draft/` | Draft POD + select/draw/view factories（原 gestures） | `tool/draft/draft.h` |
| `nav/` | 像素相机数学（原 camera_nav） | `tool/nav/camera_nav.h` |
| `workspace/` | 每视图组合根 | `tool/workspace/workspace.h` |

根目录仅 `BUILD.gn` + `README.md`（**无**伞头 `tool/*.h`）。

GN：`//src/tool:dispatch`（`group` → 各 module `source_set`）进日常 `src_all`。`GT_MSG` 适配：`//src/legacy/tool/adapter:adapter`（`source_set`，非 DLL；不在 `:dispatch` 内）。leftover DLL 另编 `//src/legacy/tool:legacy_tool_all`（默认不进 `src_all`）。

## 依赖方向

- 新代码 / shell → `content::ViewHost` / `ToolRouter` → `tool::Workspace`
- 文档写入 → `gis::EditSession`；域事件 → `content::EventBus`（不在本目录）
- `tool` 终局 → `legacy_tool` **禁止**；leftover / ViewHost 可依赖 `//src/legacy/tool/adapter:adapter`
- 叶子：`nav`、`command`；`draft` → `interaction`；`workspace` → command/draft/interaction/nav；adapter（legacy）→ workspace

设计：[`docs/superpowers/specs/2026-09-13-tool-event-dispatch-design.md`](../../docs/superpowers/specs/2026-09-13-tool-event-dispatch-design.md)、[`docs/build/src-layout.md`](../../docs/build/src-layout.md)。

**SP1b（行为搬迁）**：bound 时指针只走 `Workspace::dispatch_input` → Interaction → `Draft`（含 `Draft.flags` 细类型）；leftover ViewCtrl / Select / Append / **3DViewCtrl** 仅 bind + notify 同步 + `apply_draft` 副作用。见 [`docs/superpowers/specs/2026-09-19-tool-behavior-migration-design.md`](../../docs/superpowers/specs/2026-09-19-tool-behavior-migration-design.md)。

**双指平移**：双指左右（及任意方向）拖动是**平移**，不是缩放。捏合才是缩放。路径见历史 README 说明；`nav` 提供 `hwheel_pan_dx` / pinch helpers。
