<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

> **Status: superseded** (2026-09-28 merge B). Merged into [`../../specs/2026-09-13-plugin-host-design.md`](../../specs/2026-09-13-plugin-host-design.md) — §Plugin subdirectory layout (folded). Do not revise here except mechanical link fixes; revise the living umbrella in place.


# Plugin subdirectory layout (L1)

**Date:** 2026-09-14; **relocated leftover:** 2026-09-27 (`src/plugin/legacy` → `src/legacy/plugin`); **runtime/product split:** 2026-09-27; **AM adapter:** 2026-09-27 (`plugin/runtime/host/legacy_{am,cmd}` → `legacy/plugin/adapter`); **orthogrid kernel:** 2026-09-27 (`product/orthogrid` 2010 class → `legacy/plugin/orthogrid/kernel`); **product package roles:** 2026-09-28  
**Updated:** 2026-09-28  
**Status:** superseded (2026-09-28 merge B)
**Legacy half (2026-09-27):** physical tree under `src/legacy/plugin` is **as-built / frozen** — see [`../archive/specs/2026-09-27-legacy-plugin-subdirectory-layout-design.md`](2026-09-27-legacy-plugin-subdirectory-layout-design.md) (landed; no further nesting of AuxModule root or domain `dlg_*` in that freeze).  
**Choices:** L1 + I1 + nested domain shells under `legacy/plugin/<domain>` (not sibling `legacy_XXX`). No include shims. Endgame tree splits **runtime** (host, processing, widgets, python, samples) from **product** domains. Public namespace stays `plugin`. Product domains use a **package** layout (`manifest` / `views` / `processing` / `tests`) with global file naming (`docs/superpowers/src-layout.md` File naming).

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
  product/                 # domain packages. Stable plugin ids unchanged.
    dem/ print/ model3d/ orthogrid/
      # see § Product domain package (below). product/proj removed.

src/legacy/plugin/         # leftover AuxModule + domain MFC shells
  module* / plugin_msg*
  adapter/                 # am / cmd source_sets (not the AuxModule DLL)
  dem/ proj/ print/ model3d/ orthogrid/
    orthogrid/kernel/      # 2010 Orthogrid class (grid/curve/region/polyline)
```

Depth: `plugin/{runtime|product}/…` then domain (and optional role dirs). Not a third public namespace.

## Includes (I1)

| Old | New |
| --- | --- |
| `plugin/registry.h` | `plugin/runtime/host/registry.h` |
| `plugin/host/…` | `plugin/runtime/host/…` |
| `plugin/processing/…` `plugin/widgets/…` `plugin/python/…` | `plugin/runtime/…` |
| `plugin/<domain>/…` (dem, print, model3d, orthogrid) | `plugin/product/<domain>/…` (plus role subdir when present) |
| `plugin/product/<domain>/dem_commands.h` | `plugin/product/<domain>/commands.h` (after package cutover) |
| `plugin/runtime/host/legacy_am.h` | `legacy/plugin/adapter/am.h` |
| `plugin/runtime/host/legacy_cmd.h` | `legacy/plugin/adapter/cmd.h` |
| `plugin/module.h` | `legacy/plugin/module/module.h` |
| `plugin/dem/dlg_*.h` | `legacy/plugin/dem/views/dlg_*.h` |
| `plugin/product/orthogrid/{grid,curve,region,types,orthogrid}.h` and `detail/polyline.h` | `legacy/plugin/orthogrid/kernel/…` (no product shim) |

## Product domain package

Locked (approach **3**, naming per `docs/superpowers/src-layout.md` File naming). One role directory under `product/<domain>/` max for each concern. **No** `include/` + `src/` split. Headers stay beside `.cc` (colocated).

### Template

```
src/plugin/product/<domain>/
  BUILD.gn
  commands.h / commands.cc     # register_<domain>(PluginHost*); package entry
  <stem>_export.h              # only if DLL export macros need a short qualifier
  manifest/
    plugin.json                # store Manifest shape; id/name/version/api_version/kind + contributes
  views/                       # dialogs / pages only (omit if none)
    *_dialog.h / .cc
    *_page.h / .cc
  processing/                  # MFC-free kernels + processing backends (omit if none)
    <unit>.h / <unit>.cc       # stem matches header (no *_core.cc)
  tests/                       # domain tests (omit if none)
    <unit>_test.cc             # or role-scoped stem, e.g. loader_test.cc
  detail/                      # optional private helpers (orthogrid Laplace today)
```

### Naming rules

| Item | Rule |
| --- | --- |
| Directory | `product/<domain>/` — short snake (`dem`, `print`, `model3d`, `orthogrid`) |
| Files | `snake_case` + `.cc` / `.h`. Path already names the domain → **no** `dem_` / `print_` file prefix on entry/commands |
| Entry | `commands.h` / `commands.cc` + `bool register_<domain>(…)` (symbol may keep domain; filename does not) |
| Manifest wire | always `manifest/plugin.json` (same shape as `runtime/samples/*/plugin.json`) |
| Export header | keep a short qualifier when needed (`dem_export.h`), not bare `export.h` |
| GN | `source_set("<domain>_views")` = `commands.*` + `views/*`; `source_set("<domain>_processing")` = `processing/*` when present; `test("<domain>_*_test")` with sources under `tests/` |
| Include | `"plugin/product/<domain>/commands.h"`, `"plugin/product/<domain>/views/…"`, `"plugin/product/<domain>/processing/…"` — full path from `//src`, no shims |
| Namespace | `plugin` only (internals → `plugin::detail` or anonymous) |
| Stable ids | unchanged (`smartgis.dem`, `smartgis.baogrid`, …) |
| Builtin load | `register_*` still performs `contribute_*`. `plugin.json` is the declared contract; **PluginShell need not parse it in the dem cutover** |

### Rollout

1. **Landed 2026-09-28:** `dem` (full package), `print` (`commands` + `views` + `manifest`), `model3d` (`commands` + `manifest`), `orthogrid` (`manifest` + `tests/`; `detail/` kept).
2. **Later (optional):** fold orthogrid `detail/` into `processing/`.
3. Do **not** recreate `product/proj` (removed; legacy AM mapping may remain).

### dem target tree (sample)

```
src/plugin/product/dem/
  BUILD.gn
  commands.h / commands.cc
  dem_export.h
  manifest/
    plugin.json
  views/
    tin_loader_dialog.h / .cc
    grid_loader_dialog.h / .cc
  processing/
    tin_loader.h / tin_loader.cc
    grid_loader.h / grid_loader.cc
  tests/
    loader_test.cc
```

## GN

- `//src/plugin/runtime/host:host` (root `//src/plugin:host` re-exports)
- `//src/plugin/runtime/processing:processing_views` (root `//src/plugin:processing` re-exports)
- `//src/plugin/product/<domain>:<domain>_views` and optional `<domain>_processing` (dem sample). Until cutover, orthogrid may still use flat `commands.*` + `detail/*` under `orthogrid_views`.
- `//src/legacy/plugin/orthogrid:orthogrid_kernel` — 2010 `Orthogrid` (`CreateOrthGrid` / `CvtToGrid` / `LoadGridBndFromFile`). `plugin_orthogrid` links this target. No shim headers under `plugin/product/orthogrid`.
- `//src/legacy/plugin:plugin` (AuxModule DLL)
- `//src/legacy/plugin/adapter:am` and `:cmd` (source_sets; root `//src/plugin:cmd` re-exports `:cmd`, `:legacy_cmd` alias; host deps `:am`)
- `//src/legacy/plugin/dem:plugin_dem` (and proj/print/model3d/orthogrid)

## Orthogrid kernel (2010 class)

Product `orthogrid/` keeps the Views registration (`commands.*`), Eigen Laplace (`detail/laplace_solver.*`), and `detail/boundary_solve` (gridbnd load + solve without the 2010 class) until the package rollout reaches that domain. The 2010 `Orthogrid` implementation (`grid`, `grid_create`, `grid_modify`, `curve`, `region`, `detail/polyline`, and the headers they own) lives under `src/legacy/plugin/orthogrid/kernel/` so `plugin_orthogrid` still links `CreateOrthGrid` / `CvtToGrid` / `LoadGridBndFromFile`. PascalCase on that class stays. Do not move the other product domains (`dem`, `print`, `model3d`) into `legacy/`.

## Non-goals

- Do not rename stable plugin ids.
- Do not put product domains back under `runtime/`.
- Do not add `include/` + `src/` under a product domain.
- Do not require PluginShell to load `manifest/plugin.json` in the dem sample cutover.
- Do not reshape print / model3d / orthogrid in the same change as the dem sample unless explicitly scoped.
