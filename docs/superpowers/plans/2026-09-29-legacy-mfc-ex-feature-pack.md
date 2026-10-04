<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# leftover `grid/` + `dock/` → MFC Feature Pack — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** On leftover `SmartGIS-Legacy.exe` only, delete in-tree `legacy/ui/grid` + `legacy/ui/dock` shims; use MSVC Feature Pack; deliver three UX waves (controls → shell look → Catalog/AMBox/FeatureInfo IA).

**Architecture:** Approach C′ tree today. Waves 1–3 remove `grid/`/`dock/` and polish Feature Pack UX. **Wave 4 (after 1–3):** reshape `legacy/ui/**` capability dirs to mirror endgame shell vocabulary (`app/views/shell` roles: shell / viewport / panels / catalog / dialogs / widgets) while **remaining under `src/legacy/ui`** — never hoist into `src/ui` or `src/app/views`. Design lock: umbrella **§11c**.

**Tech Stack:** MFC Feature Pack (`afxcontrolbars.h`, `widgets/feature_pack/feature_pack.h`), GN `ui_legacy` / `legacy_app`. Deepen widgets in §11d.

## Global Constraints

- Product path: leftover `SmartGIS-Legacy.exe` / `legacy_app` only — **no** `SmartGisViews` / `ui::views` edits.
- Toolkit: MSVC Feature Pack only — no BCG Pro vendor, no Qt, no second control tree.
- Feature Pack is a leftover bridge — **not** the product endgame (Views + Skia).
- Freeze `dll_stem=ui_legacy`; keep sole AFX attach in `widgets/dll/dll_main.cpp`.
- HWND-free / drop `SmtFeature*` from chrome: **out of scope** (separate SP3).
- Work on `master`; revise this plan + §11c in place — no new dated design twin.
- Includes use `"legacy/ui/<capability>/…"` (scheme C, no shim).
- After waves 1–3: layout may align to endgame **names/roles**; code and tree **stay in `legacy/ui`**.

---

## Current tree (Approach C′, 2026-09-29)

| Capability | Role today | §11c fate |
| --- | --- | --- |
| `grid/` | Chris Maunder `CGridCtrl` sources | **Delete** after wave 1 |
| `dock/` | `StackedWndDockBar` / `TabbedWndDockBar` | **Delete** after wave 2 |
| `widgets/` | `bcg_cmfc.h`, `grid_ctrl_support.h`, `prop_list_dock`, `widgets_core.cpp` DllMain; GN pulls `../grid` + `../dock` | Keep glue; drop grid umbrella + dock sources from `widgets_sources` |
| `dialogs/` | `dlg_2d_feature_info`, `dlg_att_struct_set` use `grid_ctrl_support` | Migrate controls |
| `ambox/` | `ambox_dock_bar` (`SmtAMBoxMgrDocBar : StackedWndDockBar`) | Base → `CMFCOutlookBar` |
| `catalog/` | Catalog trees hosted in frame tabbed dock | Wave 3 search |
| `legacy/app/shell` | `CMainFrame` owns `TabbedWndDockBar` + `SmtAMBoxMgrDocBar` | Flatten Catalog dock |
| `legacy/plugin/product/dem/views` | `dlg_tin_loader` → `grid_ctrl_support` | Migrate ListCtrl |

Forwarder groups `//src/legacy/ui/grid:grid` and `//src/legacy/ui/dock:dock` only re-export `ui_legacy` — remove with the dirs.

---

## File map

| Path | Action |
| --- | --- |
| `src/legacy/ui/grid/**` | Delete after call sites migrate; strip from `widgets/BUILD.gn` |
| `src/legacy/ui/widgets/grid_ctrl_support.h` | Delete |
| `src/legacy/ui/dock/stacked_wnd_dock_bar.*` | Delete after flatten |
| `src/legacy/ui/dock/tabbed_wnd_dock_bar.*` | Delete after flatten |
| `src/legacy/ui/dialogs/dlg_2d_feature_info.{h,cpp}` (+ `dialogs.rc` / `res/dialogs` as needed) | `CMFCPropertyGridCtrl` |
| `src/legacy/ui/dialogs/dlg_att_struct_set.{h,cpp}` | `CMFCListCtrl` report |
| `src/legacy/plugin/product/dem/views/dlg_tin_loader.{h,cpp}` (+ dem resources) | `CMFCListCtrl` report |
| `src/legacy/ui/shell/ambox/ambox_dock_bar.{h,cpp}` | Inherit `CMFCOutlookBar` / `CBCGPOutlookBar`; keep CP936 `AddWnd` |
| `src/legacy/app/shell/frame/main_frame.{h,cpp}` | Catalog: `CDockablePane` + `CMFCTabCtrl`; drop `TabbedWndDockBar` include |
| `src/legacy/ui/widgets/bcg_cmfc.h` | **Keep** |
| `src/legacy/ui/widgets/widgets_core.cpp` | **Keep** (sole DllMain) |
| `src/legacy/ui/README.md`, `docs/superpowers/src-layout.md` | Drop `grid/` / `dock/` rows when gone |
| Stale `src/legacy/ui/mfc_ex/**` if still present | Delete leftover duplicate (canonical is `widgets/` / `grid/` / `dock/`) |

---

## Wave 1 — Controls (delete `grid/`)

- [x] Inventory: `rg` `CGridCtrl|grid_ctrl_support|legacy/ui/grid` under `src/legacy` (expect dialogs ×2, dem TIN, `widgets/grid_ctrl_support`, `grid/**`)
- [x] `dialogs/dlg_2d_feature_info` → `CMFCPropertyGridCtrl` (read-only name/value; keep geom `CStatic`); update `dialogs.rc` / DDX
- [x] `dialogs/dlg_att_struct_set` → `CMFCListCtrl` report; rewire append/remove/move + end-edit handlers off `GVN_*`
- [x] `plugin/dem/views/dlg_tin_loader` → `CMFCListCtrl` report; drop `grid_ctrl_support` include
- [x] Remove `widgets/grid_ctrl_support.h`
- [x] Remove all `../grid/*.cpp` from `widgets/BUILD.gn`; delete `src/legacy/ui/grid/**` and `grid/BUILD.gn` (or empty group)
- [x] Drop `include_dirs += "../grid"` from `widgets_sources`
- [x] `build.bat ui_legacy` green (`plugin_dem` blocked by unrelated missing `legacy/core/api.h` / base link — not Wave 1)
- [x] Manual: FeatureInfo, AttStructSet, TIN loader each open once

## Wave 2 — Shell look + delete `dock/`

- [x] `main_frame`: replace `TabbedWndDockBar m_wndCatalogDocBar` with `CDockablePane` + embedded `CMFCTabCtrl`; port `InitCatalogDockBar` / `AddWnd` behavior file-locally (no exported shim type)
- [x] `ambox_dock_bar`: base class → `CMFCOutlookBar` (or `CBCGPOutlookBar` typedef); keep title normalize; stop calling `StackedWndDockBar::AddWnd`
- [x] Remove `../dock/*.cpp` from `widgets/BUILD.gn`; delete `src/legacy/ui/dock/**` and `dock/BUILD.gn`
- [x] Drop `include_dirs += "../dock"`; purge includes of `stacked_wnd_dock_bar.h` / `tabbed_wnd_dock_bar.h`
- [x] Tune default `OnAppLook` / Visual Manager (keep menu ID range unless broken)
- [ ] Manual: Catalog three tabs + AMBox dock/undock; theme switch
- [ ] Build green (blocked by unrelated `SmtListener`/`SmtCommand` link after `legacy/core` header-only — not §11c)

## Wave 3 — Information architecture

- [x] FeatureInfo (`dialogs/`): geom vs attr property groups + name filter edit
- [x] Catalog: in-pane search/filter for current tab’s tree (`CatalogTabDockPane` filter edit)
- [x] AMBox (`ambox/`): sort modules by name; Outlook titles prefixed `[Letter]` for grouping
- [ ] Manual smoke one-liner per surface
- [ ] Build green (same blocker)

## Wave 4 — Layout align to endgame vocabulary (still `legacy/`) — **B1 landed**

> Reference: `src/ui/gis/{shell,catalog,inspect,dialogs}` + `ui/views/map` — **names only**, MFC Feature Pack stays.

Target under `src/legacy/ui/`:

```
shell/{ambox,chart}/  map/  inspect/  catalog/  dialogs/  widgets/  res/shell/{,ambox,chart}/…
```

- [x] Confirm `grid/` and `dock/` are gone (waves 1–2); also removed stale `mfc_ex/`
- [x] Fold `ambox/` → `shell/ambox/`; fold `chart/` → `shell/chart/`
- [x] Rename `viewport/` → `map/`; `panels/` → `inspect/`
- [x] `res/ambox|chart` → `res/shell/ambox|chart`; scheme C includes + `.rc` paths
- [x] Capability names map to endgame roles in README table
- [x] `docs/superpowers/src-layout.md` + umbrella §11c B1 target
- [x] **Do not** move any TU to `src/ui/**` or `src/app/views/**`
- [x] RC compile (`ambox.res` / `diagram.res` / catalog icons) — scheme C paths
- [ ] Full `ui_legacy` link (blocked by unrelated `SmtListener` / `gis` LNK — same as wave 2–3)
## Docs / closeout

- [x] `src/legacy/ui/README.md` — remove `grid/` / `dock/`; document post-wave target + endgame-name mapping
- [x] `docs/superpowers/src-layout.md` — drop stale `gridctrl` / dock-shim names; note wave-4 alignment
- [ ] Umbrella §11c Success + this plan checkboxes; archive plan only when fully complete (incl. wave 4 or explicit deferral note) — wait for link green

## Done when

- No `CGridCtrl`, `StackedWndDockBar`, or `TabbedWndDockBar` under `src/legacy`
- No `src/legacy/ui/grid/` or `src/legacy/ui/dock/`
- Waves 1–3 done with build + manual evidence
- Wave 4 done **or** README records deferred layout delta vs target
- Tree remains entirely under `src/legacy/ui` (and related `legacy/app` / plugin call sites)
- README capability table matches tree
