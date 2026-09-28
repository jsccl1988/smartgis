<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/legacy/tool` subdirectory layout — Implementation Plan

> **For agentic workers:** Landed on **master**. Spec: [`../specs/2026-09-27-legacy-tool-subdirectory-layout-design.md`](../specs/2026-09-27-legacy-tool-subdirectory-layout-design.md).

**Goal:** Nest leftover IATool ABI under `src/legacy/tool/iatool/`; keep `adapter/` and flat `group/`; break includes; keep aggregate GN labels.

**Architecture:** Mechanical `git mv` of root `t_*` / `tool_export.h` into `iatool/` + new `iatool/BUILD.gn` `source_set("tool_sources")` + root DLL deps that target. No behavior / `dll_stem` / `Smt_*` rename. Do not edit `src/legacy/render/**`.

**Tech Stack:** C++23, GN/`build.bat`, leftover MFC tool DLL + `ui_legacy` consumers.

## Global Constraints

- No shim headers at `legacy/tool/t_*.h` / `tool_export.h`
- `group/` stays flat; `adapter/` paths unchanged
- Aggregate `//src/legacy/tool:legacy_tool` / `:tool` / `:legacy_tool_all` stay
- No `Smt_*` / export macro / Notify ABI renames
- English comments; copyright 2026 on touched docs/BUILD
- Stay on **master**; no commit unless asked
- Out of scope: `src/legacy/render/**`, endgame `src/tool/<module>/` moves

---

## Tasks

- [x] Task 1: Create `iatool/`; `git mv` `t_iatool*` / `t_iatoolmanager*` / `t_msg*` / `tool_export.h`; add `iatool/BUILD.gn` with `source_set("tool_sources")`; fix internal includes/header guards
- [x] Task 2: Rewire root `src/legacy/tool/BUILD.gn` to `deps = [ "//src/legacy/tool/iatool:tool_sources" ]` (drop inline sources list)
- [x] Task 3: Rewrite all in-tree `#include "legacy/tool/t_….h"` / `tool_export.h` → `legacy/tool/iatool/…` (group, ui, app, plugins, self)
- [x] Task 4: Add `src/legacy/tool/README.md`; update `src/legacy/README.md`, `docs/build/src-layout.md` leftover/Tool rows, `docs/README.md` index; cross-link from endgame tool layout / strangler / dispatch path tables
- [x] Task 5: Verify — `ninja -C out legacy_tool` LINK OK; spec Status **landed**; spec+plan archived

## Include map (locked)

| Old | New |
| --- | --- |
| `legacy/tool/t_iatool.h` | `legacy/tool/iatool/t_iatool.h` |
| `legacy/tool/t_iatoolmanager.h` | `legacy/tool/iatool/t_iatoolmanager.h` |
| `legacy/tool/t_msg.h` | `legacy/tool/iatool/t_msg.h` |
| `legacy/tool/tool_export.h` | `legacy/tool/iatool/tool_export.h` |

## Verification

```bat
ninja -C out legacy_tool
```

Optional if `smt_build_app` / ui_legacy already in the graph: rebuild the targets that compile the rewritten TUs. Do not expand into `legacy/render` fixes.
