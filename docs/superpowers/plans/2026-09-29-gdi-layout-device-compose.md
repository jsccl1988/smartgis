<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# GDI layout rename + device composition Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Under `src/legacy/render/rhi2d/impl/gdi/`, drop redundant `gdi_` file prefixes, move RC assets into `res/`, split `SmtGdiRenderDevice` into a thin facade plus composed helpers, then organize as `core/{host,worker,surface}` + `gdiaux/` + `res/` + `test/` ? without changing leftover ABI.

**Architecture:** Spec ?GDI layout rename + device composition in [`../specs/2026-09-13-render-rhi-scene-design.md`](../specs/2026-09-13-render-rhi-scene-design.md). File rename only; `SmtGdi*` types stay.

**Tech Stack:** C++23, Win32 GDI, existing `legacy_render` DLL / GN `source_set`s.

## Global Constraints

- Work on **`master`** only; parallel agents use **non-overlapping paths**.
- Scope: **`rhi2d/impl/gdi/`** only.
- Keep `SmtGdiRenderDevice` / `SmtGdiRenderThread` / CreateDevice string.
- Keep FrameJob / cancel / present-on-gen behavior from leftover-worker plan.
- Comments in English on touched code.
- **Do not** `git commit` unless the user asks.
- Windows forbids path segment `aux/` ? keep `gdiaux/`.

## File map (after)

| Path | Role |
| --- | --- |
| `core/surface/compose.*` `surface_pool.*` | DIB compose / buffers |
| `core/host/render_device.*` | Facade `SmtGdiRenderDevice` |
| `core/host/style_state.*` `geom_drawer.*` `layer_painter.*` `image_io.*` | UI-thread collaborators |
| `core/host/host_frame_scheduler.*` | UI stage/Timer (`render::GdiFrameScheduler`) |
| `core/worker/worker_frame_scheduler.*` `map_painter.*` `style_canvas.*` | Worker FrameJob |
| `core/worker/render_thread.*` | Thin `SmtGdiRenderThread` facade |
| `core/worker/map_carto2d.*` | Map carto 2D |
| `gdiaux/aux_api.*` `gdiplus.*` | GDI+ helpers |
| `res/` | Win32 resources |
| `test/` | `compose_test` `map_paint_test` `map_carto2d_test` |

---

### Task 1?2: rename + device composition

- [x] Completed in prior wave.

### Task 3: subdir tighten + `core/{host,worker,surface}`

- [x] Flatten former `device/`+`thread/`+`buffer/`+`carto/` into `core/`, then split `host/` ? `worker/` ? `surface/`.
- [x] Update includes / `BUILD.gn` / README / living ?.
- [x] `build.bat debug` GDI targets green; `gdi_compose_test` / `map_carto2d_test` run ok.

## Done when

- Layout matches living ?; facade + collaborators; tests/build green for GDI targets.
