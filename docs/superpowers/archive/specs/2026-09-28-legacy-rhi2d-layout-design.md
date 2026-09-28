<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Legacy `rhi2d` layout (2D public + GDI impl; leftover → `rhi3d`)


> **Status: landed** (2026-09-28 merge). As-built under `src/legacy/render/`; living policy in umbrella §SP2. Do not revise here except mechanical link fixes.

**Date:** 2026-09-28  
**Status:** landed  
**Scope:** Collapse top-level `bridge/` + `gdi/` into `rhi2d/` (mirror `rhi3d/public` + `rhi3d/impl`); move all `leftover_*` under `rhi3d/public/bridge/`.  
**Plan:** [`../plans/2026-09-28-legacy-rhi2d-layout.md`](../plans/2026-09-28-legacy-rhi2d-layout.md)  
**Supersedes (layout only):** dual-run tops table rows for `bridge/` / `gdi/` in [`2026-09-27-legacy-render-subdirectory-dual-run-design.md`](2026-09-27-legacy-render-subdirectory-dual-run-design.md) §6.1–6.2. Dual-run policy, DLL stem, and present ownership unchanged.

## Goal

1. **`rhi2d/`** holds leftover **2D** abstract API under `public/device/` and the GDI(+) backend under `impl/gdi/` (same shape as `rhi3d/public` + `rhi3d/impl/{gl,d3d}`).
2. **`leftover_mesh` / `LeftoverRecorder` / `leftover_session`** live under **`rhi3d/public/bridge/`** (3D / shared strangler; not a 2D backend).
3. **Delete** top-level `bridge/` and `gdi/`. No old-path include shims.
4. Keep `dll_stem = legacy_render`, `CreateRenderDevice`, `smt_leftover_session` export names.

## Non-goals

- No algorithm / present-semantics changes.
- No Qt; no modern `src/render` → legacy deps.
- No rename of `Smt*` / LoadLibrary ABI strings.

## Target tree

```
src/legacy/render/
  rhi2d/
    BUILD.gn                          # public 2D source_set aliases
    public/device/                    # header-only (mirrors rhi3d/public/device/)
      renderdevice.h
      renderer.h
    detail/
      bind_rhi_present.cpp
      renderer.cpp
    impl/gdi/                         # former gdi/ (B′ subdirs kept)
      BUILD.gn README.md resource.h *.rc icons
      device/ thread/ buffer/ gdiaux/ carto/ test/
  rhi3d/
    public/
      device|resource|shader|texture|state|camera/
      bridge/
        leftover_mesh.* leftover_record.* leftover_session.*
        *_test.cc
    impl/gl/ impl/d3d/
  scene3d/
```

## Locked decisions

| # | Decision |
| --- | --- |
| 1 | Approach 1: `rhi2d/public/device` + `rhi2d/impl/gdi`; leftover → `rhi3d/public/bridge`. |
| 2 | Cancel tops `bridge/` and `gdi/`. |
| 3 | B′ colocated; includes use full new paths; no forwarding headers. |
| 4 | Aggregate GN remains `//src/legacy/render:legacy_render`. |
| 5 | Work on **master** only; commit only if user asks. |

## Include / GN map

| Old | New |
| --- | --- |
| `legacy/render/bridge/renderdevice.h` | `legacy/render/rhi2d/public/device/renderdevice.h` |
| `legacy/render/bridge/renderer.h` | `legacy/render/rhi2d/public/device/renderer.h` |
| `legacy/render/bridge/leftover_*.h` | `legacy/render/rhi3d/public/bridge/leftover_*.h` |
| `legacy/render/gdi/…` | `legacy/render/rhi2d/impl/gdi/…` |
| `//src/legacy/render/bridge:render_sources` | `//src/legacy/render/rhi2d:rhi2d_sources` |
| `//src/legacy/render/bridge:leftover_*` | `//src/legacy/render/rhi3d:leftover_*` (or `rhi3d/public/bridge` labels) |
| `//src/legacy/render/gdi:*` | `//src/legacy/render/rhi2d/impl/gdi:*` |

## Dependency notes

- `rhi2d/detail/bind_rhi_present.cpp` may depend on `rhi3d/public/bridge` (`leftover_record`).
- `rhi3d/impl/{gl,d3d}` may include `rhi2d/public/device/renderdevice.h` (already depended on former bridge 2D header).
- `impl/gdi` depends on `rhi2d` public + `rhi3d` leftover_record (unchanged semantics).
