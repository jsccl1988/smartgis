<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Plugin product package layout — Implementation Plan

> **For agentic workers:** Implement per domain; paths must not overlap. Spec: [`../specs/2026-09-14-plugin-subdir-layout-design.md`](../specs/2026-09-13-plugin-host-design.md) § Product domain package.

**Goal:** Reshape `src/plugin/product/{dem,print,model3d,orthogrid}` to `commands` + `manifest/plugin.json` + optional `views/` / `processing/` / `tests/` / `detail/`.

**Architecture:** Path-qualified snake_case files; GN `<domain>_views` + optional `<domain>_processing`; no include shims; stable plugin ids unchanged.

## Tasks

### Task A — dem (sample / fullest)

- Move loaders → `processing/{tin,grid}_loader.{h,cc}` (rename `*_core.cc`)
- Move dialogs → `views/`
- `dem_commands.*` → `commands.*`
- Test → `tests/loader_test.cc`; GN `dem_loaders` → `dem_processing`
- Add `manifest/plugin.json`
- Update all includes / deps (`dem_commands.h` → `commands.h`, `dem_loaders` → `dem_processing`, processing paths)

**Verify:** `build.bat src/plugin/product/dem:dem_loader_test` (or new test target name) + host_test still links.

### Task B — print

- `print_commands.*` → `commands.*`; dialog → `views/`; `manifest/plugin.json`
- Update call sites / GN `print_views` sources

### Task C — model3d

- `model3d_commands.*` → `commands.*`; `manifest/plugin.json`; update call sites / GN

### Task D — orthogrid

- Keep `commands.*` at root; add `manifest/plugin.json`
- Prefer leave `detail/` as-is this cycle (allowed by spec) OR move solvers under `processing/` if trivial
- Test stays under `detail/` or move to `tests/` if already touching

**Done when:** all four trees match template roles; `build.bat` dem test + `plugin_host_test` green; no stale `dem_commands` / `dem_loaders` / `print_commands` / `model3d_commands` includes under `src/`.

## Status (2026-09-28)

- [x] Task A dem — package + `dem_processing`; `dem_loader_test` / `plugin_host_test` ok
- [x] Task B print — [print agent](248c2a26-8473-48a3-a1c4-7c30c7deed7a)
- [x] Task C model3d — [model3d agent](f7064369-f946-4104-ba17-c2ec0a1d8f4c)
- [x] Task D orthogrid — [orthogrid agent](4e4c7163-e364-4d14-913c-acbccea58723)
