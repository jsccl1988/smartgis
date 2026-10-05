<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# rhi2d leftover tile-raster Implementation Plan

> **For agentic workers:** implement task-by-task; checkbox tracking. Spec § in [`../specs/2026-09-13-render-rhi-scene-design.md`](../specs/2026-09-13-render-rhi-scene-design.md) (§rhi2d leftover tile-raster).  
> **Diagram:** [`../diagrams/legacy-render-architecture.html`](../diagrams/legacy-render-architecture.html)

**Goal:** Expose Chromium-like **viewport tile** and **per-GIS-layer** parallel execute on leftover rhi2d so wall-clock can be compared (`RHI2D_PARALLEL`), without changing `RenderDevice2d` ABI or merging into `src/gpu`.

**Architecture:** HWND commits via `Rhi2dScheduler` (`NThreadPoolExecutor(1)` = Impl). Encode stays serial (no concurrent OGR). `Rhi2dTileGraphRunner` pulls jobs for tile or layer execute into private DIBs. Layer compose = ocean clear + ocean **color-key** (TransparentBlt). Tile size adapts toward ≤~4 tiles unless `RHI2D_TILE_SIZE` is set. Default mode **tile**.

**Tech Stack:** C++23, Win32, `render::detail`, existing `cc/` / `paint/map` / `surface/`, ports GDI·GDI+·Skia.

## Global Constraints

- Work on **`master`** only.
- Freeze Create/Destroy exports and `RenderDevice2d` virtuals.
- No per-job `PostTask` storm; resident workers + pull queue.
- No shared HDC across threads; no Vista on leftover HWND this plan.
- Comments English; helpers `snake_case`; types `PascalCase` in `render::detail`.
- **Do not** `git commit` unless the user asks.

## File map

| Path | Role |
| --- | --- |
| `impl/common/cc/tile_graph_runner.*` | Resident Raster×N, ready queue, barrier, cancel |
| `impl/common/cc/raster_tile.*` | Tile rect / outset / enumerate / `Rhi2dParallelMode` |
| `impl/common/cc/scheduler.*` | FrameJob Impl lane |
| `impl/common/paint/map/map_painter.*` | Mode dispatch serial / tile / layer |
| `impl/common/paint/carto/encode/command_encoder.*` | `execute` / `execute_tile` |
| `impl/common/surface/**` | DIB pool; blit |
| `impl/common/README.md` | Pipeline + modes |
| `impl/common/test/tile_raster_test.cc` | Stitch / damage / cancel / env / layer compose |

## Tasks

### Task 1–4: Tile path (landed baseline)

- [x] `cc/raster_tile.*` + `cc/tile_graph_runner.*`
- [x] FrameJob Impl + tile fan-out in `render_map`
- [x] Serial encode + `execute_tile` + abort/cancel
- [x] `tile_raster_test` stitch / damage / cancel; three ports build

### Task 5: Dual-mode + layer grain (compose = ocean color-key)

- [x] `RHI2D_PARALLEL=serial|tile|layer` (+ legacy `TILE_RASTER=0`)
- [x] Layer: seal per-GIS-layer command buffers → parallel `execute` → ocean color-key compose
- [x] Adaptive tile size (≤~4 tiles) + worker cap 8
- [x] `RHI2D_PARALLEL_LOG=1` prints execute wall-clock
- [x] Unit coverage for mode env + layer color-key compose
- [ ] PLP / showcase wall-clock compare notes (serial vs tile vs layer)

## Non-goals

- Geographic LOD / slippy cache
- GPU raster on leftover HWND
- Merge into `src/gpu` compositor
- Full dedicated Impl thread replacing `NThreadPoolExecutor(1)` (optional follow-up)
