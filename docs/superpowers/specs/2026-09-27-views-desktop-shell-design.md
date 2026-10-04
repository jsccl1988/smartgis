<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/app/views` desktop shell (QGIS-style menus + map context)

**Status:** active  
**Date:** 2026-09-27  
**Updated:** 2026-10-04 — **§Content browser subdirectory tighten** (C11: fold thin `browser/` siblings; flatten `document/` helpers). Prior same day — C10 remaining root TUs into `{bootstrap,contents,catalog,attrs,plugin,session}/` (scheme C; public/ unchanged). Prior 2026-10-03 — `content/browser/present/scene3d/{policy,stereo}` folded into `session/` (scheme C as-built). Prior 2026-10-02 — GIS product modals colocated under `ui/gis/catalog|inspect` (drop flat `dialogs/`; `AttStruct` → `AttributeSchema`). Prior same-day §Horizon product brand; `*Chrome` → `*Composer` / `init_shell` / `ui/views/map/frame` batch rename. Prior same-day §Debug Console D1–D7; UI Views shell HTML; §shell/ui composers; §Shell chrome layout; §Declarative markup; §Startup profile; 2026-10-01 — §Visual review; `ui/views/map` nest; §Chromium Browser plugin writers; §IL interaction recorder. Prior 2026-09-30 — §Shell perf / compositor; §UI visual forensics; §Harness suite loop. Do not open new dated twins.  
**Diagram:** [`../diagrams/ui-views-shell-architecture.html`](../diagrams/ui-views-shell-architecture.html) · [`../diagrams/content-browser-layers.html`](../diagrams/content-browser-layers.html) · [`../diagrams/debug-console-agent.html`](../diagrams/debug-console-agent.html) · as-built process [`../diagrams/views-window-process.html`](../diagrams/views-window-process.html)
**Plans:** [`../plans/2026-09-20-m0-views-main-path.md`](../plans/2026-09-20-m0-views-main-path.md) · [`../plans/2026-09-28-debug-console.md`](../plans/2026-09-28-debug-console.md) · compositor / markup / forensics / harness on later § Plan lines
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
| Product brand | **§Horizon product brand** (this file) | SmartGIS Horizon; tree stays `shell/` |
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
| 14 | Product brand for the destination Views shell is **Horizon** (SmartGIS Horizon). Engineering tree stays `src/app/views/shell/` — do **not** introduce `src/chrome/`. See §Horizon product brand. |

---

## §Horizon product brand（2026-10-02）

**Locked:** the next-generation / modern desktop GIS product line name is **Horizon**.

| Layer | Name | Notes |
| --- | --- | --- |
| Product brand | **SmartGIS Horizon** | External / docs / release talk track for the Views + Skia destination shell |
| Binary (today) | `SmartGisViews.exe` | Keep until an explicit rename PR; brand ≠ PE stem |
| Engineering tree | `src/app/views/shell/` | Maps Chromium’s `chrome/browser` role; directory stays `shell` |
| Toolkit | `src/ui/views` + `src/ui/gis` | Unchanged |
| Common noun “chrome” | UI frame around the map | Code uses `*Composer` / `init_shell` / `ui/views/map/frame`; common noun “shell chrome” may remain in prose |

**Non-goals**

- Do not rename `shell/` → `horizon/` or `chrome/` in this lock.
- `*Chrome` composer types and files were renamed to `*Composer` (2026-10-02 batch).
- Do not reintroduce `src/chrome/` or a WebView2 / `src/web` stack.

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
      browser_view.h / .cc        # layout + thin forwards; owns *Composer helpers
      shell_layout_composer.h / .cc  # ShellLayoutComposer (markup + imperative layout)
      pages/
        map_pages_composer.h / .cc  # MapPagesComposer (tabs/viewports/gestures)
      panels/
        processing_composer.h / .cc
        inspect_composer.h / .cc
        inspector_sync_composer.h / .cc
        debug_console_composer.h / .cc
        atmosphere_composer.h / .cc
        report_panel.h / .cc      # ReportPanel View (not a BrowserView method TU)
    harness/
      showcase/
        atmosphere/
        map2d/
        ui/
        input/
      self_test/
        self_test.h
        probe.h / .cc
        run_self_test.cc
        console_self_test.cc
        shell_ready.cc
        edit_m0.cc
        layers_m1.cc
        navigate.cc
        present.cc
        layout_bounds.cc
        milestones.cc
    util/
      exe_sidecar_path.h

# Landed under content (Approach 2):
#   src/content/browser/document/   MapScene
#   src/content/browser/camera/     ViewFrame, OrbitFrame, ViewNavigation
#   src/content/browser/present/    map2d / scene3d present stack — see §Present
```

Include examples after the shell reshape:

- `app/views/shell/app/browser_main.h`
- `app/views/shell/browser/browser.h`
- `app/views/shell/ui/browser_view.h`
- `content/browser/session/map_session.h`
- `content/browser/document/map_scene.h`
- `content/browser/camera/view_frame.h`
- `content/browser/present/map2d/map2d_presenter.h`
- `content/browser/present/scene3d/scene3d_presenter.h`
- `content/browser/input/map_hwnd_gestures.h`

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
      plugin/                  # PluginShell + analysis_writers (product commit/wire)
    ui/                           # BrowserView only (≈ chrome/browser/ui)
      browser_view.*              # Widget tree + thin forwards; holds Browser*
      pages/                      # MapPagesComposer
      panels/                     # *Composer helpers + ReportPanel
    harness/
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
| Controller logic | Nav / tool / catalog / file / extent on `Browser` (`commands/`, `nav/`); product analysis writers in `plugin/analysis_writers` | Residual: some pages/panels TUs still carry wide UI includes; analysis_writers still large (further product splits optional) |
| Fat `browser_view.cc` | Shell + menus/ambox/status; controller moved; panel/page wire in `*Composer` helpers | Optional: `ShellLayoutComposer` for `build_contents`; `detail/ptr_guard.h` |
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
6. [x] Sync as-built blurbs in `src/app/views/README.md` / `docs/superpowers/src-layout.md` (no new dated specs).
7. [x] S6 — `plugin/analysis_writers.{h,cc}`: product document/scene/analysis commit helpers + `wire_plugin_analysis_writers`; `browser.cc` is lifecycle/chrome only.

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

Plan: [`../plans/2026-09-28-document-map-scene-split.md`](../archive/plans/2026-09-28-document-map-scene-split.md).

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
| P7 | Showcase body stays in `shell/harness/showcase/`; self-test in `shell/harness/self_test/`; both take `Browser&` after P2. Behavior and exit codes unchanged. |
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

## §China product defaults（interactive ↔ showcase, 2026-09-30）

**Status:** active  
**As-built:** `shell/browser/china_product_defaults.{h,cc}`; callers: `Browser::fit_map_extent`, `BrowserView::switch_map_tab`, `--map2d-showcase=china`, `--atmosphere-showcase=full`.

### Locked decisions

| # | Decision |
| --- | --- |
| C1 | Interactive bare launch keeps **FlyCube / RHI** (no `SMT_FORCE_CONTENT_MAPVIEW_2D` / GDI force). Showcase/self-test may still force GDI for BMP gates. |
| C2 | Shared helpers: `ensure_china_maplibre_carto`, `frame_china_map2d`, `apply_china_map2d_product_defaults`, `apply_china_scene3d_atmosphere` / `_orbit` / `_product_defaults`. |
| C3 | China 2D: clear `china_city.style.json` → default MapLibre carto; frame `kChinaLonLatExtent` at the given pixel size. |
| C4 | China 3D: seed procedural + ocean/cloud/sky/**fog** (match atmosphere.full); orbit distance `2.55`. Opt out: `SMT_SCENE3D_ATMO=0` / `SMT_SCENE3D_LAND_ONLY=1`. |
| C5 | Sample paths stay **exe-relative** (`out/Debug` → `../data/…`); not cwd. |

### Checklist

- [x] Shared helper + wire interactive Map fit / 3D tab.
- [x] Wire map2d china + atmosphere full showcase to the same helpers.
- [x] README note (观感对齐 vs GDI 强制).

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
| B6 | Scope C: reshape covers process entry (`app/`), controller (`browser/`), UI (`ui/`), plus `harness/{showcase,self_test}/`. Menus / AM Box / paint rules stay as §2–§4 and map2d-frame. |
| B7 | Git: work on `master`. No feature branch. No new dated twin for this split. |

### Target `shell/` tree

```
src/app/views/shell/
  app/          # browser_main, ViewsContentHost, cmdline/
  browser/      # Browser controller + commands/ + nav/ + plugin/
  ui/           # BrowserView + pages/ + panels/
  harness/
    showcase/   # atmosphere / map2d / ui / input scene packages
    self_test/  # probe + stage TUs + run_self_test
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

**Status:** landed (directory big-bang + `MapSession` ownership fold + 2026-10-04 browser root TU split).
**Diagram:** [`../diagrams/content-browser-layers.html`](../diagrams/content-browser-layers.html)
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
| C10 | Remaining `browser/` root TUs move by responsibility: `bootstrap/` (sample paths), `contents/` (OOP `MapContents` pipe), `catalog/` / `attrs/` / `plugin/` (public-header implementations), `session/` (`MapSession`). **No** forwarding headers. **`src/content/public/` stays.** Thin siblings **folded by C11** (§ below). |

### Target tree

```
src/app/views/
  main.cc
  shell/                    # chrome only (Browser owns MapSession)

src/content/browser/
  bootstrap/                # sample / china map path policy (public/map_bootstrap.h)
  contents/                 # MapContents OOP/GPU pipe (public/map_contents.h)
  catalog/                  # LayerDesc JSON (public/catalog_layers.h)
  attrs/                    # feature tokens (public/feature_attrs.h)
  plugin/                   # PluginHost impl (public/plugin_host.h)
  session/                  # MapSession owns document/camera/present/input + MapContents*
  document/                 # MapScene
  camera/                   # ViewFrame, OrbitFrame, ViewNavigation
  present/                  # facade + frame + session + host (no paint/)
    scene3d/session/        # engine SoT + leftover stereo (was policy/ + stereo/)
  input/                    # MapHwndGestures
  capability/               # IL Host
  debug/                    # DebugAgent

  # software/ colocated under present/map2d|scene3d (not under src/render)
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
6. [x] `docs/superpowers/src-layout.md` Hosted map row mentions browser capability dirs + `MapSession`.
7. [x] Root DLL / session TUs off `browser/` root into `{bootstrap,contents,catalog,attrs,plugin,session}/`; no shims; `public/` unchanged.

## §Content browser subdirectory tighten（2026-10-04）

**Status:** active（P0 文档锁定；产品 `.cc` 未搬）  
**Diagram:** [`../diagrams/content-browser-layers.html`](../diagrams/content-browser-layers.html)  
**As-built:** [`../src-layout.md`](../src-layout.md) Hosted map row · [`../../../src/content/browser/README.md`](../../../src/content/browser/README.md)  
**Precedent:** C10 in §Content sink · GIS `gis/model` flatten · Vista **§Vista subdirectory tighten**  
**Checklist:** 下列分阶即本 § 的可执行表（不新开 dated plan 只重述搬目录）。每步可编译；scheme C，无转发头。

C10 把 `browser/` 根上 TU 按 public 头 1:1 拆成 12 个同级目录。职责对了，但 **薄目录过多**（`bootstrap/` `catalog/` `attrs/` `plugin/` `input/` 各 1–2 个文件；`document/{store,ingest,query,style,edit}/` 是 MapScene 组合件，不是第二套产品 API）。本 § 收紧同级面，**不**重开 chrome vs content，**不**改 C1–C9 / C3 present 嵌套。

### 现状诊断

| 事实 | 证据 |
| --- | --- |
| C10 as-built | `browser/{bootstrap,contents,catalog,attrs,plugin,session,document,camera,present,input,capability,debug}`；根上无 TU |
| 薄 DLL 实现 | `bootstrap/` `catalog/` `attrs/` `plugin/` 各一对 `.cc`+test（`plugin/` 无 test）；`contents/` 仅 `map_contents.cc`。全部进 `:content`，对应 `public/` 头（头本身不搬家） |
| 薄 session 卫星 | `input/` 仅 `MapHwndGestures`；调用方是 `session/map_session.h` + `app/views/shell/ui/browser_view.cc` |
| `document/` 过深 | 门面 `map_scene.*` + 五个子目录各 1–3 对文件。`detail::MapLayer` 在 `store/map_layer.h`，易与 `gis/map/map_layer.h` / `gis::MapLayer` 撞名（类型仍分属 hosted vs GIS） |
| 保持 | `camera/` 已平铺；`present/{map2d,scene3d}/{frame,gpu,software}` + `scene3d/{session,atmosphere}` + `present/host`（C3）；`debug/{wire,policy,schema,cmd}` 已有体量；`capability/host.h` 头文件、GN `:capability` 隔离 |
| 禁止混淆 | `content::MapSession` ≠ `gis::MapEditSession`；hosted `detail::MapLayer` ≠ `gis::MapLayer`。不要把 document 折进 `gis/edit` |
| CBM | 索引仍可能列 C10 前根文件；以磁盘 + `BUILD.gn` 为准 |

### 锁定（C11）

| # | Decision |
| --- | --- |
| C11a | `content.dll` 在 `browser/` 下的 public 实现并入 **`contents/`**：`map_contents` + `map_bootstrap` + `catalog_layers` + `feature_attrs` + `plugin_host`（及对应 `*_test.cc`）。删除 `bootstrap/` `catalog/` `attrs/` `plugin/`。 |
| C11b | `input/map_hwnd_gestures.*` → **`session/`**。删除 `input/`。`:map_hwnd_gestures` 可保留为 source_set（sources 改路径）或并入 `:map_session`。 |
| C11c | `document/{store,ingest,query,style,edit}/*` **拍平到 `document/`**（文件名不变）。删除五个子目录。`map_layer.h` 仍是 `content::detail` POD，不改成 `gis::`。 |
| C11d | **不收紧：** `present/**`（C3）、`debug/{cmd,policy,schema,wire}`、`camera/`、`capability/`、`src/content/public/`。 |
| C11e | 无转发头。Include 同变更改完。命名空间仍 `content` / `content::detail`。不进 `content.dll` 的 present/GDI 规则（C6）不变。 |

### 目标树

```
src/content/browser/
  contents/          # content.dll：MapContents 管道 + public 头实现（bootstrap/catalog/attrs/plugin）
  session/           # MapSession + MapHwndGestures
  document/          # MapScene + store/ingest/query/style/edit 文件（无子目录）
  camera/            # ViewFrame, OrbitFrame, ViewNavigation
  present/           # 不变：host | map2d/{frame,gpu,software} | scene3d/{session,frame,atmosphere,gpu,software}
  capability/        # IL Host（header-only）
  debug/             # DebugAgent + cmd/policy/schema/wire
```

12 同级 → **7**。`public/` 九个头路径不变。

### 依赖（不变）

```
shell → :map_session → {map_scene, map_camera, map_present, scene3d_present, map_hwnd_gestures, view_host}
contents/* → :content     (DLL)
present ↛ shell
render ↛ content
product ↛ leftover
gis::Map ↛ content::MapScene
```

### 分阶（每步可编译）

- [x] **P0 文档（本变更）** living § + HTML + `src/content/README.md` + `browser/README.md` + `src-layout`。不搬产品 `.cc`。
- [ ] **P1 `contents/` 合并** scheme C

| 旧 | 新 |
| --- | --- |
| `browser/bootstrap/map_bootstrap.*` | `browser/contents/map_bootstrap.*` |
| `browser/catalog/catalog_layers.*` | `browser/contents/catalog_layers.*` |
| `browser/attrs/feature_attrs.*` | `browser/contents/feature_attrs.*` |
| `browser/plugin/plugin_host.cc` | `browser/contents/plugin_host.cc` |

`contents/map_contents.cc` 不动。`BUILD.gn` `:content` / 三个 `*_test` 改 sources。Grep `"content/browser/{bootstrap,catalog,attrs,plugin}/` 必须空。

- [ ] **P2 `input/` → `session/`** `map_hwnd_gestures.{h,cc}`；改 `session/map_session.h`、`app/views/shell/ui/browser_view.cc`。

- [ ] **P3 `document/` 拍平**

| 旧 | 新 |
| --- | --- |
| `document/store/{layer_store.*,map_layer.h}` | `document/layer_store.*` · `document/map_layer.h` |
| `document/ingest/{ogr_ingest,geojson_write,seed_paths}.*` | `document/` 同名 |
| `document/query/{extent_query,inspector}.*` | `document/` 同名 |
| `document/style/style_bind.*` | `document/style_bind.*` |
| `document/edit/feature_edit.*` | `document/feature_edit.*` |

门面 `map_scene.*` 不动。测试 `feature_edit_test` / `map_scene_test` 改 include。

- [ ] **P4 验证** `build.bat debug content` · `map_session` · `content_map_bootstrap_test` · `map_scene_test` · `feature_edit_test` · `scene3d_presenter_test` · `views`。

### 不做什么

- 不把 `MapSession` 折进 `MapContentsImpl`（C5）。
- 不把 hosted document 折进 `gis/{map,edit}`；不合并两个 `MapSession` 类型。
- 不把 GDI/GPU present 吸进 `content.dll`；不把 `software/` 挪到 `src/render`。
- 不拍平 `present/map2d|scene3d` 的 frame/gpu/software；不拍平 `debug/cmd`。
- 不改 `public/` 头路径；不加 Qt；不新开 dated spec。

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
**Updated:** 2026-09-30 — UI Views paint/compositor spans (`ui.views`); Trace tab + `UI` filter; UiDesigner docks Console+Trace by default  
**Plan:** [`../plans/2026-09-28-render-trace-profiler.md`](../archive/plans/2026-09-28-render-trace-profiler.md) (timing) + memory § in hybrid; console: [`2026-09-28-debug-console-design.md`](../archive/specs/2026-09-28-debug-console-design.md) + [`../plans/2026-09-28-debug-console.md`](../plans/2026-09-28-debug-console.md)

VS-style bottom **Diagnostic Tools** dock (replaces standalone Debug Console + Inspector `RenderTrace`):

| Item | Choice |
| --- | --- |
| UI | `ui::views::DiagnosticToolsPanel` bottom dock |
| Tabs | Output \| Console \| **Trace** (was CPU) \| Memory |
| UI paint profile | `BASE_TRACE_EVENT(..., "ui.views")` on Widget `on_paint` / layout / record_commit / present + ShellCompositor `raster` / `blt_present`; RenderTrace filter checkbox **UI** |
| UiDesigner | View → Toggle Console+Trace (default collapsed; toggle opens dock) |
| Product shell | Diagnostic Tools **open by default**; active tab **Console** (Trace adjacent) |
| Tabs | `Output` \| `Console` \| `Trace` \| `Memory` |
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
| Trigger | **Process start** (`base::trace::start_always_on_diagnostics` from `wWinMain`) |
| Startup log | Full `BASE_TRACE_EVENT(..., "startup")` tree + matching `LOGGING` → `LogSink` → Output |
| Perf Gantt | Always-on `base::trace::process_trace`; CPU tab auto-refresh (~500ms while tools visible) |
| Memory | 500ms sampler thread + `AllocationTracker::enable` at bootstrap; Memory tab auto-refresh |
| Escape | Record (clear+continue) / Stop / Armed / Track allocs still work |

Bootstrap API: `src/base/trace/diag/diagnostic_bootstrap.{h,cc}`. `base::trace::set_tracing_enabled(true)` clears only on **off→on** so always-on startup spans survive UI re-arm; explicit Record clears first.

## §Debug Console（2026-09-28）

**Status:** active  
**Updated:** 2026-10-02 — D1–D7 landing (host layers/overlay, rpc.methods/schema, diag.pack, console UX, policy gate, record/:script, Ask stub)  
**Owning spec:** [`2026-09-28-debug-console-design.md`](../archive/specs/2026-09-28-debug-console-design.md)  
**Plan:** [`../plans/2026-09-28-debug-console.md`](../plans/2026-09-28-debug-console.md)  
**Diagram:** [`../diagrams/debug-console-agent.html`](../diagrams/debug-console-agent.html)

Bottom-dock **Debug Console** capabilities live under Diagnostic Tools tabs. Shell responsibilities:

| Item | Choice |
| --- | --- |
| UI | Bottom dock `DiagnosticToolsPanel` (Output + Console panes) |
| Menu | View → Toggle Diagnostic Tools (starts `DebugAgent` if needed) |
| Layering | Panel → Agent / `LogSink` only; no direct `SdbdClient` from views |
| Trace | CPU/Memory tabs share `base::trace::process_trace` (not merged with LogSink) |
| Transport | Loopback-only NDJSON JSON-RPC (`127.0.0.1`) |

### D1–D7 landing (2026-10-02)

| ID | Item | Lock |
| --- | --- | --- |
| D1 | Host `layer_names` from `MapScene::layer_descs`; `ui.overlay_stats` = HUD FPS + GPU/content present + map2d present flags | done |
| D2 | `rpc.methods` / `:help json` + `tools/debug/agent_methods.json` | done |
| D3 | `diag.pack` / `:diag` [capture] — log + extent + layers + tree + overlay (+ optional capture) | done |
| D4 | Console history (Up/Down), Tab `:cmd` complete, level/text filter, taller dock (~360dip) | done |
| D5 | Policy gate: `:py` / bare eval / `sdbd.query` / `ui.click` / capture — `:confirm` or `SG_DEBUG_ALLOW=1` (debug builds auto); audit via LogSink | done |
| D6 | `record.*` console + Python helpers; `script.run` / `:script` thin Host wrap (harness IL) | done |
| D7 | Panel **Ask** → `:ask` local keyword→tools stub; **remote LLM backend not wired** | partial (UI + local stub) |

Architecture remains **thin UI + thick `content::DebugAgent`**. No Qt / second widget kit. LLM/tools use Agent RPC + policy gate only.

Full protocol, LogSink, Python worker, and sdbd bridge live in the owning spec.

## §GIS Python Console + analysis results（2026-09-28）

**Status:** active  
**Updated:** 2026-09-28 — dual-runtime A (embed + worker parallel); prior phase-1 analysis  
**Plan:** [`../plans/2026-09-28-gis-python-spatial-analysis.md`](../archive/plans/2026-09-28-gis-python-spatial-analysis.md)  
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
| Profile | `smartgis.debug` → Diagnostic Tools CPU (`base::trace::process_trace`) |
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
**As-built:** [`../ui-testing.md`](../ui-testing.md) + [`../../../testing/README.md`](../../../testing/README.md)

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

Product dialog and GIS panel assets live in **`src/ui/resources/<area>/`** (nested by responsibility, aligned with `ui/gis/{catalog,inspect,shell,style,analysis,debug}` plus `toolkit/` for generic views dialogs). GN `:markup_resources` copies each area to shared **`out/ui/<area>/`** (`$root_out_dir/../ui`, sibling of Debug/Release — same pattern as `out/data/`) plus flat `markup/testdata/` samples into `out/ui/`. Call sites use relative names: `load_markup("catalog/create_map.ui.xml")`, `load_markup("inspect/attribute_schema.ui.xml")`, `load_markup("inspect/measure_panel.ui.xml")`. Resolver searches `<exe>/../ui/<rel>`, `<exe>/ui/<rel>`, and `src/ui/resources/<rel>`.

**Panel markup contract:** C++ panel constructs via `load_markup` + id bind + `FillLayout` (same as product dialogs). Dynamic rows/trees stay on `set_*` APIs. Nested C++ children (TabStrip pages, History) mount into `panel` hosts (`tabs_host` / `history_host` / `chart_host` / `plot`). Landed: StatusBar, Measure, Selection, Legend, Symbology, LayerProperties, FeatureInfo, AttributeTable, Catalog, SpatialAnalysis, Processing, History, Atmosphere, ResultPlayback, DebugConsole, RenderTrace, DiagnosticTools (chrome + `tabs_host`), Memory page chrome (`debug/memory_page`), Ambox scroll shell (`shell/ambox_view`), ChartView title chrome (`shell/chart_view`). Intentionally C++: Ambox dynamic group buttons, ChartView series plot paint, Memory sparkline paint, LayerTree custom rows (hosted by Catalog markup).

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
| GIS chrome | `src/ui/gis/` (panels + catalog/inspect product modals) | same `ui_views.dll` (`//src/ui/gis:gis` → `:ui_views`) |
| Product host | `src/app/views/` | `SmartGisViews.exe` |

**Keep in `views/dialogs/`:** `Dialog`, `MessageBox`, `FilePicker`, `InputTextDialog`, `SelectOneDialog` (generic toolkit; no GIS types).

**Moved to `ui/gis/`:** former `views/gis/**` panels; product modals colocated by domain — `catalog/` (`AddBasemap`, `CreateDatasource`, `CreateLayer`, `CreateMap`) and `inspect/` (`AttributeSchema`).

**Namespace:** stay `ui::views` for moved types (directory + `"ui/gis/…"` includes first). **One** export for all of `//src/ui`: `UI_EXPORT` / `UI_EXPORTS` (`ui/ui_export.h`). Mass `ui::gis` rename is a later optional pass.

**Markup:** ControlFactory GIS tags remain **placeholders** registered in views (`register_gis_placeholder_markup_tags`). Real panel instances stay C++-constructed by the app (optional real-tag registration TU under `ui/gis` later if markup embeds live panels). Panel **chrome layout** migrates to `src/ui/resources/<area>/*.ui.xml` (Wave1 landed: StatusBar / Measure / Selection / Legend).

**Resources nest:** see §Declarative markup subdirectory (same file) + [`../plans/2026-09-28-gis-resources-markup.md`](../plans/2026-09-28-gis-resources-markup.md).

**GN:** `views` must not *list* GIS sources in its own `source_set`s; `:ui_views` `deps` `//src/ui/gis:gis_sources`. App / tests / plugin widgets `public_deps` `//src/ui/gis:gis` (forwards to `:ui_views`). Root `//:ui_views` group pulls `:gis`.

As-built: [`docs/superpowers/ui-views-skia.md`](../ui-views-skia.md), [`src/ui/gis/README.md`](../../../src/ui/gis/README.md), [`src/ui/views/README.md`](../../../src/ui/views/README.md), [`src/ui/resources/README.md`](../../../src/ui/resources/README.md).

---

## §UI interactive harness + overlay bench（2026-09-28）

**Status:** accepted  
**Plan:** [`../plans/2026-09-28-ui-interactive-overlay-bench.md`](../plans/2026-09-28-ui-interactive-overlay-bench.md)  
**As-built alignment:** [`../ui-testing.md`](../ui-testing.md) **P2 / L1** (process-in interactive sequences + overlay bench; not a replacement for L1′ `--self-test` or L2 pixel).

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
| 3 | **Wave2 (semantic map chrome):** `MapViewport` / **`AuxOverlay`** semantic hooks (extent string, ready marks, aux layer visibility). **No map pixels in L2** — same rule as [`ui-testing.md`](../ui-testing.md) L2. |
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
**As-built alignment:** [`../ui-testing.md`](../ui-testing.md) **L1c** (failure capture + optional record-all; not a gate for default `build.bat te`).

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
- [x] Document L1c + runbook in [`ui-testing.md`](../ui-testing.md).

---

## §Harness suite loop（2026-09-29）

**Status:** active (Wave 1+2 landed: marks + BMP suites; registry covers browse/console/input + atmosphere/map2d/ui modes).  
**As-built:** [`../ui-testing.md`](../ui-testing.md) L1′ loops; `testing/tools/loop_runner.py` + `suites/*.json`.

Unify outer Python rebuild/retry loops and in-process harness paths under a shared **suite id** contract. Not product `PluginHost`.

### Model

| Layer | Role | Location |
| --- | --- | --- |
| Suite contract | id / argv / env / marks / bmp / loop | `testing/tools/harness/<family>/<id>/suite.json` (JSON for stdlib; no PyYAML) |
| Outer runner | kill → build → run → score → report | `testing/tools/loop_runner.py` + `loop/` |
| Inner registry | static `id → run(Browser&)` | `shell/harness/scenario_registry.*` + `scenario_builtins.cc` |

**Probes:** `marks` and `bmp` (`score_id`: `ui_shell_dark` / `map2d_china` / `atmosphere_full`). `forensics` via suite env (`SMT_UI_FORENSICS=1` on `ui.shell`). Trace / live `debug_agent` still optional later.

### Decisions

| # | Decision |
| --- | --- |
| 1 | Suite ids align across JSON and C++ (`browse`, `input`, `console`, `ui.shell`, `map2d.*`, `atmosphere.*`). |
| 2 | Exe does **not** parse suite JSON; Python owns outer loop; C++ owns steps/marks/BMP write. |
| 3 | Suite dir is `suite.json` + optional `*.il` only. No colocated `*_loop.py`; entry is `loop_runner --suite <id>`. Processing args inline via `run_processing(..., args="...")` (`\"` escapes in Interact.g4). |
| 4 | Showcase cmdline modes dispatch through `ScenarioRegistry` (same ids as suites). |
| 5 | No product PluginHost for harness; live object probes use DebugAgent (separate from default mark/BMP gates). |

### Checklist

- [x] `loop/` + `loop_runner.py` + suites `browse` / `input` / `console`.
- [x] Thin wrappers removed (2026-09-30 layout A); use `--suite` only.
- [x] `ScenarioRegistry` + builtins; `browser_main` dispatches browse/console/input via registry.
- [x] Wave 2: BMP `score_id` suites (`ui.shell` / `map2d.china` / `atmosphere.full` / orthogrid / legacy.*); register atmosphere/map2d/ui showcase ids.
- [x] Unified loader: suites discovered from `harness/**/suite.json`; see `testing/tools/README.md`.
- [ ] Optional: trace / live `debug_agent` probe steps.
- [ ] Optional CI: suite JSON ids ⊆ `--dump-scenarios` (not required yet).

---

## §UI interact script（A inproc + C OS）（2026-09-30）

**Status:** active (Wave 2: Interact DSL + ANTLR).  
**Updated:** 2026-09-30  
**As-built:** `testing/tools/harness/_shared/scripts/grammar/Interact.g4` + suite-colocated `*.il`; C++/Python ANTLR visitors (gen under `out/{Debug|Release}/gen`, not checked in); `loop/interact/dsl.py` + `os_inject.py`; suites `ui.interact` / `ui.interact.os` / `ui.interact.smoke` / `ui.interact.combo`.

Authoring is **Interact DSL** (`.il`). Approach C: ANTLR **visitor → AST → direct execution** (no JSON Step IR). Same grammar for inproc (C++) and OS (Python). GN `interact_antlr_gen` (JDK + `antlr-4.13.2-complete.jar`) writes lexer/parser into `$root_gen_dir`; RD mirrors removed.

| Driver | Who runs steps | Injection |
| --- | --- | --- |
| **inproc** (`ui.interact`) | C++ DSL parse+exec | Semantic tab APIs + `dispatch_input` / `PostMessage` |
| **os** (`ui.interact.os`) | Python DSL → HWND inject | `postmessage` / `sendinput` |

| Script | Suite | Role |
| --- | --- | --- |
| `ui.interact.il` | `ui.interact` / `ui.interact.os` | Full chrome + map combo (BMP) |
| `ui.interact.smoke.il` | `ui.interact.smoke` | Minimal parse/exec (marks) |
| `ui.interact.combo.il` | `ui.interact.combo` | Mid-weight path/chord/bursts demo (marks) |

### Decisions

| # | Decision |
| --- | --- |
| 1 | Suite JSON may name `script` / `driver` / `os_inject_default`; exe does **not** parse suite JSON. Script path via `SMT_UI_INTERACT_SCRIPT`. |
| 2 | Steps/blocks may use `@inproc` / `@os` / `@drivers(...)`; unsupported on current driver → skip. |
| 3 | OS inject modes: `postmessage` (default) and `@inject=sendinput`. |
| 4 | No FlaUI / UIA Provider requirement. |
| 5 | Hardcoded `apply_scenario_interaction` remains fallback if script missing. |
| 6 | Continuous combo: `seq` / `repeat` / `chord` / `path` / `pan_burst` / `wheel_burst`. |
| 7 | DSL is authoring source of truth (`Interact.g4` / `.il`); JSON step scripts removed from tree. |
| 8 | ANTLR C++/Python gen lives only under `out/*/gen` (not in git); build requires JDK. |

### Checklist

- [x] Shared `scripts/ui.interact.il` + inproc C++ runner.
- [x] OS runner + suite `ui.interact.os`.
- [x] Continuous combo ops.
- [x] Interact.g4 + dual runtimes + smoke example suite/test.
- [x] Combo demo script/suite (`ui.interact.combo`) + migrate remaining scripts off JSON.
- [x] Swap RD → ANTLR gen visitors (`interact_antlr_gen` → `out/*/gen`); suffix `.il`.
- [ ] Optional: mark probe for `interact-script-ok` / `dsl-done` on inproc suite.

---

## §Harness capability runtime（2026-09-30）

**Status:** active (Wave 2: atomic Host verbs + full `.il` suite bodies — edit scripts without rebuild).  
**Plan:** [`../plans/2026-09-30-harness-capability-runtime.md`](../archive/plans/2026-09-30-harness-capability-runtime.md)  
**As-built:** `content/browser/capability/` Host; `app/views/shell/runtime/{capability,interact,analysis}/`; Interact verbs via Host (`map2d_run` / `atmosphere_run` / `console_run` / browse / digitize); Wave 2 adds `resolve_data` / `capture_path` / `sidecar_path` / `doc_clear` / `fit_extent` / `export_bmp` / `apply_style_file` / `suppress_dialogs` / `require_plugins` with `$var` bind via `as=` (no grammar change). DebugAgent `script.run` thin wrap. Suite scripts colocated under `testing/tools/harness/<family>/<suite_id>/*.il`.

### Intent

Deepen the Interact DSL from **UI-only** into a **shared scenario language** for all harness suites (browse / input / map2d / atmosphere / ui / console), with Capability APIs sunk where DebugAgent can thin-wrap them — without expanding GIS Python/console product surface in Wave 1.

### Layering (Approach 1)

| Layer | Path | Owns |
| --- | --- | --- |
| Capability Host | `src/content/browser/capability/` | Callback bag + core verb helpers (`pump` / `mark` / `wait_ready` / `load_sample` / `detach_maps` / map input). No dep on `app::Browser`. |
| Shell runtime | `src/app/views/shell/runtime/` | Three peer trees below; no flat sources at runtime root. |
| → capability | `runtime/capability/` | `fill_host` + `run_interact_script` / `try_run_suite_script`. |
| → interact | `runtime/interact/` | ANTLR gen + `try_apply_interact` / `is_interact_path` (`:interact`). |
| → analysis | `runtime/analysis/` | `AnalysisPlayback` facade + `TrafficStore` / `FloodStore` / `OrthogridStore`. |
| Harness | `src/app/views/shell/harness/` | `ScenarioRegistry` + suite adapters; showcase/self_test become thin or deleted as scripts land. |
| Authoring | `testing/tools/harness/<family>/<suite_id>/*.il` | Source of truth for suite bodies (full migration). |
| DebugAgent | `content/browser/debug` | `script.run` / `:script` → Host `script_run` callback only. |

### Decisions

| # | Decision |
| --- | --- |
| 1 | Grammar stays `Interact.g4` / `.il`; verbs expand beyond UI. |
| 2 | `content` must not include `app/views`; Host is `std::function` bag (same pattern as `DebugAgentHost`). |
| 3 | Full suite migration: C++ keeps registry + verb impl; script bodies replace showcase/self_test step code. |
| 4 | Wave 1: Host + runtime + DebugAgent thin wrap + DSL on Host; migrate scripts incrementally with C++ fallback until marks/BMP green. |
| 4b | Wave 2 (2026-09-30): atomic verbs + `$var`/`as=` in DSL; `plugin.*` suites are full `.il` bodies; C++ showcase body removed. |
| 5 | No Qt; no FlaUI; OS driver path unchanged (`@os` / `loop/interact/os_inject.py`). |
| 6 | Do not expand product Python DSL in Wave 1. |
| 7 | Runtime layout (2026-09-30): `capability/` · `interact/` · `analysis/`; `AnalysisSession` → `AnalysisPlayback` (no shim); `fill_host` / `run_interact_script` / `try_apply_interact`. |

### Checklist

- [x] `content::CapabilityHost` + core helpers; GN `//src/content:capability`.
- [x] `shell/runtime` fill Host + `run_script`; DSL uses Host for shared verbs.
- [x] DebugAgent `script.run` + `:script <path>` wired from `BrowserView::bind_debug_agent_host`.
- [x] Move Interact under `shell/runtime/interact/` (`apply.*`; was `dsl/`).
- [x] Split `runtime/` into `capability/` + `interact/` + `analysis/` (stores + `AnalysisPlayback`).
- [x] Migrate suite scripts: `ui.*` → `input` → `browse` → `map2d.*` → `atmosphere.*` → `console`.
- [x] Update `docs/superpowers/ui-testing.md` as-built once Wave 1 compiles green.
- [ ] Wave 2 atomic verbs + migrate `plugin.*` `.il` full bodies; delete C++ showcase bodies when marks/BMP match.
- [ ] Migrate `map2d.*` / `atmosphere.*` / remaining coarse `*_run` wrappers the same way.

---

## §Text2UI（2026-09-30）

**Status:** active (landing).  
**Hosts:** `src/ui/views/text2ui/` (API + template + validate); `src/app/ui_designer` (Generate UI + Cursor Agent `LlmBackend`).

### Intent

Natural-language → Views declarative markup (`.ui.xml` fragment) inside UiDesigner: palette/property edits stay; Text2UI seeds or replaces layout from a prompt.

### Decisions

| # | Decision |
| --- | --- |
| 1 | Engine **C+A**: local **TemplateEngine** by default; prompt prefix `@llm` → Cursor Agent CLI. |
| 2 | Apply modes: **Insert under selection** (default) and **Replace document**. |
| 3 | Entry: menu **Generate…** + **Ctrl+Shift+G**; prompt via `InputTextDialog`, mode via `SelectOneDialog`. |
| 4 | Core lives under **`src/ui/views/text2ui/`** (namespace `ui::views`); **`ui_views` does not link `net`**. LLM is `LlmBackend` injected by the host. |
| 5 | Default LLM backend = **Cursor Agent** (`agent -p`). Missing install → official Windows installer; missing key → paste User API Key → process env + `setx CURSOR_API_KEY`. |
| 6 | Never Apply without **`validate_markup_fragment`** (`load_markup_bytes` wrap). Reject illegal / unparseable XML. |
| 7 | No Qt; no OpenAI-HTTP default (optional later behind same `LlmBackend`). CI does not call real Cursor API. |

### Checklist

- [x] `ui::views::generate_text2ui` + template matchers + extract/validate.
- [x] UiDesigner Generate flow + Cursor Agent backend (detect/install/key/spawn).
- [x] Unit tests for template / `@llm` strip / extract / validate (mock LLM).
- [x] As-built note in `src/ui/views/README.md` Markup notes when green.

---

## §Startup profile（2026-10-02）

**Status:** active  
**Updated:** 2026-10-04 — P3 Diagnostic Tools Console/Memory lazy markup (LoadMarkup cut).  
**As-built:** `src/base/trace/diag/startup_profile.h` + `BASE_TRACE_EVENT(..., "startup")` on the SmartGisViews launch path.

### Goal

Locate **SmartGisViews.exe** cold-start wall time from `wWinMain` through first interactive show (shell visible; map present is async by default), without a parallel timer stack.

### Facility

| Piece | Role |
| --- | --- |
| `BASE_TRACE_EVENT(name, "startup")` | Existing process_trace RAII spans (always-on diagnostics already enable recording) |
| `maybe_dump_startup_profile()` | Once after `Browser::show`: final table to `startup_profile.txt` (or `SMT_STARTUP_PROFILE_DUMP`) |
| `dump_startup_profile_partial(tag)` | Mid snapshots → `startup_profile.partial-<tag>.txt` (does **not** overwrite final / claim the once-slot) |
| `wall_ms` | Prefer `wWinMain`/`BrowserMain` dur; else **first→last** span coverage (file dump matches stderr) |
| `SMT_STARTUP_PROFILE=1` | Force-enable tracing + dump (also in Release) |
| `SMT_STARTUP_PROFILE_DUMP=<path>` | Write text table + sibling chrome JSON |
| Debug builds | Dump table to stderr/LOGGING after first show by default |

Phases covered (non-exhaustive): `wWinMain`, `ParseLaunchOptions`, `ContentMain` / `BrowserMain`, `Browser.ctor` / `init` / `show`, `Session.init_hosts` (`MapContents.Create`; optional `StartRenderProcess` / `HelloWait`), `PluginShell.*`, `InitShell` subphases (`Widget.init`, `BuildContents`, `SeedDocument` / `try_open_china` / `SeedDocument.ChinaBootstrap`, `BindPresenters`, `AttachViewports`, `MapEdit.FlyCubeAttach` / `FlyCube.Init`, `WireShell`), `ShowShell` / `WaitFirstMapPresent`, `HillshadeBake`, `LoadMarkup` when hit.

### Startup optimize（P0 / P1，2026-10-02）

| ID | Change |
| --- | --- |
| **P0-3** | Final dump after first show; mid dumps are `*.partial-*`; `wall_ms` first→last fallback; named spans above |
| **P0-1** | `MapSession::init_hosts` only `Create`s MapContents; OOP via `ensure_oop_render_process()` / first ContentMapView. Opt-in at init: `--enable-oop-render` or `SMT_ENABLE_OOP_RENDER=1`. Hard off: `SMT_DISABLE_OOP_RENDER=1` |
| **P0-2** | Inspector first-show = FeatureInfo + AttributeTable; Measure/Report/Atmosphere/… on first tab select. `load_markup` path→XML process cache |
| **P1-1** | Data/3D FlyCube attach already deferred to `switch_map_tab` (as-built) |
| **P1-2** | Product path may `set_defer_china_seed(true)` (idle open after show). Harness/self-test stay sync; `SMT_SYNC_CHINA_SEED=1` / `SMT_DEFER_CHINA_SEED=1` override |
| **P1-3** | Report WebView2 created in `wire_report_panel` only when Report tab is materialized |
| **P2-1** | `WaitFirstMapPresent` opt-in only (`SMT_SYNC_FIRST_MAP_PRESENT=1`). Product show returns after shell paint + map invalidate — does not block on full carto/GPU token |
| **P2-2** | `HillshadeBake` skipped until `MapScene::has_china_extent()` (demo/defer seed no longer pays ~0.4s GDAL shade). Force: `SMT_MAP2D_FORCE_HILLSHADE=1`; hard off: `SMT_MAP2D_NO_HILLSHADE=1` |
| **P2-3** | `FlyCube.Init` async by default (UI wait 0). Opt-in sync: `SMT_SYNC_FLYCUBE_INIT=1` (MapEdit/Data 800ms, Scene3d 2500ms) |
| **P3-1** | `DiagnosticToolsPanel`: Output + Trace eager (LogSink / default tab); Console + Memory `load_markup` on first tab select (`replace_page`) |

Product cold-start measure (no `--ui-showcase=shell`):

| Phase | Pre-P2 (≈) | Post-P2 target |
| --- | --- | --- |
| wall (`first→last`) | ~6–7s | ~1–2s (init+show; map/DEM fill-in after) |
| `WaitFirstMapPresent` | ~2.3–2.7s | absent unless sync env |
| `FlyCube.Init` | ~0.4–0.8s UI block | ~0 (async Display thread) |
| `HillshadeBake` | ~0.4s in wait pump | absent until China extent |
| `AttachViewports` | ~0.5–1.0s | tens of ms (enqueue Init) |

### How to read

```bat
set SMT_DISABLE_OOP_RENDER=1
set SMT_STARTUP_PROFILE=1
set SMT_STARTUP_PROFILE_DUMP=out\Debug\log\startup_profile.txt
REM product path — do NOT pass --ui-showcase=shell
out\Debug\SmartGisViews.exe
```

stderr lines: `[startup-profile] …`. Full chrome buffer still via `SMT_TRACE_DUMP` if needed.

### Non-goals

- Do not replace frame-level map2d/scene3d phase clocks (equal-profile plans).
- Do not LoadLibrary-scan plugins at startup solely for profiling (builtins stay deferred).

---

## §Shell perf upgrade waves（2026-09-30）

**Status:** active  
**Plan:** [`../plans/2026-09-30-ui-shell-perf-upgrade.md`](../archive/plans/2026-09-30-ui-shell-perf-upgrade.md)  
**Predecessor:** [`../plans/2026-09-28-ui-compositor-thread.md`](../plans/2026-09-28-ui-compositor-thread.md) (P0–P5 roles landed; Deferred absorbed as U3–U5)  
**Diagram:** [`../diagrams/ui-views-shell-architecture.html`](../diagrams/ui-views-shell-architecture.html)（浅色 SVG：shell/toolkit/gfx/MapViewport/GPU 泳道 + Commit→compositor→raster→GPU 流水线动画）

Close the highest-ROI gap vs Chromium-class shell feel **without** vendoring `cc`/viz. Scenario map (as-built):

| Scenario | Primary bottleneck today | Wave |
| --- | --- | --- |
| Shell hover | UI `record_commit` + single-worker CPU DisplayList raster | U0 → **U1** |
| Table scroll | Dense `kText` ops on visible cells; no row-strip cache | U0 → **U2** |
| Map + shell overlay | Shell BGRA crop/copy + dual HUD/present path | U0 → **U3** (+ U4/U5 only if needed) |

### Locked choices

| Axis | Choice |
| --- | --- |
| Strategy | **C** — scenario-ordered waves on `ShellCompositor` (reject Chromium vendor; reject property-tree big-bang) |
| Paint backend | Stay CPU DisplayList / GDI (optional CPU Skia); **no** shell Ganesh in these waves |
| Layering | `ui/views` must not hard-dep `//src/gpu`; overlay glue stays `src/app/views` |
| Ship order | U0 measure → U1+U3 → U2 → optional U4/U5 |

### Non-goals

- Do not vendor Blink / `cc` property trees / Mojo viz / Aura.
- Do not paint map pixels through Views DisplayList.
- Do not invent absolute FPS SLA vs Chrome.

### Checklist (summary; details in plan)

- [x] U0 — `PaintCounters` + `views_bench` scenarios (hover / table / overlay)
- [x] U1 — Dirty/record tighten (per-view cache / bounded Commit)
- [x] U2 — Table row-strip (or glyph) cache
- [x] U3 — Overlay coalesce + HUD-as-quad + gen skip (absorbs compositor Deferred)
- [x] U4 — Multi-worker / tiled raster (optional)
- [x] U5 — BeginFrame-driven shell Commit (optional)

---

## §Map browse forensic harness（2026-09-30）

**Status:** active  
**Plan:** [`../plans/2026-09-30-map-browse-forensic-harness.md`](../plans/2026-09-30-map-browse-forensic-harness.md)  
**Extends:** §Harness suite loop, §Harness capability runtime, §UI interact script, §UI visual forensics  
**Also covers:** leftover MFC `SmartGis.exe` 2D Edit browse + leftover GL/D3D scene3d showcase (freeze path; harness-only)

### Goal

Reproduce and **analyze** map-browse failures on **both** product shells (Views `SmartGisViews.exe` + leftover `SmartGis.exe`), covering **2D map** and **3D scene**, with:

1. **Scripts** — deterministic Interact DSL / OS inject sequences (pan, wheel, browse stress, 3D orbit).
2. **Recording** — window capture (ffmpeg gdigrab preferred; BMP frame-burst fallback) under `out/<config>/captures/`.
3. **Symptom classes** (all in scope): hang / not responding, garbled or black frame, tracking lag (input vs pixels), crash / AV.

### Locked decisions

| # | Decision |
| --- | --- |
| 1 | Approach **A+B**: suite scripts (Interact DSL / existing showcases) + optional OS inject + sidecar **record**. |
| 2 | Suite matrix: extend `browse`; add `browse.3d`; add `legacy.browse.2d` / `legacy.browse.3d` (or reuse `legacy.scene3d.china` + linger + inject). |
| 3 | Record gate: `SMT_HARNESS_RECORD=1` (or suite `env`). Prefer `ffmpeg`; else N fps BMP sequence. Not a CI hard fail if ffmpeg missing. |
| 4 | Report JSON lists `steps[]` with `t_ms` + marks + `record_path` so video timeline aligns to script. |
| 5 | Hang: timeout + dump (existing loop timeout / optional cdb). Crash: `windbg-crash-diagnose` / `run_and_catch`. Visual: BMP score gates + human video review. Lag: optional step timestamps vs paint/mark latency in report. |
| 6 | No Qt; no new widget kit; Legacy stays freeze except harness/path fixes. |
| 7 | Docs: this § + plan; as-built notes in `docs/superpowers/ui-testing.md` + `src/legacy/app/README.md` when landed. |

### Suite matrix

| Suite id | Exe | Script / argv | Probes |
| --- | --- | --- | --- |
| `browse` (extend) | SmartGisViews | `browse.il` + optional record | marks; optional frames/mp4 |
| `browse.3d` (new) | SmartGisViews | 3D tab + orbit/drag/wheel `.il` | marks; record |
| `legacy.browse.2d` (new) | SmartGis | OS inject on Edit map HWND after interactive bring-up (or short linger showcase) | marks/timeout; record |
| `legacy.browse.3d` (new / alias) | SmartGis | `--scene3d-showcase` linger + inject or headless BMP + optional record of HWND | BMP gates + record |

### Recording pipeline

- Helper: `testing/tools/loop/record/hwnd.py` (invoked from `runner` / suite env).
- Output: `out/<config>/captures/record/<suite_id>_<stamp>.mp4` or `…/record/*_frames/`.
- Title / class match: `SmartGIS Views` / legacy main / showcase HWND titles already used by scene3d.

### Non-goals

- Absolute FPS SLA vs Chrome.
- Recording as required CI green on machines without ffmpeg.
- New Legacy product features beyond harness.

### Checklist

- [x] Record helper + suite `env` wiring
- [x] Extend `browse` + add `browse.3d` (+ showcase dispatch if needed)
- [x] Legacy 2D / 3D browse suites + inject
- [x] Timeline report fields + ui-testing as-built note
- [x] One local recorded run per shell (2D + 3D) for forensic sample

---

## §IL interaction recorder（hybrid OS + agent）（2026-10-01）

**Status:** active  
**Updated:** 2026-10-01  
**As-built:** `testing/tools/loop/record/{il_recorder,os_hook,il_compact,agent_events}.py`; `loop_runner.py --record-il`; DebugAgent `record.enable` / `record.poll` / `record.clear`; `BrowserView::switch_map_tab` → `push_record_event("select_map_tab")`.

### Intent

Open (or attach) the product app, record a human repro session, and emit a replayable Interact `.il` (OS verbs + semantic upgrades) for bug reproduction — complementary to HWND video (`SMT_HARNESS_RECORD`).

### Decisions

| # | Decision |
| --- | --- |
| 1 | Approach: Python-first OS LL hooks + optional DebugAgent semantic poll; always write `events.jsonl`, then compact to `.il`. |
| 2 | Entry: `il_recorder.py` core; `loop_runner --record-il` thin wrap. |
| 3 | Stop: Ctrl+Shift+F9 hotkey + console Enter / Ctrl+C. |
| 4 | No `Interact.g4` change; emit existing verbs only (`path` / `pan_burst` / `wheel_burst` / `drag` / `click` / `key` / `select_map_tab` @inproc). |
| 5 | Launch sets `SG_DEBUG=1`; without Agent, still emit pure `@os` `.il`. |
| 6 | Output: `out/<config>/captures/record/il_<stamp>/`. |

### Checklist

- [x] OS hook + jsonl + compact → `.il`
- [x] `loop_runner --record-il` + attach/launch
- [x] Hotkey + console stop
- [x] DebugAgent `record.*` + tab semantic push
- [x] Unit tests for compact
- [ ] Optional: more Host verbs (`run_command`, catalog tabs) on record path
- [ ] Optional: round-trip e2e (record → `loop_runner --suite` with generated `.il`)

---

## §VS Code UI Markup preview（2026-09-30）

**Goal:** Edit product/plugin `.ui.xml` + `.ui.css` in Cursor/VS Code with (1) automatic approximate Webview preview and (2) one-command launch of `UiDesigner.exe` for true Views render (existing path arg + hot-reload).

**Approach:** Independent in-repo extension `testing/tools/harness/_shared/scripts/vscode/vscode-ui-markup/` (`mogu.ui-markup-0.1.0`), install via `install_vscode_ui_markup.bat` (same pattern as `vscode-interact`). No C++ change required for v0.1.

| Surface | Behavior |
| --- | --- |
| Auto preview | On open `*.ui.xml` when `mogu.uiMarkup.autoPreview` (default true) |
| Webview | Subset map: `ui`/`vbox`/`hbox`/`label`/`button`/`textfield` + `<style src>`; linked CSS beside XML |
| Commands | `mogu.uiMarkup.preview`, `mogu.uiMarkup.openInDesigner` |
| Designer path | `mogu.uiDesigner.path` → `out/Debug/UiDesigner.exe` → `out/Release/` → Open dialog (persist workspace setting) |

**Non-goals:** Pixel-perfect Skia parity in Webview; Custom Editor replacing the XML text buffer; shipping to Marketplace.

### Checklist

- [x] Extension package + Webview preview + Open in UiDesigner
- [x] Install bat + `.vscode/README.md`
- [ ] Optional later: richer tag map / theme tokens / single-instance designer IPC

---

## §Shell chrome DPI / body font（2026-09-30）

**Symptom:** Interactive SmartGisViews chrome text looks too small; moving/scrolling the mouse makes glyphs jump larger. On **250% (240 DPI)** hosts the catalog still looked unreadable after the 16→18 DIP bump.

**Cause:**

1. Controls measure at scale `1.f` in their ctor (no `Widget` yet). `set_contents_view` used to propagate DPI only once; children added later stayed at 1× until a dirty paint rebuilt ink. Separately, `GetDpiForWindow` right after `CreateWindow` can still report 96 until the HWND is shown.
2. **TreeView** row/indent metrics were fixed device pixels while the shell face scaled → clipped labels.
3. **U4 parallel raster (primary “always tiny” bug):** `ShellCompositor::raster_dirty_into` painted large dirty regions onto **fresh temp DCs without selecting the shell HFONT**. `TextOutW` fell back to SYSTEM (~12px). Small hover dirties reused the back DC (font selected) → glyphs suddenly looked larger. Full-frame chrome stayed tiny forever.

**Fix (toolkit):**

| Seam | Behavior |
| --- | --- |
| `View::set_widget` | If `device_scale_factor() != 1`, call `on_device_scale_factor_changed(1, scale)` on that node |
| `Widget::set_contents_view` | Rely on `set_widget` only (no second `propagate` — avoids double-scaling `preferred_size` ratios) |
| `Widget::show` | Re-`sync_dpi_from_hwnd`; if scale changed, propagate + layout + paint |
| `dpi_for_hwnd` | Prefer monitor effective DPI when `GetDpiForWindow` still reports 96 |
| `kShellBodyFontDip` | **13** (was 16 → 18 → 20; rolled back 2026-10-04 — 20 overcrowded Diagnostic Tools / inspector once row heights scaled) |
| `TreeView` | Row / indent / twisty / checkbox are DIPs × `device_scale_factor` |
| `BrowserView` | Do not hardcode menu/status heights in raw px |
| `ShellCompositor::raster_dirty_into` | Select shell face on every paint target (per-strip HFONT on U4 temps) |

### Checklist

- [x] Attach-time + show-time DPI notify; body font DIP bump
- [x] TreeView + shell chrome preferred sizes scale with DPI (no fixed-px row clip)
- [x] U4 parallel strip DCs select shell HFONT (no SYSTEM-font full-frame)
- [ ] Ordinary open on 125%/150%/250% host: menu/catalog/status readable without jump on wheel/hover
- [x] 2026-10-04: body font 20→13 DIP + Diagnostic Tools flex `tabs_host` + Trace default tab (visual_review #1–#4)
---

## §Report dock（WebView2 report browser, 2026-09-30）

**Status:** active  
**Updated:** 2026-09-30  
**Capability spec:** [`2026-09-13-plugin-host-design.md`](2026-09-13-plugin-host-design.md) §report browser capability  
**Plan:** [`../plans/2026-09-30-plugin-report-browser.md`](../plans/2026-09-30-plugin-report-browser.md)

### Goal

Host an inspector **Report** tab that embeds `plugin::ReportBrowser` (v1 WebView2) for plugin-generated local HTML reports. This is **not** product chrome and does **not** reopen archived CEF HWND shell.

### Locked

| # | Choice |
| --- | --- |
| 1 | `ui::views::ReportPanel` in inspector TabStrip (`Report`), peer to Playback / Analysis. |
| 2 | Browser installs `PluginHost::set_report_bridge` → panel `open` / `post` / `close`. |
| 3 | Panel owns child HWND for WebView2; layout syncs bounds with the Views node. |
| 4 | Soft-fail when Runtime / loader missing; panel shows status, shell stays up. |
| 5 | Archived CEF product shell remains **rejected**; optional later `CefReportBrowser` only as ReportBrowser backend. |

### Checklist

- [x] ReportPanel + WebView2ReportBrowser + Host bridge
- [x] Sample HTML pack + FakeReportBrowser unit test
- [x] Python Host bindings `open_report` / `post_to_report` / `close_report`

---

## §Visual review closed-loop（2026-10-01）

**Status:** active  
**Updated:** 2026-10-01 (Wave2: browse/ui/legacy checklist seed + real `--review-prep` on `legacy.browse.2d` + `map2d.china`)  
**Plan:** [`../plans/2026-10-01-harness-visual-review.md`](../plans/2026-10-01-harness-visual-review.md)  
**Extends:** §Harness suite loop, §UI visual forensics (A+C), §Map browse forensic harness  
**As-built:** `testing/tools/loop/review/`; `loop_runner --review-prep`; `.cursor/skills/harness-visual-review/SKILL.md`; `docs/superpowers/ui-testing.md` §Visual review.

### Goal

Precipitate the multi-conversation practice that already works for product showcases:

1. **Per-feature capture** — run showcase / suite → BMP under `out/<config>/captures/`.
2. **Agent identifies all visible bugs** — `Read` an inspect PNG; enumerate numbered bugs (severity + product vs gate gap).
3. **Human confirms** — no code fix until the user confirms or selects the list.
4. **Closed-loop fix** — build → re-run → re-read + `score_bmp` → tighten `score_id` when a visual miss was the root cause; split crash verify from visual fix when needed.

This is **agent + human** quality loop on top of existing marks/BMP gates — **not** automatic CV bug discovery and **not** default `build.bat te`.

### Locked decisions

| # | Decision |
| --- | --- |
| 1 | Approach **contract + artifacts + Cursor skill** (no heavy auto-bug CV). |
| 2 | Optional `suite.json` block `visual_review` (`enabled`, `checklist[]`, `expect_notes`). Suites with `bmp` are reviewable by default even without the block. |
| 3 | After BMP score (or `--review-prep`): write `*.inspect.png` (BMP→PNG for Agent `Read`) + `*_visual_review.json` stub. |
| 4 | Review JSON `status`: `pending` → `confirmed` → `fixing` → `verified` (agent/human update; runner only creates `pending` + score snapshot). |
| 5 | CLI: `loop_runner.py --suite <id> --review-prep` runs one round, emits inspect + review stub, does **not** fix. Existing `--bmp` score path stays. |
| 6 | Implementation under `testing/tools/loop/review/` (`inspect_png.py`, `emit_review.py`); wire from `runner` after bmp probe. |
| 7 | Skill: `.cursor/skills/harness-visual-review/SKILL.md` — hard gate “no fix before human confirm”; crash vs visual split; strengthen `score_id` after confirmed visual misses. |
| 8 | **Not** in default `te` / CI hard fail. Optional `probes` entry `visual_review` only writes artifacts. |
| 9 | Docs: this § + plan; as-built in `docs/superpowers/ui-testing.md`; link from living Active table. |
| 10 | Wave1 suites: `plugin.stormsurge`, `atmosphere.full`, `map2d.china`, primary `plugin.*` showcases. |
| 11 | Wave2 suites (explicit checklist): `legacy.browse.2d` / `legacy.browse.3d`, `ui.shell` / `ui.catalog` / `ui.data` / `ui.scene` / `ui.interact` / `ui.interact.os`, `atmosphere.legacy`, `legacy.map2d.china`, `legacy.scene3d.china` (+ `.d3d`), `map2d.orthogrid`. `browse` / `browse.3d` stay marks-only (no bmp → no visual_review). |
| 12 | Wave2 first real review runs: `legacy.browse.2d` + `map2d.china` (`--review-prep --force-run`; prefer `--no-build`). Confirmed Vision misses tighten `score_id` / `zoom_gate` in the same change set when practical. |

### State machine

```
run / --review-prep
  → captures/*.bmp + *.inspect.png + *_visual_review.json (status=pending, score snapshot)
  → Agent Read(inspect.png) → numbered bug table
  → Human confirm / select
  → Agent fix → build.bat debug <target> → re-run suite
  → re-Read + score_bmp → status=verified; tighten score_id if gate was weak
```

### Review JSON shape (v1)

```json
{
  "suite_id": "plugin.stormsurge",
  "status": "pending",
  "bmp": "plugin-showcase-stormsurge.bmp",
  "inspect_png": "plugin-showcase-stormsurge.inspect.png",
  "score_id": "plugin_stormsurge",
  "score": {},
  "checklist": [],
  "expect_notes": "",
  "bugs": []
}
```

`bugs[]` entries are filled by the Agent (or human) after confirm — not by the runner.

### Non-goals

- Auto-listing “all bugs” into CI without human confirm.
- New pixel golden baselines for map/GPU frames.
- Replacing L1c `ui_forensics` / Mode C analyze.
- New dated design twin files.

### Checklist

- [x] `loop/review/` inspect PNG + emit review stub; wire runner + `--review-prep`
- [x] `suite.py` parse optional `visual_review`; seed enabled suites
- [x] Cursor skill `harness-visual-review`
- [x] Living as-built note in `docs/superpowers/ui-testing.md`
- [x] One local `--review-prep` smoke on `plugin.stormsurge` or `map2d.china`
- [x] Wave2: seed `visual_review` on browse forensic / ui / legacy / orthogrid bmp suites
- [x] Wave2: `--review-prep --force-run` on `legacy.browse.2d` + `map2d.china` → Agent bug tables → human confirm → fix / tighten gates

---

## §Shell chrome layout（2026-10-02）

**Status:** active  
**Updated:** 2026-10-02  
**Owns:** product `BrowserView::build_contents` chrome geometry (not a new dated twin).

| Region | Choice |
| --- | --- |
| Map / Catalog tab headers | `TabStrip::HeaderPlacement::kBottom` (Map\|Data\|3D + Layers\|Sources\|Maps) |
| Select / Edit / Tools | Horizontal `AmboxView` tool bar above Catalog\|Map column (`ambox_`) |
| Right dock | Multi-tab: **AMBox** (vertical `side_ambox_`) \| FeatureInfo \| AttributeTable \| Measure…Atmosphere |
| Bottom dock | Diagnostic Tools only (no FeatureInfo strip); **open by default**, Console active |
| Former bottom inspector | Moved into the right multi-tab (`inspector_tabs_`) |

Checklist:

1. [x] Map + Catalog tab headers bottom.
2. [x] Horizontal Ambox tool bar above map column.
3. [x] Right multi-tab (AMBox + FeatureInfo + GIS panels).
4. [x] Diagnostic Tools open + Console tab default.

---

## §shell/ui composers（2026-10-02）

**Status:** active  
**Updated:** 2026-10-02  
**Owns:** deep split of `src/app/views/shell/ui` after S1–S5 file-only multi-TU landed.

### Before → after

| Before (S4 as-built) | After (this §) |
| --- | --- |
| `browser_view.*` + `pages/map_pages.cc` + `panels/*.cc` as **multi-TU `BrowserView::` methods** | Same public `BrowserView` / `BrowserUiDelegate` surface |
| Fat wire logic still on `BrowserView` private API | **Composer types** own wire/sync bodies; `BrowserView` keeps fields + thin forwards |
| Flat `panels/` TU names (`processing_panels`, `map_inspect_panels`, `debug_console_wire`, …) | Colocated `*_composer.{h,cc}` per responsibility |

### Target composition (locked)

```
BrowserView                    # Widget tree + inspector placeholders + menus/status
  ├─ ShellLayoutComposer         # load_markup(main_app) + mount hosts (or imperative fallback)
  ├─ MapPagesComposer            # Map|Data|3D attach, overlays, gestures, tool seams
  ├─ ProcessingComposer          # Processing / playback / report / spatial / Python bridge
  ├─ InspectComposer             # Measure / selection / legend / layer props
  ├─ InspectorSyncComposer       # FeatureInfo / AttributeTable / edit feedback sync
  ├─ DebugConsoleComposer        # Diagnostic Tools + DebugAgent bind
  ├─ AtmosphereComposer          # Atmosphere inspector wire
  └─ ReportPanel*              # existing View host for plugin ReportBrowser
```

Rules:

- Public namespace stays `app` (composers are `app::*Composer`; no third semantic layer).
- Composers are **friends** of `BrowserView` and hold `BrowserView* host_` — field layout on `BrowserView` stays append-only (parallel-ninja `map_*` offset AV hazard unchanged).
- `BrowserView` private `wire_*` / sync / map helpers remain as **thin forwards** so call sites (`ensure_inspector_tab`, timers, `BrowserUiDelegate`) stay stable.
- Colocate `.h` with `.cc`; update `//src/app/views:shell_ui` sources in the same change. **No** forwarding headers at old `map_pages.cc` paths.
- Do **not** reopen a dated layout twin (ban list → this living file).

### Checklist

1. [x] Extract `MapPagesComposer` / `ProcessingComposer` / `InspectComposer` / `InspectorSyncComposer` / `DebugConsoleComposer` / `AtmosphereComposer`.
2. [x] `BrowserView` owns `unique_ptr` composers (append-only members); ctor wires them.
3. [x] GN `:shell_ui` sources + includes updated; old multi-TU `.cc` removed.
4. [ ] Optional: hoist duplicated `ptr_addr_poison` / `ptr_mem_readable` into `shell/ui/detail/ptr_guard.h`.
5. [x] Peel `build_contents` into `ShellLayoutComposer`: `load_markup("shell/main_app.ui.xml")` + host mount (Catalog / Map tabs / Ambox tool bar / right inspector / Diagnostic / Status); imperative fallback if markup missing. `splitter` markup tag + Yoga skip for splitter children.

---

## §ui/views/map subdirectory nest（2026-10-01）

As-built: `src/ui/views/map/` nests by responsibility — `viewport/` (`MapViewport` + display/paint/shell/flycube + features), `input/` (`viewport_input`, `TouchMultitouch`), `frame/` (identity HUD, embed fill), `device/` (legacy CreateRenderDevice helpers). **Public include paths stay** `"ui/views/map/map_viewport.h"` and `"ui/views/map/touch_multitouch.h"` via thin root forwards. Namespace remains `ui::views`. Module README: [`../../../src/ui/views/README.md`](../../../src/ui/views/README.md).

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

