<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# rhi2d Chromium-cc compose Implementation Plan

> **For agentic workers:** implement task-by-task; checkbox tracking. Spec § in [`../specs/2026-09-13-render-rhi-scene-design.md`](../specs/2026-09-13-render-rhi-scene-design.md) (§rhi2d Chromium-cc compose).

**Goal:** Introduce `impl/common/cc/` (LayerTreeHost / LayerTreeImpl / Scheduler) mapped to Chromium commit→activate→draw, then deepen retire/damage — without changing `SmtRenderDevice` ABI.

**Architecture:** Shared host stays under `common/`. New `cc/` owns scheduling and tree activate. Paint + surface keep current roles. Ports remain thin LoadLibrary players.

**Tech Stack:** C++23, Win32 GDI leftover, `base::execution::NThreadPoolExecutor(1)`, `surface/composer` free functions.

## Global Constraints

- Work on **`master`** only.
- Freeze Create/Destroy exports and `SmtRenderDevice` virtuals.
- Single serial GDI worker; no multi-raster.
- Comments English; helpers `snake_case`; types `PascalCase` in `render::detail`.
- **Do not** `git commit` unless the user asks.

## File map

| Path | Role |
| --- | --- |
| `impl/common/cc/scheduler.*` | Serial FrameJob lane (ex-`worker/frame_scheduler`) |
| `impl/common/cc/layer_tree_impl.*` | Active tree + gen / damage / retire |
| `impl/common/cc/layer_tree_host.*` | Commit + schedule; wires activate→draw |
| ~~`impl/common/worker/*`~~ | Removed (Host owns scheduler; no forward shim) |
| `impl/{gdi,gdiplus,skia}/` | Player + create_player only |

## Tasks

### Task 1: `cc/` scheduler + tree skeleton

- [x] Add `cc/scheduler.*` (`Rhi2dScheduler`)
- [x] Add `cc/layer_tree_impl.*` + `cc/layer_tree_host.*`
- [x] Point BUILD.gn at `cc/`; delete `worker/` forward shim (include `cc/scheduler.h`)
- [x] `build.bat debug legacy_rhi2d_gdi` green

### Task 2: Wire FrameWorker / Present through host

- [x] `Rhi2dFrameWorker` owns `Rhi2dLayerTreeHost`; commit/schedule APIs
- [x] Worker paint path: activate then draw; abort retires without publish
- [x] Behavior-equivalent with prior FrameScheduler

### Task 3: Docs + cleanup

- [x] Update `impl/common/README.md` pipeline table
- [x] Confirm port-only `impl/gdi` (no shared-tree mirror)
- [x] Deduplicate backend shims if still duplicated (single `backend/` remains)

### Task 4: Phase 2 retire / damage

- [x] Explicit commit / activate / draw boundaries + gen contract tests (`gdi_cc_test`)
- [x] Damage rect API (full-frame default; partial via `commit(rc, damage, false)`)
- [x] encode / compose / cc tests green; map_paint functional steps green (teardown FreeLibrary may hang on leaked executor — pre-existing leftover)

### Task 5: Concept collapse (C)

- [x] Merge `FrameWorker` into `LayerTreeHost` (device holds `layer_tree_host_`)
- [x] Delete `FrameSink`; publish uses `submit_surface` only
- [x] Remove unused `cc/Layer` list + `backend/` shims; README three-axis model
- [x] Delete `worker/` forward shim; painter includes `cc/scheduler.h` directly
- [x] Thin `surface/`: merge blend/frame into `composer/` free fns; Owned = DIB lifecycle only; `present_to_hwnd` / `blit_owned_to`
- [x] Thin `paint/`: encode+carto_draw → `carto/`; layer → `map/`; flatten `draw_*`
- [x] Split `paint/carto/` into equal peers `encode/` · `draw/` · `frame/` · `style/` (no root façade / no shim)
