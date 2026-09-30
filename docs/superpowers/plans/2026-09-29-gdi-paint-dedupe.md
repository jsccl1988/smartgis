<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# GDI paint dedupe (`core/paint/`) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** One shared Draw*/style/layer implementation under `core/paint/`; host keeps UI-thread sync paint via a separate canvas/painter instance; delete duplicate host `GeomDrawer` / `StyleState` / `LayerPainter` bodies.

**Architecture:** Spec §GDI paint dedupe in [`../specs/2026-09-13-render-rhi-scene-design.md`](../specs/2026-09-13-render-rhi-scene-design.md). Share code, not instances. Worker FrameJob cancel/gen/publish unchanged.

**Tech Stack:** C++23, Win32 GDI, existing `legacy_render` / GN `source_set`s.

## Global Constraints

- Work on **`master`** only; parallel agents use **non-overlapping paths**.
- Keep `SmtGdiRenderDevice` / `SmtGdiRenderThread` / CreateDevice string + public `Draw*` / `Render*` names.
- UI-thread sync paint must remain (BeginRender path + realtime fallback).
- Comments in English on touched code; new functions `snake_case`.
- **Do not** `git commit` unless the user asks.
- Do not merge leftover into `gpu/` compositor in this change.

## File map (after)

| Path | Role |
| --- | --- |
| `core/paint/style_canvas.*` | Sole style + Draw* |
| `core/paint/map_painter.*` | Sole map/layer/feature/geometry orchestration |
| `core/paint/device_geom.h` `render_context.h` `map_carto2d.*` | Paint helpers + carto |
| `core/host/render_device.*` | Facade; owns host `GdiStyleCanvas` + `GdiMapPainter`; host-only compose/realtime |
| `core/host/host_frame_scheduler.*` `image_io.*` | Unchanged roles |
| `core/worker/render_thread.*` `worker_frame_scheduler.*` | FrameJob shell only |
| **Deleted** | `host/geom_drawer.*` `host/style_state.*` `host/layer_painter.*` |

---

### Task 1: move paint sources → `core/paint/`

- [x] **Step 1:** Move worker `{style_canvas,map_painter,device_geom,render_context,map_carto2d}.*` → `core/paint/` (untracked files used filesystem move).
- [x] **Step 2:** Update includes inside moved files; `map_painter.h` forward-declares `GdiFrameScheduler` (include `worker_frame_scheduler.h` only in `.cpp`) so paint does not pull worker into public headers.
- [x] **Step 3:** Update `BUILD.gn` (common + thread + carto test), all in-tree includes, README layout table.
- [x] **Step 4:** `build.bat debug` `map_carto2d_test` green.

### Task 2: host owns paint instance; delete duplicate TUs

- [x] **Step 1:** `SmtGdiRenderDevice` owns `detail::GdiStyleCanvas` + `detail::GdiMapPainter` (separate from worker). Construct painter with host back (`m_smtQuickRenderBuf` or dedicated) + shared front `m_smtMapRenderBuf` + vir viewports + `shared_front_mutex`.
- [x] **Step 2:** `BeginRender` / `EndRender` / `PrepareForDrawing` / `EndDrawing` bind host canvas DC + lock_style; remove `GdiStyleState`.
- [x] **Step 3:** Public `Draw*` → host canvas; `RenderLayer`/`Feature`/`Geometry` → host map_painter (DC already set). Full sync `RenderMap(map)` / realtime fallback → `map_painter.render_map(...)` (same prep/play as worker). Host-only: move `render_map_to_dc` / `re_render_map_by_proxy` / `re_render_map_real_time` into `render_device.cpp` (or tiny host helper file if size warrants).
- [x] **Step 4:** Delete `geom_drawer.*` `style_state.*` `layer_painter.*`; drop friends; `style()` → `canvas()` (or equivalent accessors). Update `BUILD.gn`.
- [x] **Step 5:** `build.bat debug` `gdi_map_paint_test` + `gdi_compose_test` + `map_carto2d_test` green (force-rebuild GDI objs; run paint test from `out/Debug` so `legacy_render_d.dll` resolves).

### Task 3: docs + living §

- [x] **Step 1:** Living §GDI paint dedupe accepted; layout table lists `core/paint/`.
- [x] **Step 2:** `impl/gdi/README.md` + Active table plan link.
- [x] **Step 3:** Mark this plan checkboxes done when green.

## Done when

- No second Draw*/style/layer body under `host/`.
- Host sync paint still works; FrameJob path unchanged in contract.
- GDI tests / `legacy_render` build green.
