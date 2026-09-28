<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/tool`（终局 dispatch → `tool.dll`）

SmartGIS 工具终局树：session 作用域的 Command / Interaction / Workspace，编入 **`dll_stem=tool`**（`TOOL_EXPORT`）。2010 leftover `SmtIATool` / `SmtGroupTool` 在 [`src/legacy/tool/{iatool,group}/`](../legacy/tool/)（`LEGACY_TOOL_EXPORT`）。`GT_MSG_*` → command id 桥接在 [`src/legacy/tool/adapter/`](../legacy/tool/adapter/)（`namespace tool` API 不变）。

布局 + DLL ABI：[`docs/superpowers/specs/2026-09-13-tool-event-dispatch-design.md`](../../docs/superpowers/specs/2026-09-13-tool-event-dispatch-design.md)（§Subdirectory layout、§Export macros、§DLL ABI）。

## 目录

| 路径 | 角色 | Include |
| --- | --- | --- |
| `command/` | CommandCatalog / CommandDispatcher（pimpl） | `tool/command/command.h` |
| `interaction/` | Interaction / Stack / InputRouter（pimpl） | `tool/interaction/interaction.h` |
| `draft/` | Draft POD + select/draw/view factories | `tool/draft/draft.h` |
| `nav/` | 像素相机数学 | `tool/nav/camera_nav.h` |
| `workspace/` | `Workspace`（pimpl）+ `detail::DraftPipeline` / `NavBridge` | `tool/workspace/workspace.h` |

根目录仅 `BUILD.gn` + `README.md` + `tool_export.h`（**无**伞头 `tool/*.h`）。

GN：`//src/tool:tool`（`smt_shared_library`）；稳定组 `:dispatch` / `:command` / … 转发到 `:tool`。`GT_MSG` 适配：`//src/legacy/tool/adapter:adapter`（不在 `:tool` 内）。leftover DLL 另编 `//src/legacy/tool:legacy_tool_all`（默认不进 `src_all`）。

## 依赖方向

- 新代码 / shell → `content::ViewHost` / `ToolRouter` → `tool::Workspace`
- 文档写入 → `gis::EditSession`；域事件 → `content::EventBus`（不在本目录）
- `tool` 终局 → `legacy_tool` **禁止**；leftover / ViewHost 可依赖 `//src/legacy/tool/adapter:adapter`
- 叶子：`nav`、`command`；`draft` → `interaction`；`workspace` → command/draft/interaction/nav；adapter（legacy）→ workspace

**SP1b（行为搬迁）**：bound 时指针只走 `Workspace::dispatch_input` → Interaction → `Draft`（含 `Draft.flags` 细类型）；leftover ViewCtrl / Select / Append / **3DViewCtrl** 仅 bind + notify 同步 + `apply_draft` 副作用。见 umbrella §SP1b。

**双指平移**：双指拖动是平移，捏合才是缩放。`nav` 提供 `hwheel_pan_dx` / pinch helpers。
