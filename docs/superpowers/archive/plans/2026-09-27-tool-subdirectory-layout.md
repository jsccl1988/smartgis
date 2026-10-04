<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/tool` subdirectory layout — Implementation Plan


> **Status: superseded** (2026-09-28 merge). Layout folded into tool-event-dispatch living. Do not revise here except mechanical link fixes.

> **For agentic workers:** Execute task-by-task on **master**. Do **not** `git commit` unless the user asks. Do **not** create branches. Spec: [`../specs/2026-09-27-tool-subdirectory-layout-design.md`](../specs/2026-09-27-tool-subdirectory-layout-design.md).

**Goal:** Split flat `src/tool/` into `command` / `interaction` / `draft` / `nav` / `workspace`; break includes to `tool/<module>/…`; keep `//src/tool:dispatch` aggregate. **Follow-up:** `adapter`/`legacy_msg` → `src/legacy/tool/adapter/` (`//src/legacy/tool/adapter:adapter`).

**Architecture:** Mechanical `git mv` + include rewrite + fine `source_set`s under root `group("dispatch")`. No behavior change. Adapter later parked under legacy (bridge only; not `legacy_tool` DLL).

**Tech Stack:** C++23, GN/`build.bat`, existing tool unit tests.

## Global Constraints

- No umbrella headers at `src/tool/*.h`
- `gestures` → `draft`; `camera_nav` → `nav`; `legacy_msg` → `legacy/tool/adapter`
- Still source_set / group, not DLL
- Namespace `tool` only; English comments; copyright 2026
- Update `src/tool/README.md` + Tool row in `docs/superpowers/src-layout.md` in same change

---

## Tasks

- [x] Task 1: Create module dirs; move/rename sources + tests; fix internal includes/header guards
- [x] Task 2: Rewrite all in-tree `#include "tool/….h"` to module paths (content/app/ui/plugin/legacy)
- [x] Task 3: Rewire `src/tool/BUILD.gn` (`:dispatch` group + module source_sets); rename `gestures_test` → `draft_test`
- [x] Task 4: Update `src/tool/README.md` + `docs/superpowers/src-layout.md` Tool row; mark plan/spec done pointers
- [x] Task 5: Build/run verification — all tool TUs compile; `camera_nav_test` PASS; `draft_test`/`tool_dispatch_test` link blocked by parallel `gis/datasource/gdal` WIP (follow-up)
- [x] Task 6: Relocate `adapter` to `src/legacy/tool/adapter/`; drop from `:dispatch`; update includes/docs/GN deps

## Include map (locked)

| Old | New |
| --- | --- |
| `tool/command.h` | `tool/command/command.h` |
| `tool/interaction.h` | `tool/interaction/interaction.h` |
| `tool/gestures.h` | `tool/draft/draft.h` |
| `tool/camera_nav.h` | `tool/nav/camera_nav.h` |
| `tool/workspace.h` | `tool/workspace/workspace.h` |
| `tool/legacy_msg.h` | `legacy/tool/adapter/legacy_msg.h` |
