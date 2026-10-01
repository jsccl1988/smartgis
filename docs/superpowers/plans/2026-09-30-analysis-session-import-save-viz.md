<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# AnalysisPlayback import / save / 2D�3D viz � Implementation Plan

> **For agentic workers:** Implement task-by-task. Spec: [`../specs/2026-09-13-plugin-host-design.md`](../specs/2026-09-13-plugin-host-design.md) �analysis session (import / save / playback).

**Goal:** Shared `AnalysisPlayback` + `ResultPlayback` for `traffic` / `flood` / `orthogrid` / `orthogrid3d`: sample import defaults, CommitLayer + explicit Export, controllable 2D/3D frame playback, PNG/BMP frame-sequence export.

**Architecture:** Approach B � chrome-owned session; plugins keep `run_processing` + writers; Browser applies frames and exports captures.

**Tech Stack:** C++23, Views shell, MapScene, CapabilityHost / interact DSL, `build.bat debug`.

## Global Constraints

- No Qt; no OSRM / full hydrology; no video encode this drop.
- Kernels stay in `gis/analysis`; session under `app/views/shell/runtime/`.
- Compile via `build.bat` + `out/.build.lock`; set `SMARTGIS_BUILD_OWNER`.

## Tasks

### Task 1 � Living � + session core

- [x] Append �analysis session to plugin-host living spec
- [x] Add `analysis_session.{h,cc}` (product kind, frames, paths, fps/loop)
- [x] Unit test: set_frame clamp + frame_count

### Task 2 � Browser Commit + Playback

- [x] Browser owns `AnalysisPlayback`; traffic/flood writers fill it
- [x] `apply_analysis_frame(i)` redraws current prefix/mask
- [x] `export_analysis_frames(dir)` ? captures + `playback.json`
- [x] CapabilityHost + DSL verbs: `analysis_set_frame`, `analysis_export_frames`

### Task 3 � Export commands + sample import

- [x] `traffic.export_path` / `flood.export_mask` (+ orthogrid export already) copy/verify last output
- [x] Dialogs default sample paths from `SMT_PLUGIN_SAMPLE_DIR` / `out/data/plugin`

### Task 4 � Orthogrid / 3d session + harness

- [x] Mesh/hex writers register single-frame (or iter) sessions
- [x] Orthogrid: capture Laplace + elliptic intermediate frames into AnalysisPlayback when `elliptic_iters > 0`
- [x] Extend `plugin.traffic` / `plugin.flood` / `plugin.orthogrid` .il with export_frames
- [ ] orthogrid3d remains single-frame (deferred)

### Task 4b � ResultPlayback chrome (P2)

- [x] Views `ResultPlaybackPanel` (play/pause/loop + scrub + prev/next) on inspector Playback tab
- [x] Wired to `Browser::apply_analysis_frame` + AnalysisPlayback fps/looping/playing
- [x] Fixed `Browser::pull_orbit_extent` AV (skip MapContents::Extent; use document extent)

### Task 5 � Verify

- [x] `analysis_playback_test` green; `SmartGisViews` link green
- [x] e2e `plugin.traffic` � exit 0, marks include `playback-ok`, captures under `out/Debug/captures/analysis_playback_traffic/` (12 frames + playback.json)
- [x] e2e `plugin.flood` � exit 0, marks `flood-ok`/`playback-ok`/`bmp-ok`/`pass`/`dsl-done`; `out/Debug/captures/analysis_playback_flood/` (6 BMPs + `playback.json`, product=flood)
- [x] e2e `plugin.orthogrid` � exit 0 after quiet out/ (killed `SmartGisViews_normalopen` LNK1168 locker); marks `orthogrid-ok`/`playback-ok`/`bmp-ok`/`pass`/`dsl-done`; `out/Debug/captures/analysis_playback_orthogrid/` (4 BMPs + `playback.json`, product=orthogrid, elliptic frames)
