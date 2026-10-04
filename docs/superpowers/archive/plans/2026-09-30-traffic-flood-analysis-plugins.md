<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Traffic + flood analysis plugins — Implementation Plan

**Status:** landed (archived 2026-10-03 — checkboxes complete)

> **For agentic workers:** Implement task-by-task. Spec: [`../../specs/2026-09-13-plugin-host-design.md`](../../specs/2026-09-13-plugin-host-design.md) §traffic + flood analysis products.

**Goal:** Land `smartgis.traffic` and `smartgis.flood` product plugins with `native.cost_path` / `native.flood_fill` kernels and 2D/3D viz seams.

**Architecture:** Kernels in `src/gis/analysis/{network,raster}`; product packages only load data, call processing, and animate via writer callbacks. Catalog ids shared with `plugin::run_builtin_op` / Python `gis.analysis`.

**Tech Stack:** C++23, GDAL/OGR, Views markup, map2d/scene3d writers.

## Global Constraints

- No Qt; Views + Skia only.
- No core algorithms under `src/plugin/`.
- Public namespaces ≤ two levels; functions `snake_case`.
- Gen roots `out/Debug` | `out/Release`; compile via `build.bat`.

## Tasks

### Task 1 — Kernels + catalog

- [x] `gis/analysis/network/cost_path.{h,cc}` — OGR lines → graph → Dijkstra → GeoJSON path
- [x] `gis/analysis/raster/dem/flood_fill.{h,cc}` — GDAL DEM → inundation mask (+ optional frames)
- [x] Wire `ops_runner` catalog + `run_builtin_op` dispatch
- [x] Unit tests under product trees (`traffic_path_test` / `flood_inundate_test`)

### Task 2 — `product/traffic`

- [x] Package layout + `manifest/plugin.json` + markup dialogs
- [x] `commands` / processing / path writer seam
- [x] Register in `PluginShell` + `app/views` deps

### Task 3 — `product/flood`

- [x] Same template as traffic
- [x] Flood writer seam for frame animation
- [x] Register + deps

### Task 4 — Verify

- [x] `build.bat debug` traffic/flood tests + link `SmartGisViews`
- [x] Browser installs traffic/flood map2d+scene3d writers

