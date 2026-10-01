<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Geochem analysis plugin — Implementation Plan

> **For agentic workers:** Implement task-by-task. Spec: [`../specs/2026-09-13-plugin-host-design.md`](../specs/2026-09-13-plugin-host-design.md) §geochem 地球化学分析.

**Goal:** Land `smartgis.geochem` with `native.geochem_stats` / `native.geochem_idw`, graded map overlay, **full-extent IDW heat raster**, stats panel, CSV + active-layer input, and `plugin.geochem` harness.

**Architecture:** Kernels in `src/gis/analysis/geochem/`; product package only loads, runs processing, and publishes via `GeochemWriter`.

**Tech Stack:** C++23, GDAL/OGR, Views markup, map2d writer seam.

## Global Constraints

- No Qt; Views + Skia only.
- No core algorithms under `src/plugin/`.
- Public namespaces ≤ two levels; functions `snake_case`.
- Gen roots `out/Debug` | `out/Release`; compile via `build.bat`.

## Tasks

### Task 1 — Kernels + catalog

- [x] `gis/analysis/geochem/{samples,stats,idw,grade}.{h,cc}`
- [x] Wire `ops_runner` catalog (`native.geochem_stats`, `native.geochem_idw`)
- [x] `analysis_geochem_test` + product `geochem_analyze_test`

### Task 2 — `product/geochem`

- [x] Package layout + `manifest/plugin.json` + markup dialog
- [x] `commands` / processing / `GeochemWriter` + `GeochemLayerReader` seams
- [x] Register in `PluginShell` + `app/views` deps

### Task 3 — Samples + harness + shell showcase

- [x] Mid-size CSV under `testing/data/plugin/geochem/`
- [x] `plugin.geochem` suite + `--plugin-showcase=geochem`
- [x] Browser installs writer + active-layer reader

### Task 4 — Docs

- [x] Living umbrella §geochem + product table row
- [x] This plan hung off plugin-host Active row

### Task 5 — Full-extent heat raster viz（2026-09-30）

- [x] IDW result exposes `min_value`/`max_value`; `geochem_heat_score` / `format_geochem_heat`
- [x] `commit_geochem` paints padded-bbox `geochem_heat_raster` (interpolate heat 0..100), not anomaly-only cells
- [x] `plugin.geochem` IL uses `cells:48`; capture `plugin-showcase-geochem.bmp`
