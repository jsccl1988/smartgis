<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Plugin full upgrade: cutover → retire MFC → style/ABI → domain depth

**Date:** 2026-09-14  
**Status:** accepted  
**Foundation:** `docs/superpowers/specs/2026-09-13-plugin-host-design.md` (host / Registry / store / Python already landed). This spec does **not** reopen those APIs.  
**Execution:** phases A→B→C→D may land **in parallel** on disjoint paths; integrate on `master`. No new branches.

## Goal

Make `SmartGisViews` the product plugin entry: builtins start via `plugin::Registry` + `content::PluginHost`, Plugin Manager is reachable from the shell, MFC `CDlg*` cease to be the Views path, `src/plugin` aligns with cutover style/ABI, and domain processing factories call real kernels (`algorithm/*` + `EditSession`) instead of stubs.

## Locked product choices

| ID | Choice |
| --- | --- |
| A1 | Wire **only** `SmartGisViews` (`src/app/views`). MFC `SmartGis.exe` keeps `InitSmtAuxModules`. |
| S1 | Startup loads **five builtins only** (dem / proj / print / model3d / orthogrid), all enabled. No zip / `*.am` scan. |
| M1 | Menu entry opens **Plugin Manager** (`ManagerView`) for enable/disable. No install-from-zip UI required this program. |
| Ownership | `BrowserView` owns a chrome helper (`app::PluginChrome`) that holds Registry + PluginHost + ProcessingPool + CommandCatalog. |
| Parallel | Agents edit **non-overlapping trees**; see path partition below. |

## Non-goals (still out)

- Real `--type=utility` Mojo worker for processing (keep thread pool + `kUtilityStub`).
- Marketplace / ratings / identity.
- Multi-interpreter Python / pip into embed.
- Native plugin sandbox.
- Changing stable plugin ids (`smartgis.dem`, …).
- Rewriting leftover `dlg_*.h` as the new public API (they stay leftover until removed from GN).

## Architecture (target)

```
SmartGisViews (BrowserView)
  MapContents session     ← content/public/map_contents.h (GPU session)
  ViewHost ×3             ← EventBus / tools
  PluginChrome            ← separate TU (avoids MapContents name clash)
    tool::CommandCatalog
    content::PluginHost   ← create_plugin_host(catalog, events, plugin_map*)
    plugin::Registry
    plugin::ProcessingPool
    ManagerView (Widget on demand)
        │
        ▼ register_* builtins on start
  plugin/{dem,proj,print,model3d,orthogrid}  (*_views source_sets)
```

**MapContents name clash:** `content/public/map_contents.h` and `content/public/plugin_host.h` both declare `content::MapContents`. Product chrome **must not** include both in one TU. `PluginChrome` lives in `app/views/plugin_chrome.*` and includes only `plugin_host.h`. Session map stays on `BrowserView` via `map_contents.h`. Plugin-face map may be `nullptr` in A (dialogs that need extent use host later) or a tiny adapter type local to `plugin_chrome.cc` that does **not** share the session class name in headers included by `browser_view.cc`.

## Phase A — Cutover (Views wiring)

### Behavior

1. `BrowserView::init` constructs `PluginChrome` after ViewHosts / session exist.
2. `PluginChrome::start_builtins()`:
   - `add_manifest` for `smartgis.dem|proj|print|model3d|baogrid` (`kind=builtin`, `api_version=2`, `TrustClass::kBuiltin`). Tree name is `orthogrid`; stable plugin id remains `smartgis.baogrid`.
   - `register_builtin_hooks(id, register_X, stop_noop)` where `register_X` is `plugin::register_dem` / `register_proj` / `register_print` / `register_model3d` / `register_orthogrid`.
   - `set_enabled(id, true, host)` for each.
3. Menu bar adds **Plugins** → opens a `ui::views::Widget` hosting `ManagerView(registry, host)`.
4. Ambox / catalog command dispatch: if `PluginHost::execute(id)` succeeds, prefer that; else keep existing ViewHost paths.
5. MFC `SmtApp::InitSmtAuxModules` **unchanged**.

### Files

| Path | Role |
| --- | --- |
| `src/app/views/plugin_chrome.h` `.cc` | Owns catalog / host / registry / pool; start_builtins; show_manager |
| `src/app/views/browser_view.h` `.cc` | Owns `unique_ptr<PluginChrome>`; menu item; destroy order |
| `src/app/views/BUILD.gn` | deps on `//src/plugin:host` + five `*_views` targets + `//src/tool:dispatch` |

### Acceptance A

- `ninja -C out SmartGisViews` links.
- `plugin_host_test` still green.
- Manual / self-test: Plugins menu shows 5 rows; disable `smartgis.dem` withdraws `dem.*` commands; re-enable restores via hooks.

## Phase B — Retire MFC shell (Views path)

### Behavior

1. Views / `*_views` **never** compile or link leftover `dlg_*.cpp` / `CDialog`.
2. DEM (and any other) **loader kernels** used by processing move to MFC-free `.cc` (no `stdafx.h`) in a `source_set` consumed by `dem_views` (and optionally still by MFC DLL).
3. MFC `smt_mfc_shared_library` targets may keep `dlg_*` + `stdafx` for `SmartGis.exe` until that host dies; they are **leftover-only**, not on the Views graph.
4. Document in `src/plugin` / `src-layout`: product path = builtin + Views; `*.am` = leftover adapter only.

### Acceptance B

- `SmartGisViews` dependency graph has zero `dlg_*.cpp`.
- Loaders callable from `dem_views` without MFC.

## Phase C — Style / ABI (`src/plugin` only)

### Behavior

Align with `docs/build/abi-rename-map.md` and the repo cutover design, **scoped to `src/plugin/**`**:

- Includes: `"plugin/..."` / `"algorithm/..."` form; no flat leftover-only includes in **new** TUs.
- New/touched functions `snake_case`; public namespace `plugin` (+ `detail`).
- Rename leftover product types encountered on the Views path (e.g. `SmtTinFileFmt` → `TinFileFmt`) in the same change as call sites under `src/plugin`.
- `dll_stem` already mapped (`plugin_dem`, …); do not resurrect `SmtAM*`.
- Leftover `SmtAuxModule` / `SmtAModuleManager` stay until MFC exe drops; do not snake_case LoadLibrary ABI exports still required by `*.am`.

### Acceptance C

- `tools/cutover/scan_abi_residuals.py` scoped to `src/plugin` new trees (`*_views`, host, widgets, python) reports clean for flat includes / `Export_Smt` / `dll_stem = "Smt`.
- Touched Views-path headers have no new `Smt*` type names.

## Phase D — Domain depth

### Behavior

1. Replace DEM processing stubs (`dem.tin_from_xyz`, `dem.grid_from_heightmap`) with factories that call MFC-free loaders + `algorithm/tin` (and GDAL/grid as already used by leftover loaders). Success path posts `done` on main thread; document writes via `sdb::EditSession` when a map write is required (if session seam missing, return structured error — no silent no-op).
2. Proj processing already gated by `PLUGIN_PROJ_VIEWS_USE_PROJ_API` — enable for Views build and cover with host_test.
3. Orthogrid / model3d / print: ensure command handlers do not crash without scene; deepen where kernels exist (`orthogrid` Laplace / boundary I/O; model3d scene ops behind null checks).
4. Docs and code use **orthogrid** naming; `baogrid` only as historical alias in abi map.

### Acceptance D

- `plugin_host_test` covers at least one DEM processing path with a tiny fixture file (or synthetic buffer) returning true.
- Proj transform path returns true for a known XY fixture when PROJ is linked.
- No `stub_*` processing factories remain for dem.

## Path partition (parallel agents)

| Lane | Paths | Phases |
| --- | --- | --- |
| Chrome | `src/app/views/**`, `docs/README.md` index row | A (+ docs) |
| DEM loaders | `src/plugin/dem/**` (except do not edit `app/views`) | B + D (dem) + C (dem types) |
| Other domains | `src/plugin/{proj,print,model3d,orthogrid}/**` | D (+ light C) |
| Host polish | `src/plugin/{registry,manifest,manager_view,processing,legacy_*}.*`, `src/plugin/widgets/**`, `src/plugin/python/**` | C (host tree) |
| Content clash note | If renaming plugin-face `MapContents`, only `src/content/public/plugin_host.h` + `plugin_host.cc` + plugin call sites — **coordinate**; prefer adapter-in-chrome to avoid rename this program |

## Testing

- `ninja -C out plugin_host_test && out\plugin_host_test.exe`
- `ninja -C out SmartGisViews`
- Optional: `build.bat views` when fixing compile errors (user / auto-build-fix).

## Docs (same program)

- Index this spec + plan in `docs/README.md`.
- `docs/build/src-layout.md` plugin row: Views owns Registry; MFC `*.am` leftover.
- Do not archive `plugin-host` docs until this program lands and host remains accurate.

## Relation to prior cycle

| Prior (plugin-host) | This program |
| --- | --- |
| Built Registry / PluginHost / Views dialogs / Python / store | Wire chrome (A), stop treating MFC dialogs as product (B), finish style on plugin tree (C), un-stub domain kernels (D) |
| Out of cycle: retire `CDlg*` from dll graphs | B starts that for Views; MFC exe may keep compiling dlg |
| Out of cycle: real utility process | Still out |
