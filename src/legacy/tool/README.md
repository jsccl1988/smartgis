<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/legacy/tool`

Leftover 2010 `SmtIATool` / capability tools and the GT_MSG → endgame Workspace bridge. **Not** the desktop tool endgame (`src/tool/{command,interaction,draft,nav,workspace}`).

Directory names **mirror** endgame modules where a twin exists (`nav` / `draft` / `select`); leftover-only glue stays as `abi` / `msg`.

| Subdir | Role | GN |
| --- | --- | --- |
| `abi/` | `SmtIATool`, manager, `t_msg`, `LEGACY_TOOL_EXPORT` | `//src/legacy/tool/abi:tool_sources` → `dll_stem=legacy_tool` |
| `msg/` | `GT_MSG_*` → `tool::Workspace` (`msg.h` / `msg.cc`; `namespace tool`) | `//src/legacy/tool/msg:adapter` (source_set; not the DLL) |
| `nav/` | View / 3D view control + zoom apply（终局 `tool/nav` 镜像） | part of `:tool_group_sources` |
| `select/` | Select + flash + query apply | part of `:tool_group_sources` |
| `draft/` | Input point/line/region + append + draft_to_ogr（终局 `tool/draft` 镜像） | part of `:tool_group_sources` |
| `base/` | `SmtBaseTool` / `SmtBase3DTool` | part of `:tool_group_sources` |
| `factory/` | `SmtGroupToolFactory` | part of `:tool_group_sources` |
| *(root)* | `defs.h` / RC / `res/` / `tool_export.h` | — |

Aggregate: `//src/legacy/tool:legacy_tool` / `:tool` / `:legacy_tool_all` / `:tool_group_sources`（→ `ui_legacy`）.

Layout: umbrella §SP1 + [`docs/superpowers/plans/2026-09-29-legacy-tool-bridge-capability-layout.md`](../../../docs/superpowers/plans/2026-09-29-legacy-tool-bridge-capability-layout.md).
