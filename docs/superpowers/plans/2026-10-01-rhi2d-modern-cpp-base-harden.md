<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# rhi2d modern C++ / base harden Implementation Plan

> **For agentic workers:** implement task-by-task; checkbox tracking. Spec § in [`../specs/2026-09-13-render-rhi-scene-design.md`](../specs/2026-09-13-render-rhi-scene-design.md) (§rhi2d modern C++ / base harden).

**Goal:** Harden leftover `rhi2d` like §rhi3d: loader/`FreeLibrary`/`LOGGING`, full-tree safe `nullptr`, paint hot-path `base` alloc + light modern C++. Freeze `RenderDevice2d` ABI.

**Architecture:** Built sources live under `impl/common/` + port `create_player` / players in `impl/{gdi,gdiplus,skia}/`. `detail/renderer.cpp` LoadLibrary by API name. Do not re-enable TLS POINT pools.

**Tech Stack:** C++23, Win32 GDI/GDI+, `base::allocate` / `LOGGING`, existing GN DLLs.

## Global Constraints

- Work on **`master`** only.
- Keep Create/Destroy export names and `RenderDevice2d` virtuals.
- Preserve `NULL_BRUSH` / `NULL_PEN` / `BS_NULL` / `GetStockObject(NULL_BRUSH)`.
- Comments English; new helpers `snake_case`.
- **Do not** `git commit` unless the user asks.

## File map

| Path | Role |
| --- | --- |
| `rhi2d/detail/renderer.cpp` | LoadLibrary / FreeLibrary / LOGGING (mirror rhi3d) |
| `rhi2d/impl/common/**` | Shared host + paint (primary build) |
| `rhi2d/impl/gdi/paint/player/*` + `create_player.cc` | GDI port |
| `rhi2d/impl/gdiplus/**` / `skia/**` | AA / Skia ports |
| `rhi2d/public/device/*` | Drive-by `nullptr` only |
| `rhi2d/impl/common/paint/carto/draw/points.h` | POINT scratch → `base::allocate` RAII |

## Tasks

### Task 1: Loader mirror §rhi3d

- [x] Rewrite `detail/renderer.cpp`: no `MessageBox`; `LOGGING(LOG_ERROR)`; `Release()` then load; fail-path `FreeLibrary`; `Release` destroys then unloads; `dll_stem` + fixed Destroy export (mirror rhi3d).
- [ ] `build.bat debug legacy_render` green.

### Task 2: Safe NULL → nullptr (full tree)

- [ ] Replace pointer `NULL` with `nullptr` under `rhi2d/` (common + ports + public + stale gdi mirrors).
- [ ] Skip Win32 stock/`BS_NULL` tokens.
- [ ] Compile `legacy_rhi2d_gdi` (at least).

### Task 3: Paint hot path

- [ ] `points.h`: `base::allocate` / `deallocate` (keep heap; document TLS ban).
- [ ] Hot draw/prep: `span` / range-for / `static_cast` where cheap; no behavior change.
- [ ] `gdi_map_paint_test` and/or `gdi_encode_test`.

### Task 4: Port hygiene + docs

- [ ] Same nullptr/ownership pass on `gdiplus` / `skia` players.
- [ ] Tick acceptance checkboxes on living `§`; refresh Active row date if needed.

## Done when

- Loader has no MessageBox; FreeLibrary on release/fail.
- Safe nullptr sweep done.
- POINT scratch uses `base::allocate`.
- Debug build of `legacy_render` + three `legacy_rhi2d_*` green; paint tests green or documented skip.
