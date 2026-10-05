<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Legacy render Pipeline + Arena Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Instrument all of `src/legacy/render` with `base::trace`, and deepen hot paths with `base::execution::Pipeline` + `base::memory` Arena — without breaking public `Smt*` signatures.

**Architecture:** Shared `detail/frame_pipeline.h` helpers; GDI feature prep becomes produce→map→sink Pipeline; rhi3d present/draw get categories; host VB/mesh scratch use Arena allocate; scene3d Update/Render/build_mesh traced. `Rhi2dFrameScheduler` coalesce lane stays.

**Tech Stack:** `base::trace` · `base::execution::Pipeline` · `base::allocate` / `STLAllocator` · leftover GDI/D3D/GL/scene3d

**Spec:** [`../specs/2026-09-13-render-rhi-scene-design.md`](../specs/2026-09-13-render-rhi-scene-design.md) §Legacy render Pipeline + Arena

## Global Constraints

- Stay on `master`; public `Smt*` virtuals frozen.
- Categories: `gdi.*` / `rhi3d.d3d.*` / `rhi3d.gl.*` / `scene3d.*` / `memory`.
- New comments English; new symbols `snake_case`.
- HDC play remains serial.

## Files

| Path | Role |
| --- | --- |
| `src/legacy/render/detail/frame_pipeline.h` | Frame-end memory sample + `[legacy.flow]` log helper |
| `…/gdi/paint/layer/painter.cc` | Prep → Pipeline |
| `…/gdi/worker/frame_worker.cc` | Sample memory + flow log after RenderMap |
| `…/d3d/host/device_present.cpp` | Begin/End/Swap spans |
| `…/d3d/…/vertex_buffer.cpp` | Host arrays via Arena |
| `…/gl/host/device_present.cpp` | Begin/End/Swap spans |
| `…/gl/…/vertex_buffer.cpp` | Host arrays via Arena |
| `…/scene3d/scene/scene.cpp` | Update/Render spans |
| `…/scene3d/dem/dem_height_field.cc` | build_mesh span + Arena scratch |
| `src/legacy/app/shell/dock/debug_console.*` | MFC Console dock (log_sink + debug_agent) |
| `src/legacy/app/shell/dock/render_trace.*` | MFC RenderTrace dock |
| `src/legacy/app/shell/frame/main_frame.cpp` | Wire Console + RenderTrace into AMBox |

---

### Task 1: Shared helper + GDI Pipeline prep

- [x] Add `legacy/render/detail/frame_pipeline.h`
- [x] Replace GDI `parallel_for` prep with `Pipeline` (3 stages)
- [x] `paint_once` calls `finish_legacy_frame_memory_sample()` + flow log
- [ ] `.\build.bat debug gdi_map_paint_test` green

### Task 2: rhi3d D3D/GL traces + VB Arena

- [x] `BASE_TRACE_EVENT` on Begin/End/SwapBuffers (d3d + gl)
- [x] Host float arrays: `base::allocate` / `deallocate`
- [x] Link already via `//src/base:base`

### Task 3: scene3d key paths

- [x] `Scene::Update` / `Render` spans under `scene3d`
- [x] `DemHeightField::build_mesh` span + Arena for `vert_of` scratch

### Task 4: Legacy Console + RenderTrace docks

- [x] `DebugConsoleDockBar` (log_sink, prefer `[legacy.flow]`)
- [x] Wire Console + RenderTrace into `InitAMBoxMgrDockBar`
- [x] Dep `//src/content:debug_agent` for Run submit

### Task 5: Verify

```bat
.\build.bat debug legacy_render
.\build.bat debug gdi_map_paint_test
.\build.bat debug SmartGis
```

- [ ] Builds green; Console shows flow lines when painting
