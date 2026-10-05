<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Storm surge 3D disaster — Implementation Plan

> **For agentic workers:** Implement task-by-task. Spec: [`../specs/2026-09-13-plugin-host-design.md`](../specs/2026-09-13-plugin-host-design.md) §stormsurge 3D disaster（P1–P3）. **Land P1 only** in the next code cycle; P2/P3 sections stay unchecked follow-ons.

**Goal:** Land `smartgis.stormsurge` product plugin with DEM+coast+tide → mask/depth/polygons kernel and map2d/scene3d viz (mask-on-DEM first, then water-surface mesh).

**Architecture:** Approach 1 — kernels in `src/gis/analysis/coastal/` (or `storm_surge/`); product package only loads data, calls processing, and commits via `set_stormsurge_writer`. Catalog id `native.storm_surge` shared with `plugin::run_builtin_op` / Python `gis.analysis`. Do **not** fold into `flood`.

**Tech Stack:** C++23, GDAL/OGR, Views markup, map2d/scene3d writers.

## Global Constraints

- No Qt; Views + Skia only.
- No core algorithms under `src/plugin/`.
- Public namespaces ≤ two levels; functions `snake_case`.
- Gen roots `out/Debug` | `out/Release`; compile via `build.bat`.
- Stay on `master`; no new dated design specs — revise living plugin-host umbrella only.
- Full hydrodynamic engine is out of scope for P1.

## Tasks (P1 — implement)

### Task 1 — Coastal / storm_surge kernel + catalog + unit tests

- [x] `gis/analysis/raster/dem/storm_surge.{h,cc}` — DEM + coast + tide series → inundation mask / optional depth / optional polygons (peer of `flood_fill`; shared `flood_connected_at_level`)
- [x] Wire catalog: `native.storm_surge` (+ `ops_runner` / `run_builtin_op`)
- [x] Unit tests for synthetic DEM+coast+tide → non-empty mask (and optional depth/polygon smoke)

### Task 2 — `product/stormsurge` package + dialogs + commands

- [ ] Package at `src/plugin/product/stormsurge/` (`smartgis.stormsurge`): `manifest` + markup dialogs + `commands` / `processing` / `views` / `resources` / `tests`
- [ ] Commands: `stormsurge.run`, `stormsurge.export`, `stormsurge.load_coast`, `stormsurge.about`
- [ ] Register in `PluginShell::start_builtins`; `app/views` deps

### Task 3 — Samples + harness

- [ ] Synthetic fixtures under `testing/data/plugin/` (tiny DEM + coast + tide series); GN copy to `out/data/plugin/`
- [ ] Optional China coastal showcase path documented when real coastal assets exist
- [ ] Harness `plugin.stormsurge` interact/suite (marks / non-empty BMP)

### Task 4 — Browser writer (mask then water mesh)

- [x] Browser installs `set_stormsurge_writer` in `Browser::init` (map2d + scene3d)
- [x] Viz order: **mask-on-DEM first**, then **water-surface mesh**
- [x] Unset writer → structured `no_stormsurge_seam` JSON
- [x] Real wet-cell free-surface triangle mesh (replaces raised-plane standin)
- [x] Water mesh also commits via `Scene3dPresenter::set_overlay_tin_mesh` (peer of mine stratum)
- [x] ResultPlayback frame scrub re-pushes water TIN (per-frame mesh store on `AnalysisPlayback`)
- [x] Chrome Scene3D tab switch hardened (atmosphere before FlyCube attach; stereo `abandon` under FlyCube)

### Task 5 — Verify

- [x] `build.bat debug` stormsurge / coastal kernel tests + link `SmartGisViews`
- [x] `--plugin-showcase=stormsurge` Scene3D present path green (Null RHI default; GPU HWND via `PLUGIN_STORMSURGE_GPU=1`)
- [ ] Optional: GPU HWND BMP signal probe when FlyCube chrome tab is exercised under `PLUGIN_STORMSURGE_GPU=1`
---

## Follow-on — P2 (stats)

- [x] Inundation **area** and **depth-class** summary tables / layers
- [x] Buffer / overlap stats vs asset or admin polygons
- [x] Export summary JSON alongside mask/depth

Catalog: **`native.storm_surge_stats`**. Product: **`stormsurge.stats`** (also optional `stats_output` on `stormsurge.run`).

## Follow-on — P3 (import + domain; do not implement in P1/P2 cycle)

- [ ] NetCDF / grid water-surface import path into the same writer seam
- [ ] Optional simplified tide / wind drivers (not a full hydrodynamic engine)
- [ ] Optional `DomainKind::kStormSurge` hook under `vista/domain` (Effect / session later; document seam only until render domain work opens)
