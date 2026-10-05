<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# GDI core Chromium/cc rename Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Align `impl/gdi/core` internal names with Chromium/cc lexicon; split dual `Rhi2dFrameScheduler`; snake_case device members; keep `RenderDevice2d` ABI and CreateDevice string.

**Architecture:** Spec §GDI core cc rename in [`../specs/2026-09-13-render-rhi-scene-design.md`](../specs/2026-09-13-render-rhi-scene-design.md). Directories `host/paint/worker/surface` stay. Thin Draw*/Render* forwards on device.

**Tech Stack:** C++23, Win32 GDI, existing `legacy_render` GN targets.

## Global Constraints

- Work on **`master`** only; parallel agents use **non-overlapping paths** where possible.
- Keep `SmtRhi2dRenderDevice` / `SmtGdiRenderThread` / `CreateDevice("SmtRhi2dRenderDevice")`.
- Do **not** change `RenderDevice2d` virtuals (including `DrawPloygon` / `StrethImage`).
- Do **not** touch `src/legacy/render/gdi/` dual-run tree.
- Comments English; new helpers `snake_case`.
- **Do not** `git commit` unless the user asks.

## Rename map

| From | To |
| --- | --- |
| `GdiStyleCanvas` / `style_canvas.*` | `GdiPaintCanvas` / `paint_canvas.*` |
| `GdiMapPainter` / `map_painter.*` | `GdiLayerPainter` / `layer_painter.*` |
| host `Rhi2dFrameScheduler` / `host_frame_scheduler.*` | `GdiUiController` / `ui_controller.*` |
| worker `detail::Rhi2dFrameScheduler` / `worker_frame_scheduler.*` | `detail::GdiRasterScheduler` / `raster_scheduler.*` |
| `MapCarto2dFrame` / `map_carto2d.*` | `GdiCartoFrame` / `carto_frame.*` |
| `render_context.h` | `paint_context.h` (type `SmtRenderContext` stays) |
| `device.style_canvas_` / `map_painter_` | `paint_canvas_` / `layer_painter_` |
| `device.frame_scheduler_` / `scheduler()` | `ui_controller_` / `ui()` |
| `m_smtMapRenderBuf` / `m_smtQuickRenderBuf` / `m_smtDynamicRenderBuf` / `m_smtRenderBuf` | `map_front_` / `raster_back_` / `dynamic_buf_` / `compose_buf_` |
| `m_pRenderThread` / `m_virViewport1/2` / `m_shared_front_mu_` / `m_cslock` | `render_thread_` / `vir_viewport1_/2_` / `shared_front_mu_` / `lock_` |

## Parallel ownership

| Agent | Owns |
| --- | --- |
| Paint lane | `core/paint/**`, `test/map_carto2d*`, paint symbols in BUILD.gn; call-site type renames for paint types |
| Sched lane | `core/host/ui_controller.*`, `core/worker/raster_scheduler.*`, `render_thread.*`; scheduler call sites |
| Parent / integrate | `render_device.*` member snake + `ui()`, README, living §, final BUILD.gn, verify tests |

---

### Task 1 (paint lane): paint type/file rename

- [x] Move/rename files; update include guards; rename types.
- [x] Update all in-tree references under `impl/gdi/` (and tests).
- [x] `build.bat debug map_carto2d_test` green (or compile paint objs).

### Task 2 (sched lane): UiController + RasterScheduler

- [x] Rename host/worker scheduler files + types (`render::GdiUiController` vs `detail::GdiRasterScheduler`).
- [x] Update `render_thread` / map painter forward decls / set_scheduler param type.
- [x] Device: `scheduler()` → `ui()` only if integrating; else leave for Task 3.

### Task 3 (integrate): device members + docs + verify

- [x] Snake member rename on `SmtRhi2dRenderDevice`; section comments Present/Schedule/ABI.
- [x] README + living § naming table; Active plan link.
- [x] Force-rebuild GDI objs; `gdi_map_paint_test` / `gdi_compose_test` / `map_carto2d_test` from `out/Debug`.

## Done when

- No dual `Rhi2dFrameScheduler` type name.
- Paint types use cc lexicon names.
- ABI / CreateDevice unchanged; GDI tests green.
