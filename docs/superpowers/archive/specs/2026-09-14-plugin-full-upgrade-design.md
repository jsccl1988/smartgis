<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

> **Status: superseded** (2026-09-28 merge B). Merged into [`../../specs/2026-09-13-plugin-host-design.md`](../../specs/2026-09-13-plugin-host-design.md) — §Plugin full upgrade (folded). Do not revise here except mechanical link fixes; revise the living umbrella in place.


# Plugin full upgrade: cutover → retire MFC → style/ABI → domain depth

**Date:** 2026-09-14  
**Updated:** 2026-09-28  
**Status:** superseded (2026-09-28 merge B)
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
| Ownership | `BrowserView` owns a chrome helper (`app::PluginShell`) that holds Registry + PluginHost + ProcessingPool + CommandCatalog. |
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
  PluginShell            ← separate TU (avoids MapContents name clash)
    tool::CommandCatalog
    content::PluginHost   ← create_plugin_host(catalog, events, plugin_map*)
    plugin::Registry
    plugin::ProcessingPool
    ManagerView (Widget on demand)
        │
        ▼ register_* builtins on start
  plugin/{dem,proj,print,model3d,orthogrid}  (*_views source_sets)
```

**MapContents name clash:** `content/public/map_contents.h` and `content/public/plugin_host.h` both declare `content::MapContents`. Product chrome **must not** include both in one TU. `PluginShell` lives in `app/views/plugin_shell.*` and includes only `plugin_host.h`. Session map stays on `BrowserView` via `map_contents.h`. Plugin-face map may be `nullptr` in A (dialogs that need extent use host later) or a tiny adapter type local to `plugin_shell.cc` that does **not** share the session class name in headers included by `browser_view.cc`.

## Phase A — Cutover (Views wiring)

### Behavior

1. `BrowserView::init` constructs `PluginShell` after ViewHosts / session exist.
2. `PluginShell::start_builtins()`:
   - `add_manifest` for `smartgis.dem|print|model3d|baogrid` (`kind=builtin`, `api_version=2`, `TrustClass::kBuiltin`). Tree name is `orthogrid`; stable plugin id remains `smartgis.baogrid`. (`smartgis.proj` product Views removed; legacy AM stem mapping may remain.)
   - `register_builtin_hooks(id, register_X, stop_noop)` where `register_X` is `plugin::register_dem` / `register_print` / `register_model3d` / `register_orthogrid`.
   - `set_enabled(id, true, host)` for each.
3. Menu bar adds **Plugins** → opens a `ui::views::Widget` hosting `ManagerView(registry, host)`.
4. Ambox / catalog command dispatch: if `PluginHost::execute(id)` succeeds, prefer that; else keep existing ViewHost paths.
5. MFC `SmtApp::InitSmtAuxModules` **unchanged**.

### Files

| Path | Role |
| --- | --- |
| `src/app/views/shell/plugin/plugin_shell.h` `.cc` | Owns catalog / host / registry / pool; start_builtins; show_manager |
| `src/app/views/browser_view.h` `.cc` | Owns `unique_ptr<PluginShell>`; menu item; destroy order |
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

- `.tmp/cutover/scan_abi_residuals.py` (local, not versioned) scoped to `src/plugin` new trees (`*_views`, host, widgets, python) reports clean for flat includes / `Export_Smt` / `dll_stem = "Smt`.
- Touched Views-path headers have no new `Smt*` type names.

## Phase D — Domain depth

### Behavior

1. Replace DEM processing stubs (`dem.tin_from_xyz`, `dem.grid_from_heightmap`) with factories that call MFC-free loaders + `algorithm/tin` (and GDAL/grid as already used by leftover loaders). Success path posts `done` on main thread; document writes via `sdb::EditSession` when a map write is required (if session seam missing, return structured error — no silent no-op).
2. Proj processing already gated by `PLUGIN_PROJ_VIEWS_USE_PROJ_API` — enable for Views build and cover with host_test.
3. Orthogrid / model3d / print: ensure command handlers do not crash without scene; deepen where kernels exist (`orthogrid` Laplace / boundary I/O; model3d scene ops behind null checks).
4. Docs and code use **orthogrid** naming; `baogrid` only as historical alias in abi map.

### Views seam (2026-09-27)

Dialog factories must not construct a `ui::views::View` on the stack. `plugin::show_owned_dialog` (`plugin/runtime/widgets/owned_dialog.h`) moves the body into `ui::views::Dialog::run_modal`, which owns it until close. DEM, proj, and print use that path. Orthogrid and model3d do not contribute dialogs.

Processing and command failures publish a JSON string through `plugin::set_operation_result`. `ProcessingPool` copies that string into the existing `done(bool, std::string)` callback. There is no second plugin system and no new map type.

- DEM `tin_from_xyz` / `grid_from_heightmap` still call the MFC-free loaders. `PluginHost` has no `EditSession`, and `MapContents::DispatchPlugin` has no surface schema, so a successful load returns `{"error":"no_map_seam",...}` and `false` instead of dropping the surface.
- Proj `transform_grid` keeps `project_point` and stores nodes in `TransformGridOutput` (`consume_transform_grid_output`).
- Orthogrid `create_orth_grid_processing` loads a real `gridbnd` body and runs product Laplace (`detail/boundary_solve`). A header-only file does not succeed. Save-boundary after a successful pick returns `{"error":"not_available_without_legacy_kernel"}` because the 2010 session is not on the Views graph.
- model3d commands stay `false` with `{"error":"no_scene_device",...}` until a device pointer exists on `PluginHost` / `MapContents`.
- Print preview Save reports `{"error":"export_not_implemented"}` after a path is chosen. Showing the preview dialog is the Views fix.

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
