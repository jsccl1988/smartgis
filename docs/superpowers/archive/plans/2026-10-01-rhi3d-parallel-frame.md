<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# rhi3d leftover parallel frame Implementation Plan

**Status:** landed (archived 2026-10-03 — checkboxes complete)

> **For agentic workers:** implement task-by-task; checkbox tracking. Spec § in [`../../specs/2026-09-13-render-rhi-scene-design.md`](../../specs/2026-09-13-render-rhi-scene-design.md) (§rhi3d leftover parallel frame).  
> **Diagram:** [`../../diagrams/legacy-render-architecture.html`](../../diagrams/legacy-render-architecture.html)

**Goal:** Leftover GL/D3D HWND path: Phase 1 off-HWND FrameJob; Phase 2 in-frame CPU prep parallel; Phase 3 D3D11 deferred-context multi-thread record (GL stays P2). Homogeneous GL+D3D for P1+P2.

**Architecture:** Main stages/submits. One serial Render worker. P2 fans frustum cull. P3 (D3D only) binds deferred contexts per worker, records object draws, FinishCommandList + Execute on immediate.

**Tech Stack:** C++23, Win32, D3D11 deferred contexts, `base::execution` / resident prep workers, leftover `rhi3d` + `scene3d`.

## Global Constraints

- Work on **`master`** only.
- Freeze `Smt3DRenderDevice` virtuals and Create*/Release* exports (P3 uses C exports on `legacy_render_d3d`).
- HWND thread never `join`s the Render worker.
- No shared GL context Draw across threads; no FlyCube on leftover HWND.
- Comments English; helpers `snake_case`; types `PascalCase` in `render::detail`.
- **Do not** `git commit` unless the user asks.

## File map

| Path | Role |
| --- | --- |
| `rhi3d/impl/common/frame/*` | P1 scheduler + P2 prep runner |
| `rhi3d/impl/d3d/host/deferred_draw.*` | P3 deferred pool + TLS |
| `rhi3d/impl/d3d/ext/ext_interface.*` | Cross-DLL deferred C exports |
| `scene3d/scene/d3d_deferred_objects.h` | Partition visible objs |
| `scene3d/index/octree.cpp` · `scene/scene.cpp` | P2 cull + P3 deferred Render |
| `rhi3d/impl/{gl,d3d}/README.md` | Env notes |

## Tasks

### Task 1–2: FrameJob (P1) — done

- [x] Scheduler + stereo HWND wire-up

### Task 3: P2 CPU prep — done

- [x] PrepRunner + frustum cull

### Task 4: P3 D3D deferred

- [x] Deferred context pool + per-slot mesh CB + TLS `active_context`
- [x] C exports Begin/Bind/Finish; scene/octree partition draw
- [x] `SMT_RHI3D_D3D_DEFERRED=0` serial fallback; default on
- [x] `d3d_texture_test` deferred smoke PASS
- [x] Docs (living §, plan, README)

## Non-goals

- GL multi-thread Draw
- Tile-raster 3D RT
- FlyCube / `render::graph` contract change
- Merge leftover into `src/gpu`

