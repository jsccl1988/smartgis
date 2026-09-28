<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Plugin subdirectory layout (L1)

**Date:** 2026-09-14; **relocated leftover:** 2026-09-27 (`src/plugin/legacy` → `src/legacy/plugin`); **runtime/product split:** 2026-09-27; **AM adapter:** 2026-09-27 (`plugin/runtime/host/legacy_{am,cmd}` → `legacy/plugin/adapter`); **orthogrid kernel:** 2026-09-27 (`product/orthogrid` 2010 class → `legacy/plugin/orthogrid/kernel`)  
**Status:** accepted  
**Legacy half (2026-09-27):** physical tree under `src/legacy/plugin` is **as-built / frozen** — see [`../archive/specs/2026-09-27-legacy-plugin-subdirectory-layout-design.md`](../archive/specs/2026-09-27-legacy-plugin-subdirectory-layout-design.md) (landed; no further nesting of AuxModule root or domain `dlg_*` in that freeze).  
**Choices:** L1 + I1 + nested domain shells under `legacy/plugin/<domain>` (not sibling `legacy_XXX`). No include shims. Endgame tree splits **runtime** (host, processing, widgets, python, samples) from **product** domains. Public namespace stays `plugin`.

## Target tree

```
src/plugin/
  BUILD.gn                 # group re-exports (:host :plugin :cmd :legacy_cmd :processing)
  runtime/                 # host, processing pool, widgets, python embed
    host/                  # Registry, store, ProcessingPool, ManagerView
    processing/            # builtin GeoJSON ops + Views processing panel
    widgets/
    python/
    samples/
  product/                 # domain Views (+ kernels). Stable plugin ids unchanged.
    dem/ proj/ print/ model3d/ orthogrid/   # Views + Laplace only (no 2010 Orthogrid class)

src/legacy/plugin/         # leftover AuxModule + domain MFC shells
  module* / plugin_msg*
  adapter/                 # am / cmd source_sets (not the AuxModule DLL)
  dem/ proj/ print/ model3d/ orthogrid/
    orthogrid/kernel/      # 2010 Orthogrid class (grid/curve/region/polyline)
```

Depth matches `src/legacy/plugin/<domain>`: one bucket under `plugin/`, then the module. Not a third public namespace.

## Includes (I1)

| Old | New |
| --- | --- |
| `plugin/registry.h` | `plugin/runtime/host/registry.h` |
| `plugin/host/…` | `plugin/runtime/host/…` |
| `plugin/processing/…` `plugin/widgets/…` `plugin/python/…` | `plugin/runtime/…` |
| `plugin/<domain>/…` (dem, proj, print, model3d, orthogrid) | `plugin/product/<domain>/…` |
| `plugin/runtime/host/legacy_am.h` | `legacy/plugin/adapter/am.h` |
| `plugin/runtime/host/legacy_cmd.h` | `legacy/plugin/adapter/cmd.h` |
| `plugin/module.h` | `legacy/plugin/module.h` |
| `plugin/dem/dlg_*.h` | `legacy/plugin/dem/dlg_*.h` |
| `plugin/product/orthogrid/{grid,curve,region,types,orthogrid}.h` and `detail/polyline.h` | `legacy/plugin/orthogrid/kernel/…` (no product shim) |

## GN

- `//src/plugin/runtime/host:host` (root `//src/plugin:host` re-exports)
- `//src/plugin/runtime/processing:processing_views` (root `//src/plugin:processing` re-exports)
- `//src/plugin/product/<domain>:*_views` (and `dem_loaders`). `orthogrid_views` keeps commands + `detail/laplace_solver` + `detail/boundary_solve`.
- `//src/legacy/plugin/orthogrid:orthogrid_kernel` — 2010 `Orthogrid` (`CreateOrthGrid` / `CvtToGrid` / `LoadGridBndFromFile`). `plugin_orthogrid` links this target. No shim headers under `plugin/product/orthogrid`.
- `//src/legacy/plugin:plugin` (AuxModule DLL)
- `//src/legacy/plugin/adapter:am` and `:cmd` (source_sets; root `//src/plugin:cmd` re-exports `:cmd`, `:legacy_cmd` alias; host deps `:am`)
- `//src/legacy/plugin/dem:plugin_dem` (and proj/print/model3d/orthogrid)

## Orthogrid kernel (2010 class)

Product `orthogrid/` keeps the Views registration (`commands.*`), Eigen Laplace (`detail/laplace_solver.*`), and `detail/boundary_solve` (gridbnd load + solve without the 2010 class). The 2010 `Orthogrid` implementation (`grid`, `grid_create`, `grid_modify`, `curve`, `region`, `detail/polyline`, and the headers they own) lives under `src/legacy/plugin/orthogrid/kernel/` so `plugin_orthogrid` still links `CreateOrthGrid` / `CvtToGrid` / `LoadGridBndFromFile`. PascalCase on that class stays. Do not move the other product domains (`dem`, `proj`, `print`, `model3d`) into `legacy/`.

## Non-goals

- Do not rename stable plugin ids.
- Do not put product domains back under `runtime/`.
