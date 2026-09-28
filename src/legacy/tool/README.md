<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/legacy/tool`

Leftover 2010 `SmtIATool` / group tools and the GT_MSG → endgame Workspace bridge. **Not** the desktop tool endgame (`src/tool/{command,interaction,draft,nav,workspace}`).

| Subdir | Role | GN |
| --- | --- | --- |
| `iatool/` | `SmtIATool`, manager, `t_msg`, `TOOL_EXPORT` | `//src/legacy/tool/iatool:tool_sources` → `dll_stem=legacy_tool` |
| `adapter/` | `GT_MSG_*` → `tool::Workspace` (`msg.h` / `msg.cc`; `namespace tool`) | `//src/legacy/tool/adapter:adapter` (source_set; not the DLL) |
| `group/{base,view,select,input,factory}` | `Smt*Tool` + factory; root keeps `defs.h` / RC / `res/` (sources → `ui_legacy`) | `//src/legacy/tool/group:tool_group_sources` |

Aggregate: `//src/legacy/tool:legacy_tool` / `:tool` / `:legacy_tool_all` (optional; not in `src_all`).

Package layout (landed): [`docs/superpowers/archive/specs/2026-09-27-legacy-tool-subdirectory-layout-design.md`](../../../docs/superpowers/archive/specs/2026-09-27-legacy-tool-subdirectory-layout-design.md).  
Group role dirs (active): [`docs/superpowers/specs/2026-09-27-legacy-tool-group-subdirectory-layout-design.md`](../../../docs/superpowers/specs/2026-09-27-legacy-tool-group-subdirectory-layout-design.md).  
Strangler: [`docs/superpowers/specs/2026-09-19-legacy-tool-workspace-strangler-design.md`](../../../docs/superpowers/specs/2026-09-19-legacy-tool-workspace-strangler-design.md).
