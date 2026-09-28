<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/legacy/tool/group`

Leftover `Smt*Tool` implementations and `SmtGroupToolFactory`. Sources compile into `ui_legacy` (`//src/legacy/tool/group:tool_group_sources`).

| Subdir | Role |
| --- | --- |
| `base/` | `SmtBaseTool` / `SmtBase3DTool` |
| `view/` | 2D / 3D view control tools |
| `select/` | Select + flash |
| `input/` | Input point/line/region + append feature |
| `factory/` | `SmtGroupToolFactory` |
| *(root)* | `defs.h`, `resource.h`, `group_tool_core.rc`, `res/` |

Include example: `#include "legacy/tool/group/factory/grouptoolfactory.h"`. Shared enums stay at `#include "legacy/tool/group/defs.h"`.

Layout: [`docs/superpowers/specs/2026-09-27-legacy-tool-group-subdirectory-layout-design.md`](../../../../docs/superpowers/specs/2026-09-27-legacy-tool-group-subdirectory-layout-design.md).
