<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/app/views` desktop shell (QGIS-style menus + map context)

**Date:** 2026-09-27  
**Status:** active  
**Updated:** 2026-09-28 — §Chromium-style app/views layering: S1–S5 landed (`shell_browser` / `shell_ui`, `BrowserUiDelegate`).  
**Scope:** Product host under `src/app/views`: menus + map context navigation **and** capability directories. Toolkit stays in `ui/views`.  
**Related:**

| Topic | Doc | Relation |
| --- | --- | --- |
| Views toolkit vs product host | [`2026-09-13-ui-views-controls-design.md`](2026-09-13-ui-views-controls-design.md) | widgets in `src/ui/views` |
| MFC → Views shell | [`2026-09-13-ui-views-mfc-migration-design.md`](2026-09-13-ui-views-mfc-migration-design.md) | `SmartGisViews.exe` entry |
| Toolkit subdirectory | [`2026-09-19-ui-views-subdir-responsibility-design.md`](2026-09-19-ui-views-subdir-responsibility-design.md) | `MenuBar` under `ui/views/primitives/menu/` |
| 2D frame + RHI present | [`2026-09-27-map2d-frame-design.md`](2026-09-27-map2d-frame-design.md) | CPU frame + Views GPU present |
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
- Do not add measure, layout composer, or Processing UI. Those stay on existing plans and plugins.
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

  document/                       # MapScene data only — see §Capability
  camera/                         # ViewFrame, OrbitFrame, ViewNavigation
  present/                        # map2d / scene3d present stack — see §Present
  input/                          # MapHwndGestures
```

Include examples after the shell reshape:

- `app/views/shell/app/browser_main.h`
- `app/views/shell/browser/browser.h`
- `app/views/shell/ui/browser_view.h`
- `app/views/document/map_scene.h`
- `app/views/camera/view_frame.h`
- `app/views/present/map2d/map2d_presenter.h`
- `app/views/present/scene3d/scene3d_presenter.h`
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
| Capability dirs | `document/` `camera/` `present/` `input/` siblings of `shell/` | None — keep |
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
| Namespaces | Stay `app`; no shim headers; capability root `app/views/<module>/`, layers nest under that module |
| Test binary names | `map_scene_test` / `view_navigation_test` / `scene3d_presenter_test` |

Non-goals: do not change menu ids / AM Box grouping from §2–§4; do not relocate `document/` / `camera/` / `present/` / `input/` for this Browser split; do not change paint results or GDI/GPU fallback rules (see map2d-frame living). Shell internal layout is owned by §Chromium Browser / BrowserView.

### §Present layering（Chromium-style, 2026-09-28）

As-built under `src/app/views/present/` (module README: [`../../../src/app/views/present/README.md`](../../../src/app/views/present/README.md)):

```
present/
  host/                 # BlitFrameCache — present-surface preview (StretchBlt)
  map2d/
    map2d_presenter.*   # Thin facade: bind, present_gpu, export
    frame/              # carto policy, LayerBatch, tile/mercator math
    paint/              # GDI paint TUs for Map2dPresenter
  scene3d/
    scene3d_presenter.* # Thin facade: bind, mesh, present_gpu
    session/            # prefer_scene3d_flycube policy + Scene3dStereoSession
    frame/              # OrbitGeoFrame + atmosphere field → pass prep
    paint/              # GDI HUD / wind / wireframe / engine logo
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
- Do not move `document/` / `camera/` / `present/` / `input/` under `shell/` (camera stays sibling).
- Do not introduce Qt or a second widget kit.
- Do not change paint results or GDI/GPU fallback rules (map2d-frame living).
- Do not open a new dated spec for this layout.
- Do not add compat shims at old paths.
