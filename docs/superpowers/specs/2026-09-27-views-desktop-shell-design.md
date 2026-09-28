<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/app/views` desktop shell (QGIS-style menus + map context)

**Date:** 2026-09-27  
**Status:** active  
**Updated:** 2026-09-28 — §UI visual forensics (A+C); shared `out/ui/` markup pack (Debug+Release); prior: §GIS Python Console + analysis results; §Console coverage + performance; §UI interactive harness; §Declarative markup; §Global theme paint. Do not open new dated twins.
**Related:**

| Topic | Doc | Relation |
| --- | --- | --- |
| Views toolkit / compositor / GIS panels / markup | §Folded topics (this file) + archive twins | toolkit `src/ui/views`; GIS chrome `src/ui/gis` |
| MFC → Views shell | §Folded topics (this file) | `SmartGisViews.exe` entry |
| Toolkit subdirectory | §Folded topics (this file) | `MenuBar` under `ui/views/primitives/menu/` |
| 2D frame + RHI present | [`2026-09-13-render-rhi-scene-design.md`](2026-09-13-render-rhi-scene-design.md) | CPU frame + Views GPU present |
| SP3 host extract | [`2026-09-19-legacy-deep-abstraction-umbrella-design.md`](2026-09-19-legacy-deep-abstraction-umbrella-design.md) §SP3 | HWND-free content |
| Archived capability twin | [`../archive/specs/2026-09-28-app-views-capability-split-design.md`](../archive/specs/2026-09-28-app-views-capability-split-design.md) | superseded |
| As-built | [`../../../src/app/views/README.md`](../../../src/app/views/README.md) | update when landing |
| Stack layering | **§Chromium-style app/views layering** (this file) | Chromium map + deps + gaps |

---

## 1. Goal / Non-goals

### 1.1 Goal

1. **Directory = responsibility** under `src/app/views/{shell,document,camera,present,input}/`. No root forwarding headers.
2. **Top bar** is four dropdown menus: File, Edit, View, Layer. `MenuBar::add_item` remains for other callers.
3. **View navigation** is one command table shared by the View menu and the map-canvas right-click. AM Box does not list those commands.
4. **Navigation gaps** in this cycle: extent history, zoom to active layer, zoom to selection, session bookmarks, status-bar scale, identify.

### 1.2 Non-goals

- Do not write bookmarks to a project file. The list lives for the process session only.
- Do not draw a scale bar, north arrow, or other map decoration. Scale is status-bar text.
- Do not add layout composer. Measure / Selection / Symbology / SpatialAnalysis panels are owned by §GIS panels A+B+C (this file) + toolkit § in ui-views-controls.
- Do not change catalog layer/source/map context menus (`catalog.layer.view` and siblings stay).
- Do not change `view.pan` / `view.zoom_in` / `view.zoom_out` / `view.full` / `view.refresh` tool semantics in `src/tool`.
- Do not replace 3D trackball. Navigation commands update the shared 2D extent used by the Map and Data pages.
- Do not introduce Qt, a ribbon, or a second widget kit.
- Do not deepen past `src/app/views/<module>/`. Public namespace stays `app`.

---

## 2. Locked decisions

| # | Decision |
| --- | --- |
| 1 | Physical tree in §3. Includes move with the files. No shim headers at `src/app/views/*.h`. |
| 2 | `ui::views::MenuBar::add_menu(label, items)` opens `show_context_menu` under that top item. Existing `add_item` stays. |
| 3 | Product shell uses only `add_menu`. Flat Open/Save/Map/… buttons go away. |
| 4 | `shell/browser/commands/view_commands` owns the navigation table. View menu and map right-click both render it. |
| 5 | Map right-click is the navigation table only. Render backends are View-menu-only, after a separator. |
| 6 | AM Box grouping drops every `view.*` id and the placeholder Pan item. Select, Edit, and plugin Tools stay. Placeholder Identify is removed from AM Box. |
| 7 | New navigation ids are handled in the shell. They are not added to `tool::CommandCatalog`, so AM Box cannot list them. |
| 8 | Extent stack holds at most 32 entries. A navigation that changes the extent pushes the previous extent first. Previous/Next do not push. |
| 9 | Bookmarks are an in-memory list on `ViewNavigation`. Duplicate labels become `Name (2)`, `Name (3)`, … |
| 10 | Scale text is `1:N` (§4.6). Viewport width 0 shows `1:—`. |
| 11 | Empty active layer, empty selection, or exhausted extent stack: the command runs, the status bar explains, the extent does not change. |
| 12 | Git: work on `master`. No feature branch. |
| 13 | Tests in §6. Do not add new `--self-test` scenes. Existing self-test assertions stay; only include paths change. |

---

## 3. Target tree

Capability ownership for `document/` / `camera/` / `present/` / `input/` is locked in §Capability split (archived twin [`2026-09-28-app-views-capability-split-design.md`](../archive/specs/2026-09-28-app-views-capability-split-design.md)). Those modules stay outside `shell/`. **`shell/`** follows the Chromium Browser / BrowserView layout in §Chromium Browser / BrowserView (Approach 1).

```
src/app/views/
  BUILD.gn
  README.md
  main.cc                         # wWinMain glue only (parse → content_main)

  shell/
    app/                          # process entry + content host
      browser_main.h / .cc
      views_content_host.h
      cmdline/
        views_launch_options.h / .cc
        views_launch_options_test.cc
    browser/                      # controller (no concrete UI widgets)
      browser.h / .cc             # session ownership + public controller API
      commands/
        app_commands.h / .cc
        catalog_commands.cc
        view_commands.h / .cc     # navigation table + menu item builder
        view_commands_test.cc
      nav/
        draft_nav.cc
      plugin/
        plugin_shell.h / .cc
    ui/                           # Widget tree only
      browser_view.h / .cc
      pages/
        map_pages.cc
      panels/
        shell_panels.cc
        inspector_sync.cc
    showcase/
      atmosphere_showcase.h / .cc
    self_test/
      self_test.h / .cc

  input/                          # MapHwndGestures (moves under content/browser — §Content sink)

# Landed under content (Approach 2):
#   src/content/browser/document/   MapScene
#   src/content/browser/camera/     ViewFrame, OrbitFrame, ViewNavigation
#   src/content/browser/present/    map2d / scene3d present stack — see §Present
```

Include examples after the shell reshape:

- `app/views/shell/app/browser_main.h`
- `app/views/shell/browser/browser.h`
- `app/views/shell/ui/browser_view.h`
- `content/browser/document/map_scene.h`
- `content/browser/camera/view_frame.h`
- `content/browser/present/map2d/map2d_presenter.h`
- `content/browser/present/scene3d/scene3d_presenter.h`
- `app/views/input/map_hwnd_gestures.h`

GN labels: `//src/app/views:views`, `:map_scene`, `:map_camera`, `:map_present`, `:scene3d_present`, `:map_hwnd_gestures`. Prefer `shell_browser` / `shell_ui` source_sets with **ui → browser** only when practical. Test executables: `map_scene_test`, `view_navigation_test`, `scene3d_presenter_test`, `view_commands_test`.

`MenuBar` stays in `src/ui/views/primitives/menu/`. This spec adds `add_menu` there; it does not move the widget.

---

## 4. Menus and navigation

### 4.1 Top menus

| Menu | Entries |
| --- | --- |
| File | Open, Save, Export, Exit |
| Edit | Undo (`edit.undo`), Redo (`edit.redo`), Clear selection (`selection.clear`) |
| View | Navigation table (§4.2), separator, Refresh (`view.refresh`), RHI (`view.backend.rhi`), MapLibre (`view.backend.maplibre`) |
| Layer | Create layer (`catalog.layer.create`), Add basemap (`catalog.layer.add_basemap`), Remove layer (`catalog.layer.remove`), Zoom to layer (`view.zoom_layer`) |

Layer-tree, datasource, and map-doc right-clicks stay as they are in `CatalogView`.

### 4.2 Navigation table

Shared by the View menu (above the separator) and the map right-click. Order:

| Label | Id | Behavior |
| --- | --- | --- |
| Pan | `view.pan` | Existing tool |
| Zoom in | `view.zoom_in` | View menu arms the existing tool. Right-click calls the existing zoom-at-point path (`MapScene::apply_zoom_at`) about the click and does not change tool semantics |
| Zoom out | `view.zoom_out` | Same split as Zoom in |
| Zoom full | `view.full` | Existing command. Fit all data |
| Zoom to layer | `view.zoom_layer` | Extent of the active layer. No active layer: status `No active layer` |
| Zoom to selection | `view.zoom_selection` | Extent of the current selection. Empty: status `No selection` |
| Previous extent | `view.extent_prev` | Pop toward older. At the start: status `No previous extent` |
| Next extent | `view.extent_next` | Pop toward newer. At the end: status `No next extent` |
| Identify | `view.identify` | See §4.5 |
| Add bookmark | `view.bookmark_add` | Store the current extent. Default label `Bookmark N` |
| Bookmark rows | `view.bookmark_go` | One row per stored bookmark, after Add bookmark. The menu closure carries the list index. Jump restores that extent |

`view.backend.*` is not part of this table.

### 4.3 AM Box

`groups_from_catalogs` skips ids that start with `view.`. Remove the forced Pan item and the forced Identify item. Keep the forced Select item and the Edit defaults. Plugin groups are unchanged.

### 4.4 Extent stack

`ViewNavigation` stores extents. Capacity 32; pushing past the cap drops the oldest.

Push the pre-change extent when any of these succeed in changing it: pan (once on mouse-up), each wheel step, zoom in/out, zoom full, zoom to layer, zoom to selection, bookmark jump. `view.extent_prev` and `view.extent_next` move an index and do not push. A failed command (§2.11) does not push.

### 4.5 Identify

- Map right-click: call `MapScene::hit_test` at the click point, `select_feature` on a hit, and show the FeatureInfo tab. A miss leaves the selection unchanged and sets status `No feature`.
- View menu: activate `selection.point` (no cursor point on the menu).

### 4.6 Scale

Status bar, updated on extent change and viewport resize.

- Viewport width 0: `1:—`.
- Projected meters: `N = round((extent_width_m / viewport_width_px) * 96 / 0.0254)`. Show `1:N`. `N < 1` shows `1:1`.
- Geographic degrees: center latitude is the north-south midpoint of the current extent. Convert width with `111320 * max(cos(center_latitude), 0.01)` meters per degree, then the same formula.

### 4.7 Where commands run

`Browser` dispatches shell-owned ids (`view.zoom_layer`, `view.zoom_selection`, `view.extent_prev`, `view.extent_next`, `view.identify`, `view.bookmark_add`, `view.bookmark_go`) to `ViewNavigation` before `run_tool_command` (until P1 lands, `BrowserView` may still hold this path). Existing tool ids still go through `run_tool_command`.

On the 3D page the same menu is shown. Those commands still update the shared 2D extent. They do not switch the 3D camera tool off trackball.

---

## 5. Failure behavior

| Case | Result |
| --- | --- |
| No active layer | Status `No active layer`. Extent unchanged |
| No selection | Status `No selection`. Extent unchanged |
| Extent stack has no previous / next | Status `No previous extent` or `No next extent`. Extent unchanged |
| Identify misses | Status `No feature`. Selection unchanged |
| Bookmark label collision | Append ` (2)`, then ` (3)`, … Do not replace the older bookmark |
| Viewport width 0 | Scale text `1:—` |

Commands stay enabled. The status string is the failure signal.

---

## 6. Tests

| Test | Asserts |
| --- | --- |
| `view_navigation_test` | Push and previous/next; cap 32 drops the oldest; empty layer and empty selection do not change the extent; bookmark add + go; duplicate bookmark label; meter scale and geographic scale; width 0 → `1:—` |
| `view_commands_test` | Navigation table order matches §4.2 and contains no `view.backend.*` |
| Views AM Box unit test | A catalog containing `view.zoom_in`, `view.pan`, and `view.full` produces no `view.*` items and no Pan or Identify placeholder. Select remains |
| `MenuBar` unit test | `add_menu` stores a child list; activating the top item is observable (callback or item count). Existing `add_item` tests stay green |
| `map_scene_test`, `scene3d_presenter_test`, `SmartGisViews --self-test` | Include path updates only. Assertions unchanged |

---

## 7. Done

- `src/app/views` matches §3 / §Chromium-style app/views layering / §Capability / §Chromium Browser / BrowserView and has no root forwarding headers.
- The top bar is File / Edit / View / Layer dropdowns.
- Map right-click matches the navigation table. View adds only the refresh and backend rows below a separator.
- AM Box shows Select, Edit, and plugin Tools, and does not show view navigation.
- The status bar shows the scale text from §4.6.
- Tests in §6 pass.

---

## §Chromium-style app/views layering（2026-09-28）

**Status:** accepted (S1–S5 landed). Residuals: pages/panels include noise only.

Maps SmartGIS product host layers to Chromium habits. This section is the **stack contract**; narrower §§ below own details (`§Capability`, `§Present layering`, `§Chromium Browser / BrowserView`, `§Process entry`).

### Chromium → SmartGIS

| Chromium | SmartGIS | Role |
| --- | --- | --- |
| `chrome/browser` (`Browser`) | `shell/browser` | Tabs / windows / session lifecycle; owns document + camera + present facades + input adapters; **not** paint |
| `chrome/browser/ui` (`BrowserView`) | `shell/ui` | Views chrome layout only: menus, toolbars, docks, tab strips, status |
| `content` / `WebContents` | `document/` + `present/*` facades | Document data (`MapScene`) and present orchestration (`Map2dPresenter` / `Scene3dPresenter`) |
| compositor / viz frame | `present/*/frame`, `paint/`, `host/` | Frame prep, software paint, present-surface helpers |
| viewport / transform helpers | `camera/` | **Sibling** of shell — shared by present, document (hit-test coords), and shell |
| input routing | `input/` | **Sibling** — HWND gesture adapters only |
| browser process entry | `shell/app` | `browser_main`, `ViewsContentHost`, CLI11 cmdline |

### Target tree (locked shape)

Physical directories already match this tree (2026-09-28 as-built). No new capability roots. No forwarding headers at `src/app/views/*.h`.

```
src/app/views/
  main.cc                         # wWinMain glue only
  shell/
    app/                          # process entry (≈ browser process host)
      browser_main.*
      views_content_host.h
      cmdline/
    browser/                      # Browser controller (≈ chrome/browser)
      browser.*                   # owns session; public controller API
      commands/                   # pure tables / builders (no Widget)
      nav/                        # draft / extent navigation helpers on Browser
      plugin/
    ui/                           # BrowserView only (≈ chrome/browser/ui)
      browser_view.*              # Widget tree; holds Browser*
      pages/                      # map tab / viewport chrome wiring
      panels/                     # inspector / ambox / catalog chrome sync
    showcase/
    self_test/
  document/                       # MapScene — layers/features (≈ content data)
  camera/                         # ViewFrame, OrbitFrame, ViewNavigation — SIBLING
  present/                        # map2d / scene3d present stack (see §Present)
  input/                          # MapHwndGestures — SIBLING
```

### Ownership

| Layer | Owns | Must not own |
| --- | --- | --- |
| `shell/app` | Process entry, launch options, content host glue | Session objects, Widget tree, paint |
| `shell/browser` (`Browser`) | `MapScene`, `ViewFrame`, `OrbitFrame`, `ViewNavigation`, presenters + stereo session, `BlitFrameCache`, `MapHwndGestures`×3, `MapContents` / `ViewHost`s, `PluginShell`, `unique_ptr<BrowserView>` | Concrete Widget layout code; GDI/RHI paint TUs |
| `shell/ui` (`BrowserView`) | Widget tree (MenuBar, splitters, TabStrip, MapViewport chrome, status); `Browser*` | Session members listed above; camera types as owned fields |
| `document/` | Layer/feature/OGR state | View transform, HDC, rhi Device |
| `camera/` | Extents, orbit, navigation stack | Shell chrome, present paint |
| `present/` | Facade + frame/paint/session/host (see §Present) | Shell menus; camera ownership |
| `input/` | HWND gesture adapters | Camera math; present paint |

### Allowed dependencies (hard)

```
shell/ui  →  shell/browser  →  { document, camera, present, input }
shell/app →  shell/browser     (constructs Browser; thin content_main host)
```

| Edge | Rule |
| --- | --- |
| `shell/ui` → `shell/browser` | Required. UI talks to controller via `Browser*`. |
| `shell/browser` → `{document,camera,present,input}` | Required. Browser owns / orchestrates those capabilities. |
| `shell/browser` → concrete `shell/ui` widgets | **Forbidden** (target). Prefer `BrowserWindow`-like abstract surface or callbacks; today `browser.cc` still includes `browser_view.h` for lifecycle — harden in next slices. |
| `present` → `shell` | **Forbidden.** |
| `camera` under `shell/` | **Forbidden.** Camera stays a **sibling** of `shell/`. |
| `document` / `input` → `shell` | **Forbidden.** |
| Compat / forwarding shims at old paths | **Forbidden.** Colocate `.h` with `.cc`; update includes in the same change. |

Shell may include only present **facades** + `session/` + `host/` headers it needs — not deep `paint/` internals.

### As-built vs target (gaps)

| Area | As-built | Target gap |
| --- | --- | --- |
| Capability dirs | `document/` `camera/` `present/` under `content/browser/`; `input/` may still be under `app/views` until sink lands | Finish `input/` move per §Content sink |
| `present/` Chromium split | facade / frame / paint / session / host | None for layout; see present README |
| `shell/{app,browser,ui}` dirs | Present | None for paths |
| Session ownership | Fields live on `Browser` | Done for members |
| Controller logic | Nav / tool / catalog / file / extent bodies on `Browser` (`commands/`, `nav/`) | Residual: some pages/panels TUs still carry wide UI includes |
| Fat `browser_view.cc` | Chrome + menus/ambox/status; controller moved | Trim pages/panels include noise when touching those TUs |
| Deps | `BrowserUiDelegate` + `create_browser_ui`; `browser.cc` does not include `browser_view.h` | Done for S5 |
| `commands/*.cc` | Include `browser.h` only (no concrete `BrowserView`) | Done |

### Target splits for fat `BrowserView` (design — do not land in this doc edit)

| Slice | Move to | Leaves on `BrowserView` |
| --- | --- | --- |
| S1 | `shell/browser`: `run_tool_command`, shell navigation dispatch, extent watch / scale refresh orchestration | Menu rebuild callbacks that call into Browser |
| S2 | `shell/browser/nav`: draft apply, pinch/pan → camera, `push_shared_extent` / orbit pull | HWND → draft forward from viewport chrome |
| S3 | `shell/browser/commands`: catalog / file / plugin command handlers (already partially extracted tables) | MenuBar / context menu presentation |
| S4 | `shell/ui/pages` + `panels`: pure chrome wire (tabs, attach viewports, inspector sync UI) with `Browser*` accessors only | — |
| S5 | Harden deps: remove `browser` → concrete `BrowserView` include; lifecycle via abstract `BrowserWindow` or `.cc`-only friendship | UI accessors stay on BrowserView |

### Non-goals (this layering)

- Do **not** move `camera/` under `shell/` (or under `present/`).
- Do **not** add compat shims or root forwarding headers.
- Do **not** reverse deps (`present` → `shell`, `browser` → Widget types as a permanent pattern).
- Do **not** open a new dated layout-only design file (ban list: app/views capability dirs → this living shell design).
- Do **not** change menu ids / AM Box / paint fallback rules here (owned by §2–§4 and map2d-frame).

### Recommended next implementation slices (checklist)

1. [x] S1 — shell navigation + `run_tool_command` on `Browser` (`document_commands.cc` / `draft_nav.cc`).
2. [x] S2 — draft/gesture→camera in `shell/browser/nav/draft_nav.cc`; UI HWND forwards only.
3. [x] S3 — catalog/file/plugin handlers on browser (`catalog_commands.cc`, `document_commands.cc`, `session_commands.cc`); menus call `Browser*`.
4. [x] S4 — `browser_view.*` is Widget chrome + thin wire; pages/panels UI accessors + forwards.
5. [x] S5 — `BrowserUiDelegate` / `create_browser_ui`; GN `:shell_ui` → `:shell_browser` only.
6. [x] Sync as-built blurbs in `src/app/views/README.md` / `docs/build/src-layout.md` (no new dated specs).

---

## §Capability split（merged 2026-09-28）

Directory = capability. Supersedes earlier §3 buckets `document/` + `input/` + `scene3d/` for ownership of transform/paint. Stack contract: **§Chromium-style app/views layering**.

```
src/app/views/
  document/     # MapScene — layers/features/OGR; no view transform, HDC, or rhi Device
  camera/       # ViewFrame, OrbitFrame, ViewNavigation, map_host_extent
  present/      # Chromium-style present stack (see §Present layering below)
  input/        # MapHwndGestures only (HWND adapter)
  shell/        # Chromium Browser / BrowserView — see §Chromium Browser / BrowserView
```

| Locked | Choice |
| --- | --- |
| `MapScene` | Drops `pan_*` / `scale_` / `paint*` / `present_gpu` / `<windows.h>` |
| Hit test | Map coordinates; shell converts via `ViewFrame` |
| Composition | **`Browser`** owns one `MapScene`, one `ViewFrame` (Map/Data), one `OrbitFrame` (3D), presenters, gesture adapters (not `BrowserView`) |
| Namespaces | Moved types use `content` (§Content sink); no shim headers; physical roots `content/browser/{document,camera,present}/` |
| Test binary names | `map_scene_test` / `view_navigation_test` / `scene3d_presenter_test` |

Non-goals: do not change menu ids / AM Box grouping from §2–§4; do not relocate `document/` / `camera/` / `present/` / `input/` for this Browser split; do not change paint results or GDI/GPU fallback rules (see map2d-frame living). Shell internal layout is owned by §Chromium Browser / BrowserView.

### §Document internal split（2026-09-28）

`MapScene` stays the **only** public façade callers use (`Browser` / present / tests). Implementation is composed under shallow subdirs (Approach 1; no forwarding headers):

```
content/browser/document/
  map_scene.*                 # thin façade; public API unchanged
  map_scene_test.cc
  store/   map_layer.h + layer_store.*     # GeomKind/Feature/Layer + CRUD/selection/ids
  ingest/  ogr_ingest.* + geojson_write.* + seed_paths.*
  style/   style_bind.*                    # StyleDocument + basemap ptr + resolve
  query/   extent_query.* + inspector.*
  edit/    feature_edit.*                  # draft / vertex / TIN / hit_test
```

| Locked | Choice |
| --- | --- |
| Public surface | Callers keep `MapScene*`; new types live in `content::detail` |
| Types | `MapLayer` / `MapFeature` / `GeomKind` in `detail`; `MapScene` exposes `using Layer = detail::MapLayer` (etc.) |
| Ownership | `MapScene` owns `LayerStore` + `StyleBind` by value; ingest/query/edit take store (and style when needed) |
| Nesting | One level under `document/` only |
| Behavior | No paint / open / seed / extent semantics change |
| Docs | Revise this living § + module notes; no new dated twin |

Plan: [`../plans/2026-09-28-document-map-scene-split.md`](../plans/2026-09-28-document-map-scene-split.md).

### §Present layering（Chromium-style, 2026-09-28）

As-built under `src/content/browser/present/` (module README: [`../../../src/content/browser/present/README.md`](../../../src/content/browser/present/README.md)):

```
present/
  host/                 # BlitFrameCache — present-surface preview (StretchBlt)
  map2d/
    map2d_presenter.*   # Thin facade → gpu + software
    frame/              # carto policy, LayerBatch, tile/mercator math
    gpu/                # Map2dGpuPresent (Pass + MapFrame cache)
    software/           # Map2dSoftwarePainter (GDI)
  scene3d/
    scene3d_presenter.* # Thin facade → atmosphere + gpu + software
    session/            # prefer_scene3d_flycube policy + Scene3dStereoSession
    frame/              # OrbitGeoFrame
    atmosphere/         # AtmosphereSession (Environment + prepare_*)
    gpu/                # Scene3dGpuPresent (mesh + present)
    software/           # Scene3dSoftwarePainter (GDI HUD / wind / logo)
```

Shell includes only facades + `session/` + `host/` headers it needs.

**Atmosphere (3D only):** `AtmospherePanel` → `Browser::scene3d()` setters；geo 对齐见 atmosphere living §China 3D geo-alignment。Map2d 不接 atmosphere。

---

## §Process entry / CLI11（2026-09-28）

### Goal

1. Parse Views PE switches with **CLI11** (third_party header-only), not hand-rolled `wcsstr`.
2. Keep `main.cc` as thin glue: argv → `ViewsLaunchOptions` → `ContentMainParams` → `content_main`.
3. Split browser assembly, atmosphere showcase, and self-test dispatch into `shell/` modules.

### Locked decisions

| # | Decision |
| --- | --- |
| P1 | Library: CLI11 under `third_party/.src/CLI11` (manifest pin, `install_skip`, thin GN like octree). |
| P2 | App options: `app::ViewsLaunchOptions` + `parse_views_launch_options(argc, argv)` in `shell/app/cmdline/`. |
| P3 | Switches: `--type`, `--self-test`, `--atmosphere-showcase`, `--atmosphere-fields` (CLI11 accepts `=` and space forms). |
| P4 | `--type` is parsed in app; set `ContentMainParams::process_type` + `process_type_set=true`. |
| P5 | `content_main`: if `process_type_set` use the field; else fall back to `ProcessTypeFromCommandLine` (tests / legacy). |
| P6 | `ViewsContentHost` holds `ViewsLaunchOptions`; `shell/app/browser_main` calls `run_browser_main(params, options)` which constructs `Browser` (§Chromium Browser / BrowserView). |
| P7 | Showcase body stays in `shell/showcase/`; self-test in `shell/self_test/`; both take `Browser&` after P2. Behavior and exit codes unchanged. |
| P8 | No global CommandLine singleton. Public namespace stays `app` (two layers). |
| P9 | Git: work on `master`. |

### Data flow

```
wWinMain → parse_views_launch_options → ContentMainParams{process_type_set}
  → content_main(ViewsContentHost{options})
      browser → run_browser_main → Browser → owns BrowserView
           → showcase | self_test | run_loop
```

### Non-goals

- Do not rewrite showcase GPU/BMP logic.
- Do not change renderer/gpu entry bodies.
- Do not add Qt or a second CLI library.

---

## §Chromium Browser / BrowserView（Approach 1, 2026-09-28）

**Locked package:** Scope C + Ownership A + Directory A + Public API A.  
User approved **Approach 1** with **parallel landing**.  
**Stack contract (Chromium map, full deps, gaps, S1–S5):** see **§Chromium-style app/views layering**.

### Goal

Split the product shell like Chromium: a **`Browser`** controller owns session state; a **`BrowserView`** owns only the Views `Widget` tree. Process entry constructs `Browser`, which owns `BrowserView`.

### Locked decisions

| # | Decision |
| --- | --- |
| B1 | Target tree under `shell/` is Directory A (below). `document/` / `camera/` / `present/` / `input/` unchanged **siblings**. No shim headers. **Camera must not move under shell.** |
| B2 | Ownership A: `Browser` owns session objects listed below; `BrowserView` owns only the Widget tree and holds `Browser*`. |
| B3 | Lifecycle: `run_browser_main` → `Browser` → owns `BrowserView`. |
| B4 | Deps: `shell/ui` → `shell/browser` → `{document,camera,present,input}`. Never `present` → `shell`. `browser` must not include concrete UI widget headers (`BrowserWindow` abstract / callbacks OK). |
| B5 | Public API A: controller accessors and command/session APIs live on `Browser`. `BrowserView` exposes UI accessors only. Public namespace stays `app`. |
| B6 | Scope C: reshape covers process entry (`app/`), controller (`browser/`), UI (`ui/`), plus existing `showcase/` and `self_test/`. Menus / AM Box / paint rules stay as §2–§4 and map2d-frame. |
| B7 | Git: work on `master`. No feature branch. No new dated twin for this split. |

### Target `shell/` tree

```
src/app/views/shell/
  app/          # browser_main, ViewsContentHost, cmdline/
  browser/      # Browser controller + commands/ + nav/ + plugin/
  ui/           # BrowserView + pages/ + panels/
  showcase/
  self_test/
```

Matches §3 and §Chromium-style app/views layering. Capability dirs stay siblings of `shell/`.

### Ownership

| Owner | Owns |
| --- | --- |
| `Browser` | `MapScene`, `ViewFrame`, `OrbitFrame`, `Map2dPresenter`, `Scene3dPresenter` (+ stereo), `BlitFrameCache`, `MapHwndGestures` (×3), `ViewNavigation`, `PluginShell`, `ViewHost` / `MapContents`, and `unique_ptr<BrowserView>` |
| `BrowserView` | Widget tree (menus, docks, tabs, status). Holds `Browser*`. UI accessors only (`catalog_view`, `status_bar`, `feature_info`, hwnd, contents_view, …) |

Public controller API on `Browser` (illustrative): `document()`, `view_frame()`, `orbit_frame()`, `map2d()`, `scene3d()`, `run_tool_command`, `refit_active_view`, `refresh_inspectors`, `apply_atmosphere_fields`, self-test hooks, tab/viewport accessors, `prepare_close` / teardown.

`self_test` and `showcase` take `Browser&`.

### Dependency direction

```
shell/ui  →  shell/browser  →  document/ | camera/ | present/ | input/
shell/app →  shell/browser  (constructs Browser; thin content_main host)
```

`browser` does not depend on concrete `shell/ui` widget types (target; see Gaps in §Chromium-style app/views layering). Prefer a `BrowserWindow`-like abstract surface or callbacks for UI notifications.

### Migration phases

| Phase | Work | Status |
| --- | --- | --- |
| **P1** | Add `app::Browser`; move session ownership from `BrowserView` to `Browser`; `Browser` owns `unique_ptr<BrowserView>`; `run_browser_main` constructs `Browser` | Landed (fields on Browser) |
| **P2** | Callers: `self_test` and `showcase` take `Browser&` | Landed |
| **P3** | Directory reshape to Directory A; colocated includes; no forwarding headers | Landed (paths) |
| **P4** | Harden: nav/catalog/document/tool/extent on `Browser`; `BrowserUiDelegate` (no `browser.cc`→`BrowserView`); GN `:shell_ui`→`:shell_browser`; no `friend BrowserView` | **Landed** |
| **P5** | Living spec + README shell blurb | This doc; README already points at Directory A |

Phases may land in parallel where paths do not conflict; serialize edits to `browser.*` / `browser_view.*`.

### Non-goals

- Do not change menu ids or AM Box grouping from §2–§4.
- Do not move `document/` / `camera/` / `present/` / `input/` under `shell/` (camera stays sibling of shell until §Content sink lands).
- Do not introduce Qt or a second widget kit.
- Do not change paint results or GDI/GPU fallback rules (map2d-frame living).
- Do not open a new dated spec for this layout.
- Do not add compat shims at old paths.

---

## §Content sink — Chromium `chrome` vs `content`（2026-09-28）

**Status:** landed (directory big-bang + `MapSession` ownership fold).
**Supersedes** capability physical roots under `src/app/views/{document,camera,present,input}` from §Capability / §Chromium-style app/views layering.

### Locked decisions

| # | Decision |
| --- | --- |
| C1 | End-state: `src/app/views` ≈ Chromium `chrome` — only `main.cc` + `shell/`. |
| C2 | `document/` + `camera/` + present facade/frame/session/host + `input/` → `src/content/browser/{document,camera,present,input}/`. |
| C3 | Software **paint** TUs stay under `content/browser/present/*/software/` (member TUs of presenters / painters). GPU under `*/gpu/`. **Forbidden:** `src/render` including or depending on `content`. |
| C4 | Public namespaces for moved types: `content` (internals `content::detail`). `app` keeps shell only (`Browser`, `BrowserView`, commands chrome, PluginShell, cmdline). |
| C5 | Ownership: `content::MapSession` owns `MapScene`, frames, presenters, gestures, ViewHosts, and `MapContents*`. `app::Browser` owns `MapSession` + `PluginShell` + chrome. **Not** folded into `MapContentsImpl` (that type stays OOP/GPU pipe only; keeps C6). |
| C6 | GN: capability `source_set`s under `//src/content` (`:map_session` pulls `:map_scene`, `:map_camera`, `:map_present`, `:scene3d_present`, `:map_hwnd_gestures`). Paint compiled inside `:map_present` / `:scene3d_present`. **Do not** absorb heavy present/GDI into `content.dll`. `render` must not depend on `content`. |
| C7 | No forwarding headers at `src/app/views/{document,camera,present,input}/`. Update includes in the same change. |
| C8 | Menu ids / AM Box / paint fallback semantics unchanged. Git: `master` only. |
| C9 | Landing style: **directory big-bang** (Approach 2) — one coherent move of the four trees + paint split; parallel agents on disjoint paths OK. |

### Target tree

```
src/app/views/
  main.cc
  shell/                    # chrome only (Browser owns MapSession)

src/content/browser/
  map_session.*             # owns document/camera/present/input + MapContents*
  document/                 # MapScene
  camera/                   # ViewFrame, OrbitFrame, ViewNavigation
  present/                  # facade + frame + session + host (no paint/)
  input/                    # MapHwndGestures

  # paint/ colocated under present/map2d|scene3d (not under src/render)
```

### Dependencies

```
shell/ui → shell/browser → //src/content:map_session
map_session → {map_scene, map_camera, map_present, scene3d_present, map_hwnd_gestures, view_host}
present → shell          FORBIDDEN
app/views capability roots FORBIDDEN after land
```

### Checklist

1. [x] Physical move + include/namespace rewrite (no shims).
2. [x] GN labels under content; software/gpu TUs colocated under `content/browser/present/*/`; app/views BUILD.gn drops capability sources; `render` → `content` forbidden.
3. [x] `Browser` / shell_ui / tests compile; `build.bat views` + `map_scene_test` / `view_navigation_test` / `scene3d_presenter_test` green.
4. [x] `src/app/views/README.md` + this § Updated.
5. [x] Fold session ownership into `content::MapSession`; Browser thinned to chrome + PluginShell + MapSession.
6. [x] `docs/build/src-layout.md` Hosted map row mentions browser capability dirs + `MapSession`.

## §GIS panels A+B+C（2026-09-28）

**Status:** active  
**Updated:** 2026-09-28  
**Toolkit contract:** this file § GIS panels A+B+C + archive twin [`../archive/specs/2026-09-13-ui-views-controls-design.md`](../archive/specs/2026-09-13-ui-views-controls-design.md)  
**Plan:** [`../plans/2026-09-28-gis-panels-abc.md`](../plans/2026-09-28-gis-panels-abc.md)

Shell owns inspector TabStrip placement and `shell_panels.cc` wiring. Panels stay string/callback-only.

| Milestone | Inspector tabs (additive) | Wire |
| --- | --- | --- |
| M1 | Measure, Selection | mode → tool command; selection cmds → Browser / MapScene |
| M2 | LayerProperties (Symbology+Source), Legend | apply → style write-back; legend from `LegendSnapshot` strings |
| M3 | SpatialAnalysis (replaces Processing as primary) | run → existing PluginHost / OpsRunner path with panel `distance` param; history process-local |

Host wiring landed (2026-09-28): Measure armed → draw tools + draft consume (no edit append); Selection clear/zoom/invert/export; Symbology → `MapScene::set_style_document`; Legend sync + `set_layer_visible`; inspector sync refreshes Selection/Legend/LayerProps.

`ProcessingPanel` may remain constructed for delegate access / tests; UI primary is SpatialAnalysis. Menu / AM Box may use `panel.measure.show` / `panel.selection.show` / `panel.symbology.show` / `panel.analysis.show` to `set_active` the matching tab.

Non-goals unchanged for layout composer / topology / network.

## §Diagnostic Tools（2026-09-28）

**Status:** active  
**Updated:** 2026-09-28  
**Plan:** [`../plans/2026-09-28-render-trace-profiler.md`](../plans/2026-09-28-render-trace-profiler.md) (timing) + memory § in hybrid; console: [`2026-09-28-debug-console-design.md`](../archive/specs/2026-09-28-debug-console-design.md) + [`../plans/2026-09-28-debug-console.md`](../plans/2026-09-28-debug-console.md)

VS-style bottom **Diagnostic Tools** dock (replaces standalone Debug Console + Inspector `RenderTrace`):

| Item | Choice |
| --- | --- |
| UI | `ui::views::DiagnosticToolsPanel` bottom dock |
| Tabs | `Output` \| `Console` \| `CPU` \| `Memory` |
| Shared bar | Record / Stop / Clear / Export / Armed / Track allocs / Echo→Output |
| Output | LogSink only (`DebugConsolePanel` kOutput); startup `LOGGING` appears here |
| Console | Input + echo; Agent cmd/py/sdbd (`DebugConsolePanel` kConsole) |
| CPU | Embedded `RenderTracePanel` Gantt (startup + Map2d + Scene3d swimlanes) |
| Memory | Stats strip + `ph:"C"` counters via `base::sample_memory_counters_to_process_trace` |
| Menu | View → Toggle Diagnostic Tools (`view.debug_console`) |
| Inspector | **No** RenderTrace tab |

### Always-on auto-collect（2026-09-28）

| Axis | Choice |
| --- | --- |
| Trigger | **Process start** (`base::start_always_on_diagnostics` from `wWinMain`) |
| Startup log | Full `BASE_TRACE_EVENT(..., "startup")` tree + matching `LOGGING` → `LogSink` → Output |
| Perf Gantt | Always-on `process_trace`; CPU tab auto-refresh (~500ms while tools visible) |
| Memory | 500ms sampler thread + `AllocationTracker::enable` at bootstrap; Memory tab auto-refresh |
| Escape | Record (clear+continue) / Stop / Armed / Track allocs still work |

Bootstrap API: `src/base/trace/diagnostic_bootstrap.{h,cc}`. `set_tracing_enabled(true)` clears only on **off→on** so always-on startup spans survive UI re-arm; explicit Record clears first.

## §Debug Console（2026-09-28）

**Status:** accepted (UI slice folded into Diagnostic Tools Output/Console)  
**Owning spec:** [`2026-09-28-debug-console-design.md`](../archive/specs/2026-09-28-debug-console-design.md)  
**Plan:** [`../plans/2026-09-28-debug-console.md`](../plans/2026-09-28-debug-console.md)

Bottom-dock **Debug Console** capabilities now live under Diagnostic Tools tabs. Shell responsibilities:

| Item | Choice |
| --- | --- |
| UI | Bottom dock `DiagnosticToolsPanel` (Output + Console panes) |
| Menu | View → Toggle Diagnostic Tools (starts `DebugAgent` if needed) |
| Layering | Panel → Agent / `LogSink` only; no direct `SdbdClient` from views |
| Trace | CPU/Memory tabs share `base::process_trace` (not merged with LogSink) |

Full protocol, LogSink, Python worker, and sdbd bridge live in the owning spec.

## §GIS Python Console + analysis results（2026-09-28）

**Status:** active  
**Updated:** 2026-09-28 — dual-runtime A (embed + worker parallel); prior phase-1 analysis  
**Plan:** [`../plans/2026-09-28-gis-python-spatial-analysis.md`](../plans/2026-09-28-gis-python-spatial-analysis.md)  
**Related:** plugin-host §smartgis.gis bindings + **§Python dual-runtime**; algorithm-layer OpsRunner

### Locked choices

| Item | Choice |
| --- | --- |
| Dual runtime | **A** — embed + OOP worker in parallel (see plugin-host §Python dual-runtime) |
| Console Python | Real CPython syntax; default **in-process** via `plugin::PythonRuntime::eval` |
| OOP worker | DAP / Pyright / long `:run` / crash-isolated heavy scripts; not the default GIS REPL |
| Bindings | Shared `smartgis` surface; phase-1 = `gis.analysis` + `gis.scene` + `gis.style` + `ui.config` + `debug` + host `contribute_*` |
| Panel | `SpatialAnalysisPanel` primary; Python plugins add analysis docks/dialogs |
| Scene | One `MapScene`; Map/Data/3D tabs via `present_mode` |
| Style / config | `StyleDocument` load/clear + `ThemeService` packs |
| Profile | `smartgis.debug` → Diagnostic Tools CPU (`process_trace`) |
| Results | `{ok, text, feature_count}` + map write-back (temp GeoJSON → document open) |
| Forbidden | Qt / PyQt; second geometry kernel; fake Python DSL |

### Phase-1 Console surface

```python
import smartgis.gis.analysis as a
import smartgis.gis.scene as scene
import smartgis.gis.style as style
import smartgis.ui.config as cfg

a.ops()
print(scene.layers(), scene.present_mode())
scene.set_present_mode("scene3d")
style.load(r"C:\data\map.style.json")
cfg.set_theme("light")
```

Bare Console lines (no leading `:`) and `:py …` both use `DebugAgentHost.py_eval` when bound; else fall back to OOP spawn (legacy).

### Shell wiring

`PluginShell` owns `PythonRuntime`. On Diagnostic Tools enable (and at shell init when Python is available): `init` → bind `PluginHost` → set `GisConsoleBridge` (write/load active GeoJSON, refresh) → `DebugAgentHost.py_eval = runtime.eval`.

---

## §Console coverage + performance（2026-09-28）

**Status:** active  
**Plan:** [`../plans/2026-09-28-debug-console.md`](../plans/2026-09-28-debug-console.md) (L0/L1/L2 coverage + bench checklists)  
**As-built:** [`../../build/ui-testing.md`](../../build/ui-testing.md) + [`../../../testing/README.md`](../../../testing/README.md)

Coverage and timing for the Debug Console / `DebugAgent` command surface — **not** a replacement for Views L0/L1/L2 pixel or interactive harness.

### Locked choices (brainstorming)

| Axis | Choice |
| --- | --- |
| Scope | **C** — Agent coverage + Console-driven app paths |
| Industry | **D** — QGIS-like command surface; GDAL-like data timings + MapLibre-like viewport timings (**no** absolute cross-product compare) |
| Run | **A+B+C** — headless matrix + bench; OpenCppCoverage optional; shell e2e / `--self-test-console` + JSON |
| Data | **C+D** — synthetic fixture + `china_map_samples` + DEM/tile viewport soft |

### Architecture L0 / L1 / L2

| Layer | Target | Gate | Output / notes |
| --- | --- | --- | --- |
| **L0** | `content_console_coverage_test` | `build.bat te` (`//:test_all`) | Headless Agent / command matrix |
| **L1** | `content_console_bench` | `build.bat b` (`//:benchmark_all`) | Writes `console_bench.json` (timings; soft thresholds) |
| **L2** | `SmartGisViews.exe --self-test-console` | shell e2e / self-test | Console-driven app smoke; optional OpenCppCoverage via `testing/scripts/open_cpp_coverage_console.ps1` |

OpenCppCoverage is **optional** and must **not** block default `build.bat te`. Sources filter for the console script: `src/content/browser/debug` + `src/base/log`; HTML / cobertura under `out/Debug/coverage/console/`.

### Non-goals

- Do not require OpenCppCoverage for local `te` or default CI `test_all`.
- Do not invent absolute SLAs against QGIS / GDAL / MapLibre products.
- Do not merge this gate with Views interactive harness or `views_bench` (separate surfaces).

---

## §Declarative markup subdirectory（2026-09-28）

`src/ui/views/markup/` is partitioned like `kernel/` / `primitives/` (colocated headers; no root shims; namespace stays `ui::views`):

| Subdir | Owns |
| --- | --- |
| `style/` | `CssParser`, FlexStyle / `StyleSheet` |
| `document/` | `MarkupDocument`, `NamedViewMap` |
| `layout/` | `YogaLayoutManager` |
| `factory/` | `ControlFactory` registry, `PlaceholderView`, `register_markup_tags.*`, `control_factory_default.cc` |
| `loader/` | `load_markup` / `MarkupRoot` |
| `testdata/` | test-only `.ui.xml` / `.ui.css` samples |

Product dialog and GIS panel assets live in **`src/ui/resources/<area>/`** (nested by responsibility, aligned with `ui/gis/{dialogs,catalog,inspect,shell,style,analysis,debug}` plus `toolkit/` for generic views dialogs). GN `:markup_resources` copies each area to shared **`out/ui/<area>/`** (`$root_out_dir/../ui`, sibling of Debug/Release — same pattern as `out/data/`) plus flat `markup/testdata/` samples into `out/ui/`. Call sites use relative names: `load_markup("dialogs/create_map.ui.xml")`, `load_markup("inspect/measure_panel.ui.xml")`. Resolver searches `<exe>/../ui/<rel>`, `<exe>/ui/<rel>`, and `src/ui/resources/<rel>`.

**Panel markup contract:** C++ panel constructs via `load_markup` + id bind + `FillLayout` (same as product dialogs). Dynamic rows/trees stay on `set_*` APIs. Nested C++ children (TabStrip pages, History) mount into `panel` hosts (`tabs_host` / `history_host`). Landed: StatusBar, Measure, Selection, Legend, Symbology, LayerProperties, FeatureInfo, AttributeTable, Catalog, SpatialAnalysis, Processing, History, Atmosphere. Intentionally C++: Ambox (dynamic toolbox), DiagnosticTools (composed docks), ChartView (paint-only).

**Plan:** [`../plans/2026-09-28-gis-resources-markup.md`](../plans/2026-09-28-gis-resources-markup.md).

Include prefix: `"ui/views/markup/<subdir>/…"`. GN targets `:views_control_factory` / `:views_markup` / `:views_sources` keep the same layer edges; only source paths move.

Archive twin: [`../archive/specs/2026-09-28-views-declarative-markup-design.md`](../archive/specs/2026-09-28-views-declarative-markup-design.md).

---

## §Custom frame + ThemeService（2026-09-28）

**Goal:** Unify window chrome with the Views shell (full client-side decorations) and support switchable, extensible theme packs (first ship: Dark + Light).

**Locked choices**

| # | Decision |
| --- | --- |
| 1 | Full self-drawn title bar (caption + min/max/close). No DWM-only tint of the system caption. |
| 2 | Theme API is pack-based (`ThemeService` registers by id). First packs: `dark` (default), `light`. |
| 3 | Switch UI: View menu quick items + Preferences dialog listing registered packs. Persist id under `%LOCALAPPDATA%\SmartGIS\ui_theme_id.txt`. |
| 4 | Scope: `SmartGisViews` main window **and** toolkit `Dialog` / product dialogs share the same `FrameView` + `frame_kind=custom`. |
| 5 | No OS light/dark auto-follow in this slice. `.ui.css` token bridge is a later follow-up (not a dual source now). |
| 6 | Namespace stays `ui::views`. New files under `kernel/frame/` and `kernel/shell/theme_service.*`. |

**Architecture**

| Piece | Path | Role |
| --- | --- | --- |
| `FrameView` / `CaptionButton` | `src/ui/views/kernel/frame/` | Caption strip, drag region, window buttons; client slot below |
| `Theme` + `ThemeService` | `src/ui/views/kernel/shell/` | Color snapshot + pack registry / observers / persist |
| `Widget::InitParams::frame_kind` | `kernel/widget/` | `kSystem` (default, tests) / `kCustom` (`WM_NCCALCSIZE` client=window + `WM_NCHITTEST`) |
| Product shell | `app/views/shell/ui/browser_view.*` | Root is `FrameView`; menus invoke theme commands |
| Dialogs | `ui/views/dialogs/dialog.*` | `frame_kind=custom`; caption without maximize when owned |

**Hit-test contract:** Edges → resize HTs; caption (excluding buttons) → `HTCAPTION`; caption buttons / client → `HTCLIENT` so Views receives clicks. Custom-frame CreateWindow size equals client size (no `AdjustWindowRect` expansion).

**Non-goals:** Acrylic/Mica materials; per-control style sheets; Qt/WinUI frames; auto System theme.

**Plan:** [`../plans/2026-09-28-views-csd-theme.md`](../plans/2026-09-28-views-csd-theme.md).

---

## §Global theme paint — PainterRegistry + PaintDelegate（2026-09-28）

**Goal:** Process-wide themed self-draw for all Views controls: type-keyed `Painter` registry + per-instance `PaintDelegate`, shared by `SmartGisViews`, `UiDesigner`, dialogs, and in-process plugins.

**Locked choices**

| # | Decision |
| --- | --- |
| 1 | Approach: string `paint_role()` + `PainterRegistry` (not RTTI). |
| 2 | Instance `PaintDelegate` (`paint_before` / `paint_after`) non-owning on `View`. |
| 3 | Paint pipeline in `View` recording path: before → registry painter **or** legacy `paint_self` → after. |
| 4 | Builtin default painters for all toolkit primitives + `FrameView` / `CaptionButton` / `Splitter`; map viewport keeps dedicated `paint_self` (role `""`). |
| 5 | Theme packs stay color snapshots; switching theme invalidates commands / `schedule_paint`. Replacing a type skin = `register_painter`. |
| 6 | Plugin API: C++ plugins call `PainterRegistry` (optionally tagged by plugin id for withdraw). `PluginHost::contribute_painter` records installer + withdraw hook (content stays views-free). |
| 7 | `UiDesigner` uses `FrameView` + `frame_kind=custom` + theme menu like the product shell. |
| 8 | Non-goals: `.ui.css` theme tokens; Acrylic/Mica; per-instance theme pack; Qt. |

**Architecture**

| Piece | Path | Role |
| --- | --- | --- |
| `Painter` / `PaintDelegate` / `PainterRegistry` | `kernel/paint/` | Type registry + pipeline helpers |
| `View::paint_role` / `set_paint_delegate` / pipeline | `kernel/view/` | Dispatch during DisplayList record |
| Default painters | `kernel/paint/register_default_painters.*` | RoleForwardPainter → `paint_self` |
| `register_builtin_painters()` | `primitives/` or `kernel/paint/` | Idempotent builtin install |
| Plugin withdraw | `PainterRegistry::withdraw_plugin` + `PluginHost` hook | Unload restores builtins |
| Hosts | `app/views` shell, `app/ui_designer` | CSD + ThemeService consumers |

**Paint roles (stable ids):** `button`, `checkbox`, `radio_button`, `label`, `textfield`, `combobox`, `slider`, `menu_bar`, `context_menu`, `scroll_view`, `tab_strip`, `table_view`, `tree_view`, `splitter`, `frame`, `caption_button`.

**Plan:** [`../plans/2026-09-28-views-global-theme-paint.md`](../plans/2026-09-28-views-global-theme-paint.md).

---

## §ui/gis layering move（2026-09-28）

**Decision:** Product GIS chrome lives under `src/ui/gis/` (directory layering) but shares the **same** PE and export as the rest of `//src/ui`.

| Layer | Path | DLL / label |
| --- | --- | --- |
| Paint | `src/ui/gfx/` | `ui_views.dll` (`:gfx`) |
| Toolkit | `src/ui/views/` (kernel, primitives, markup, Dialog shell, map hang) | `ui_views.dll` |
| GIS chrome | `src/ui/gis/` (panels + AddBasemap/AttStruct/Create*) | same `ui_views.dll` (`//src/ui/gis:gis` → `:ui_views`) |
| Product host | `src/app/views/` | `SmartGisViews.exe` |

**Keep in `views/dialogs/`:** `Dialog`, `MessageBox`, `FilePicker`, `InputTextDialog`, `SelectOneDialog` (generic toolkit; no GIS types).

**Moved to `ui/gis/`:** former `views/gis/**` panels; product dialogs `AddBasemap` / `AttStruct` / `CreateDatasource` / `CreateLayer` / `CreateMap`.

**Namespace:** stay `ui::views` for moved types (directory + `"ui/gis/…"` includes first). **One** export for all of `//src/ui`: `UI_EXPORT` / `UI_EXPORTS` (`ui/ui_export.h`). Mass `ui::gis` rename is a later optional pass.

**Markup:** ControlFactory GIS tags remain **placeholders** registered in views (`register_gis_placeholder_markup_tags`). Real panel instances stay C++-constructed by the app (optional real-tag registration TU under `ui/gis` later if markup embeds live panels). Panel **chrome layout** migrates to `src/ui/resources/<area>/*.ui.xml` (Wave1 landed: StatusBar / Measure / Selection / Legend).

**Resources nest:** see §Declarative markup subdirectory (same file) + [`../plans/2026-09-28-gis-resources-markup.md`](../plans/2026-09-28-gis-resources-markup.md).

**GN:** `views` must not *list* GIS sources in its own `source_set`s; `:ui_views` `deps` `//src/ui/gis:gis_sources`. App / tests / plugin widgets `public_deps` `//src/ui/gis:gis` (forwards to `:ui_views`). Root `//:ui_views` group pulls `:gis`.

As-built: [`docs/build/ui-views-skia.md`](../../build/ui-views-skia.md), [`src/ui/gis/README.md`](../../../src/ui/gis/README.md), [`src/ui/views/README.md`](../../../src/ui/views/README.md), [`src/ui/resources/README.md`](../../../src/ui/resources/README.md).

---

## §UI interactive harness + overlay bench（2026-09-28）

**Status:** accepted  
**Plan:** [`../plans/2026-09-28-ui-interactive-overlay-bench.md`](../plans/2026-09-28-ui-interactive-overlay-bench.md)  
**As-built alignment:** [`../../build/ui-testing.md`](../../build/ui-testing.md) **P2 / L1** (process-in interactive sequences + overlay bench; not a replacement for L1′ `--self-test` or L2 pixel).

Chromium-style **dual layer** for Views UI validation: in-process C++ harness (synthetic events + overlay scenes) plus optional **DebugAgent** `ui.*` RPC for console / Python orchestration when a live `SmartGisViews.exe` is running.

### Architecture (brief)

```
                    ┌─────────────────────────────────────────┐
  L1 headless/      │  views_interactive_tests.exe            │
  in-process        │  EventGenerator → ViewsTestBase         │
                    │       ↓              ↓                  │
                    │  View tree      OverlayScene (L2 shell) │
                    └─────────────────────────────────────────┘
                                        │
                    ┌───────────────────┴───────────────────────┐
  Live product      │  SmartGisViews.exe + DebugAgent (opt-in)  │
  orchestration     │  ui.click / ui.wait / ui.overlay.* (NDJSON) │
                    │       ↑                                     │
                    │  tools/debug/scripts/ui_smoke.py            │
                    └───────────────────────────────────────────┘

  views_bench.exe — perf microbench (PaintCommit / compositor path)
```

| Piece | Path | Role |
| --- | --- | --- |
| Harness core | `src/ui/views/testing/harness/event_generator.{h,cc}` | Synthetic `MouseEvent` / `KeyEvent` / pump |
| Fixture base | `src/ui/views/testing/harness/views_test_base.{h,cc}` | Headless or minimal HWND fixture, `Click` / `Wait` / `FindViewById` |
| Overlay bench | `src/ui/views/testing/harness/overlay_scene.{h,cc}` | Deterministic overlay layers for shell paint assertions |
| L1 target | `//src/ui/views:views_interactive_tests` → `out/views_interactive_tests.exe` | Behavioral matrix (required gate for harness landing) |
| Perf target | `//src/ui/views:views_bench` → `out/views_bench.exe` | Compositor / `PainterRegistry` / `PaintCommit` timing |
| Agent RPC | `DebugAgentHost` `ui_*` hooks + `debug_agent.cc` dispatch | `ui.find` / `ui.click` / `ui.type` / `ui.dump_tree` / `ui.overlay_stats` |
| Python smoke | `tools/debug/scripts/ui_smoke.py` | Discovery file → Agent → scripted shell steps |
| Coverage (optional CI) | `testing/scripts/open_cpp_coverage_views.ps1` | OpenCppCoverage export; **does not** block default `build.bat te` |

### Locked decisions

| # | Decision |
| --- | --- |
| 1 | **Dual layer:** L1 = C++ `EventGenerator` + `ViewsTestBase` + `OverlayScene`; live runs may additionally drive the same semantics via DebugAgent **`ui.*`** (console + Python), not a second widget kit. |
| 2 | **Wave1 (overlay shell):** Assert compositor path, `PainterRegistry`, and **`PaintCommit`** overlay recording on **shell chrome only** (MenuBar, Tab, StatusBar, dock chrome). Map viewport pixels stay out of L2 overlay goldens. |
| 3 | **Wave2 (semantic map chrome):** `MapViewport` / **`AuxOverlay`** semantic hooks (extent string, ready marks, aux layer visibility). **No map pixels in L2** — same rule as [`ui-testing.md`](../../build/ui-testing.md) L2. |
| 4 | **Coverage:** A **behavioral matrix** documents every harness scenario (control × action × assertion). **OpenCppCoverage** is an **optional** CI gate via script; default **`build.bat te`** stays green without it. |
| 5 | **GN targets:** `views_interactive_tests` (L1 behavioral); `views_bench` (perf). Harness lives under **`src/ui/views/testing/harness/`** (colocated headers). |
| 6 | **Stack:** Views + Skia only; **no Qt**. New-tree functions **`snake_case`**; public namespace **`ui::views`** (helpers in `ui::views::detail` if needed). |
| 7 | **Git:** work on **`master`**; parallel agents use non-overlapping paths under `testing/harness/`, `tools/debug/`, `content/browser/debug/`. |
| 8 | **Regression policy:** Do not shrink L0 `views_unittests` or L2 `views_pixel_tests`; interactive matrix **adds** L1 coverage aligned with **P2** in `ui-testing.md`. |

### Non-goals

- Do not introduce Qt, Squish, or WinAppDriver as the primary interactive driver.
- Do not add map render pixels to overlay bench or default L2 PNG baselines.
- Do not require OpenCppCoverage for local `build.bat te` or default CI `test_all`.
- Do not replace `SmartGisViews.exe --self-test` (L1′); harness complements it with finer-grained, headless-friendly sequences.
- Do not expose `ui.*` Agent RPC on non-loopback interfaces.

### Wave1 checklist (shell overlay)

- [x] `OverlayScene` records shell-only layers tied to compositor / `PaintCommit`.
- [ ] Matrix rows: MenuBar open/close, Tab switch, StatusBar field update, Theme switch invalidates paint.
- [x] `views_interactive_tests` registered in `//src/ui/views/BUILD.gn` (+ `test_shell`); harness documented in `src/ui/views/README.md` / `ui-testing.md`.
- [x] At least one green L1 scenario (`views_interactive_tests: OK`); include in default `te` when matrix file is complete.
- [x] DebugAgent `ui.*` + `tools/debug/scripts/ui_smoke.py` (Wave1 click/type/dump; `overlay_stats` still unavailable).
- [x] `views_bench` reports ns/op for click + OverlayScene commit + ShellCompositor path.

### Wave2 checklist (MapViewport / AuxOverlay semantic)

- [ ] Harness waits on semantic marks (`wait_ready`, aux overlay visibility) without reading map bitmaps.
- [ ] Agent `ui.wait_map_ready` / `ui.aux_overlay_state` mirror C++ `ViewsTestBase` helpers.
- [ ] Matrix rows: Map/Data/3D tab + aux overlay toggle; assertions on strings / marks only.
- [ ] Markup/`NamedViewMap` id for `ui.find`; shell-side overlay metrics for `ui.overlay_stats`.

---

## §UI visual forensics (A+C)（2026-09-28）

**Status:** accepted  
**Plan:** [`../plans/2026-09-28-ui-visual-forensics.md`](../plans/2026-09-28-ui-visual-forensics.md)  
**As-built alignment:** [`../../build/ui-testing.md`](../../build/ui-testing.md) **L1c** (failure capture + optional record-all; not a gate for default `build.bat te`).

**Scheme 1 (approved):** complement existing semantic / `layout_check` gates with **Mode A** (automatic failure dumps) and optional **Mode C** (Python forensics driver + offline frame analysis). Map GPU pixels are never golden targets; dumps are **shell offscreen capture only**.

### Modes

| Mode | Trigger | Output | Default `te` |
| --- | --- | --- | --- |
| **A — failure capture** | Any L0/L1/L1′/L2 gate fails on semantic or `layout_check` | `out/ui_forensics/<run_id>/` | On failure only |
| **A (dev override)** | `SMT_UI_FORENSICS=1` | Same directory layout even when gates **pass** | Opt-in local |
| **C — record / analyze** | `tools/debug/scripts/ui_visual_forensics.py` | Reads last run dir or drives live Agent loop | **Not** in default `te` |

### Mode A artifact layout (`out/ui_forensics/<run_id>/`)

| File | Content |
| --- | --- |
| `frame_NNNN.png` | Sequential shell offscreen frames (WIC / existing capture path) |
| `manifest.json` | Run id, timestamps, view ids, optional layout metrics (TabStrip cell widths, Gantt lane gaps, Ambox button bounds) |
| `layout_issues.txt` | `collect_layout_violations` / `layout_check` text (same as failing assertion) |

**Never** write map viewport GPU bitmaps into this tree as regression goldens. Map / Scene readiness stays on semantic marks (`map-frame-ok`, `scene-frame-ok`) per L1′.

### Mode C — `ui_visual_forensics.py`

Path: `tools/debug/scripts/ui_visual_forensics.py` (**not** wired into default `build.bat te`).

| Flag | Behavior |
| --- | --- |
| `--record` | After a failing run, attach to the latest `out/ui_forensics/<run_id>/` (or explicit run dir) and append Agent-driven frames (fail-only workflow) |
| `--record-all` | Drive a live DebugAgent capture loop (dev / CI optional); produces or extends forensics dirs |
| `--analyze <dir>` | Offline pass over `frame_*.png` + `manifest.json`: sibling **AABB overlap**; **TabStrip** cell vs text width; **Ambox** Tools button **y** spacing; **DiagnosticTools** CPU **Gantt** lane minimum gap when metrics present in manifest |

### Scenario matrix (forensics targets)

| Area | Failure class | Assertion style |
| --- | --- | --- |
| Catalog — Layers / Sources / Maps tabs | Label pile-up / clipped tab text | Shell capture + manifest tab metrics; semantic tab labels |
| Ambox — Tools buttons | **y-collapse** (stacked controls) | AABB + min vertical gap in `--analyze` |
| DiagnosticTools — CPU Gantt | Lanes visually merged | Lane min gap from manifest when exported |
| Map ↔ Data ↔ 3D switch | Wrong host / blank chrome | Semantic marks only (`map-frame-ok` / `scene-frame-ok`); **no** map pixel golden |

### Reuse (no parallel stack)

| Existing piece | Forensics use |
| --- | --- |
| `capture_view_tree` / shell offscreen WIC | `frame_NNNN.png` source |
| `ui/views/kernel/layout_check.h` | `layout_issues.txt` + default gates unchanged |
| DebugAgent `ui.*` | Mode C live capture / `--record-all` |
| L0 `views_unittests`, L1 `views_interactive_tests`, L1′ `--self-test`, L2 `views_pixel_tests` | Mode A hooks on existing failure paths only |

### Locked decisions

| # | Decision |
| --- | --- |
| 1 | **Default gates unchanged:** semantic + `layout_check` remain the pass/fail authority; forensics is diagnostic, not a new mandatory gate for `te`. |
| 2 | **Mode A on failure:** any qualifying test failure triggers dump under `out/ui_forensics/<run_id>/` with the three artifact types above. |
| 3 | **`SMT_UI_FORENSICS=1`:** forces dump on pass for local dev; must not be required in CI. |
| 4 | **Shell-only pixels:** offscreen capture of Views chrome; map GPU surface excluded from forensics goldens and from default L2 map baselines. |
| 5 | **Mode C optional:** Python script + flags documented in as-built; **not** part of default `build.bat te`. |
| 6 | **Scheme 1 only:** no separate dated design twin; requirements live in this § and the linked plan. |

### Non-goals

- Qt, Squish, WinAppDriver, or UIA-primary forensics.
- Map render pixels in default L2 or forensics goldens.
- Requiring forensics dumps for `build.bat te` to be green.
- Replacing L1′ `--self-test` or L1 interactive matrix — forensics **augments** failure diagnosis.

### Implementation checklist (summary)

- [x] Wire Mode A dump on failure from L0/L1/L1′/L2 runners (shared `run_id` + manifest schema).
- [x] Honor `SMT_UI_FORENSICS=1` pass-through dump in dev builds.
- [x] Add `tools/debug/scripts/ui_visual_forensics.py` with `--record`, `--record-all`, `--analyze`.
- [x] Export manifest metrics for TabStrip, Ambox Tools, DiagnosticTools Gantt where available.
- [x] Document L1c + runbook in [`ui-testing.md`](../../build/ui-testing.md).

---

## Folded topics (2026-09-28 merge B)

Former hot specs are under `archive/specs/` (`superseded`). **Revise this file** (append `§`) for new requirements in this topic. Do not create a new `YYYY-MM-DD-*-design.md`.

| Former hot spec | Section / note |
| --- | --- |
| [`../archive/specs/2026-09-13-ui-views-controls-design.md`](../archive/specs/2026-09-13-ui-views-controls-design.md) | §Views toolkit / compositor / GIS panels (folded) |
| [`../archive/specs/2026-09-13-ui-views-mfc-migration-design.md`](../archive/specs/2026-09-13-ui-views-mfc-migration-design.md) | §MFC → Views migration (folded) |
| [`../archive/specs/2026-09-14-app-cef-hwnd-host-design.md`](../archive/specs/2026-09-14-app-cef-hwnd-host-design.md) | §rejected CEF host (archived; Views endgame) |
| [`../archive/specs/2026-09-14-ui-leftover-chrome-parity-design.md`](../archive/specs/2026-09-14-ui-leftover-chrome-parity-design.md) | §Leftover chrome parity (folded) |
| [`../archive/specs/2026-09-15-app-cs-winui-host-design.md`](../archive/specs/2026-09-15-app-cs-winui-host-design.md) | §rejected WinUI host (archived; Views endgame) |
| [`../archive/specs/2026-09-19-ui-views-subdir-responsibility-design.md`](../archive/specs/2026-09-19-ui-views-subdir-responsibility-design.md) | §ui/views subdirectory responsibility (folded) |
| [`../archive/specs/2026-09-28-debug-console-design.md`](../archive/specs/2026-09-28-debug-console-design.md) | §Debug console / LogSink (folded) |
| [`../archive/specs/2026-09-28-views-declarative-markup-design.md`](../archive/specs/2026-09-28-views-declarative-markup-design.md) | §Declarative markup XML+Yoga + subdirectory nest (folded) |

