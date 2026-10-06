<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Plugin analysis present → processing — Implementation Plan

> **For agentic workers:** Implement task-by-task. Spec: [`../specs/2026-09-13-plugin-host-design.md`](../specs/2026-09-13-plugin-host-design.md) **§Analysis present in plugin**. Diagram: [`../diagrams/plugin-analysis-processing.html`](../diagrams/plugin-analysis-processing.html).

**Goal:** Move chrome `app/views/browser/plugin/analysis_writer_*` into product plugins. Callers keep `PluginShell::run_processing(id, args)`. Compute stays on the pool; present runs on the UI-thread done callback via `PluginHost` GIS/Scene3D seams. Delete `set_*_writer` globals.

**Architecture:** Invert the writer callback. Chrome keeps `plugin_shell` + generic `present_dataset` / ResultPlayback UI. Product packages own paint/mesh/commit.

## Global Constraints

- No Qt; Views + Skia only.
- Product TUs must not `#include` `app/views/...` except `plugin_shell` / Browser wiring of host bridges.
- Do not put kernels under `src/plugin/`.
- `PluginHost` ABI: append-only virtuals after `present_dataset`.
- Public namespaces ≤ two levels; functions `snake_case`.
- Stay on `master`. Gen roots `out/Debug` | `out/Release`.
- Showcase `--plugin-showcase=*` processing ids stay stable.

## Tasks

### Task 0 — Host seams (unlock)

- [x] `content::GisDocument` on `content/public` wrapping `MapScene` (layer / feature / style JSON / triangle mesh / extent). `PluginHost::gis_document()` append-only.
- [x] `PluginHost::scene3d_sink()` shell-installed facade (mesh / tileset / invalidate). No `Browser*` in product TUs.
- [x] `PluginHost::playback()` generic frame list; ResultPlayback UI ticks index only.
- [x] EventBus `document.layers_changed` replaces `refresh_ui_after_layer`.
- [x] `ProcessingPool`: factory compute on worker; **present** only in `done` on the thread that called `flush` / UI drain. Document the split in `processing.h`.
- [x] Tests: GisDocument smoke + processing compute-then-present without `set_*_writer`.

### Task 1 — Shared present helpers

- [x] Move `append_map_polygon` / `append_map_polyline` / `apply_style_json` / stand-in mesh off `analysis_writer_common` into `src/plugin/runtime/host/present/gis_present.*` (GisDocument only; Map2d + Scene3d).
- [x] Shell `present_plugin_map2d/scene3d` live under `runtime/plugin/present.*`; `present_dataset` bridge only for tab switch.

### Task 2 — Dual-run per product (one plugin per change)

Keep `set_*_writer` as a shim to the moved function until that plugin's showcase is green, then delete the shim.

- [x] `smartgis.geochem` — `present/` from `analysis_writer_geochem`
- [x] `smartgis.traffic` — path / blocks + playback frames
- [x] `smartgis.flood` — mask paint + flood playback
- [x] `smartgis.stormsurge` — mask + water mesh
- [x] `smartgis.mine` — stratum / volume mesh
- [x] `smartgis.world3d` orthogrid + hexgrid — from `analysis_writer_orthogrid`
- [x] `smartgis.world3d` scene / DEM / pointcloud — `Scene3dSink` earth bridges + `GisDocument`

### Task 3 — Playback

- [x] `Browser::install_plugin_host_bridges` binds `PluginPlayback` → `PluginHost::playback()` (`push_frame` / `frame_count` / `set_index`). No `set_playback` (Host owns the list).
- [x] Re-present is `"<id>.present_frame"` looked up from the contributing plugin (no chrome product switch).
- [x] Product payload stays in `src/plugin/product` (`g_last_*`). Chrome `runtime/analysis/*_store` deleted; `runtime/plugin/playback.*` sits next to `present.*`.

### Task 4 — Delete chrome writers

- [x] Remove `analysis_writer_*.cc/.h`, `analysis_writers.cc/.h`, `wire_plugin_analysis_writers` from `Browser::init`.
- [x] Remove remaining `set_*_writer` from product `commands.h` (world3d scene writer deleted; orthogrid/hexgrid live under world3d gis present).
- [x] `browser/plugin/` retains only `plugin_shell.*`.
- [x] `app/views` GN deps no longer list product writer TUs.
- [ ] `--plugin-showcase` matrix + `build.bat debug` plugin tests green.

### Task 5 — Docs

- [x] Living § + this plan + HTML diagram
- [x] §Chrome runtime: execution only — `contribute_export_frame`; drop `plugin_run` / `PluginShowcaseMode`; recursive harness leaf resolve; `plugin/product/builtins` façade (`PluginShell` opaque register)
- [ ] After Task 4 matrix green: mark this plan landed; archive; as-built note on the umbrella
