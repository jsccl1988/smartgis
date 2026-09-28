<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/legacy/tool/group` subdirectory layout — Implementation Plan


> **Status: superseded** (2026-09-28 merge). Folded into umbrella §SP1. Do not revise here except mechanical link fixes.

> **For agentic workers:** Execute task-by-task on **master**. Do **not** `git commit` unless the user asks. Do **not** create branches. Spec: [`../specs/2026-09-27-legacy-tool-group-subdirectory-layout-design.md`](../specs/2026-09-27-legacy-tool-group-subdirectory-layout-design.md).

**Goal:** Nest flat `Smt*Tool` sources under `group/{base,view,select,input,factory}`; break includes; keep aggregate GN labels and root `defs.h` / RC / `res/`.

**Architecture:** Mechanical `git mv` + include rewrite + update `tool_group_sources` paths. No behavior change.

**Tech Stack:** C++23, GN/`build.bat`, leftover MFC tools linked into `ui_legacy`.

## Global Constraints

- No shim headers at old flat `group/*.h` tool paths
- `defs.h` / `resource.h` / `group_tool_core.rc` / `res/` stay at `group/`
- Labels `:tool_group_sources` / `:tool_group` stay
- No `Smt_*` renames; English comments; copyright 2026 on touched docs/BUILD
- Stay on **master**; no commit unless asked
- Agent does **not** compile

---

## Tasks

- [x] Task 1: Create role dirs; `git mv` paired `.h`/`.cpp`; update `group/BUILD.gn` sources list
- [x] Task 2: Rewrite all in-tree `#include "legacy/tool/group/<file>.h"` for moved headers (group internals + xview + any other hits)
- [x] Task 3: Add `group/README.md`; update `src/legacy/tool/README.md`, `src/legacy/README.md`, `docs/build/src-layout.md` group row; index row in `docs/README.md`
- [ ] Task 4: Human verification — document command; leave Status `active` until green then archive

## Include map (locked)

| Old | New |
| --- | --- |
| `legacy/tool/group/basetool.h` | `legacy/tool/group/base/basetool.h` |
| `legacy/tool/group/base3dtool.h` | `legacy/tool/group/base/base3dtool.h` |
| `legacy/tool/group/viewctrltool.h` | `legacy/tool/group/view/viewctrltool.h` |
| `legacy/tool/group/3dviewctrltool.h` | `legacy/tool/group/view/3dviewctrltool.h` |
| `legacy/tool/group/selecttool.h` | `legacy/tool/group/select/selecttool.h` |
| `legacy/tool/group/flashtool.h` | `legacy/tool/group/select/flashtool.h` |
| `legacy/tool/group/input*tool.h` / `appendfeaturetool.h` | `legacy/tool/group/input/…` |
| `legacy/tool/group/grouptoolfactory.h` | `legacy/tool/group/factory/grouptoolfactory.h` |
| `legacy/tool/group/defs.h` | unchanged |

## Verification

```bat
.\build.bat
```

Optional if `smt_build_app` already enabled:

```bat
.\build.bat legacy_app
```
