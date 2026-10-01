<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Mine / stratum earthwork — Implementation Plan

> **For agentic workers:** Implement task-by-task. Spec: [`../specs/2026-09-13-plugin-host-design.md`](../specs/2026-09-13-plugin-host-design.md) §mine / stratum（矿山三维地层）.

**Goal:** Land `smartgis.mine` product plugin with borehole → stratum TIN → coarse prism volume kernels and stick/TIN viz seams.

**Architecture:** Approach 1 — kernels in `src/gis/analysis/geology/`; product package only loads CSV, calls processing, and commits via `set_mine_stratum_writer`. Catalog ids shared with `plugin::run_builtin_op` / Python `gis.analysis`. Do **not** fold into `world3d`.

**Tech Stack:** C++23, Views markup, map2d/scene3d writers.

## Global Constraints

- No Qt; Views + Skia only.
- No core algorithms under `src/plugin/`.
- Public namespaces ≤ two levels; functions `snake_case`.
- Gen roots `out/Debug` | `out/Release`; compile via `build.bat`.
- Stay on `master`; no new dated design specs.

## Tasks

### Task 1 — Kernels + catalog + unit tests

- [x] `gis/analysis/geology/` — `load_boreholes_csv`, `interpolate_stratum_tin`, `prism_volume_between`
- [x] Wire catalog: `native.stratum_interpolate`, `native.stratum_prism_volume` (+ `ops_runner` / `run_builtin_op`)
- [x] Unit tests for CSV load, TIN interpolate, prism volume

### Task 2 — `product/mine` package + dialogs + writer

- [x] Package at `src/plugin/product/mine/` (`smartgis.mine`): `manifest` + markup dialogs + `commands` / `processing` / `views` / `resources` / `tests`
- [x] Commands: `mine.load_boreholes`, `mine.interpolate_stratum`, `mine.prism_volume`
- [x] Mine stratum writer seam (borehole sticks + TIN surfaces MVP)

### Task 3 — Samples + harness + PluginShell/Browser register

- [x] Sample `testing/data/plugin/mine_boreholes.csv` (GN copy to `out/data/plugin/`)
- [x] Harness `plugin.mine` interact/suite
- [x] Register in `PluginShell::start_builtins`; Browser `set_mine_stratum_writer` in `Browser::init`

### Task 4 — Verify

- [x] `build.bat debug` mine/geology tests + link `SmartGisViews`
