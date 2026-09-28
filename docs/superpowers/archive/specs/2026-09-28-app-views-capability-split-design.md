<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/app/views` capability split (document / camera / present)


> **Status: superseded** (2026-09-28 merge). Merged into `2026-09-27-views-desktop-shell-design.md` §Capability split. Do not revise here except mechanical link fixes.

**Date:** 2026-09-28  
**Status:** active  
**Scope:** Replace the `document` / `input` / `scene3d` buckets with capability partitions. `MapScene` keeps map data. 2D pan/zoom and 3D orbit become cameras. Paint, GPU present, blit preview, and RHI/stereo device lifetime become presenters. `input/` stays a thin HWND adapter. One implementation plan after this spec is approved.  
**Related:**

| Topic | Doc | Relation |
| --- | --- | --- |
| Desktop shell menus | [`2026-09-27-views-desktop-shell-design.md`](2026-09-27-views-desktop-shell-design.md) | **active** — menus, navigation commands, and `shell/` stay. Its §3 tree for `document/` `input/` `scene3d/` is superseded here |
| Views toolkit | [`2026-09-19-ui-views-subdir-responsibility-design.md`](2026-09-19-ui-views-subdir-responsibility-design.md) | **active** — `ui/views` partitions are unchanged. `MapViewport` stays the HWND host |
| 2D RHI path | [`2026-09-27-views-2d-map-rhi-design.md`](2026-09-27-views-2d-map-rhi-design.md) | **active** — `present_gpu` false still falls back to full GDI paint |
| As-built | [`../../../src/app/views/README.md`](../../../src/app/views/README.md) | Update in the same change that lands the split |

---

## 1. Goal / Non-goals

### 1.1 Goal

1. A directory under `src/app/views/` names one capability: document, camera, present, input, or shell.
2. `MapScene` is the in-process map document: layers, features, OGR open/save, selection, map-space hit testing, land rings. It does not own a view transform, an `HDC`, or a `render::rhi::Device`.
3. One shared `content::Extent2` story: `ViewFrame` (2D) and `OrbitFrame` (3D) both frame that extent. `ViewNavigation` stays the session history.
4. `Map2dPresenter` and `Scene3dPresenter` draw. `Scene3dRhiSession` and `Scene3dStereoSession` are present backends.
5. `MapHwndGestures` keeps translating Win32 messages into callbacks. It does not include document, camera, or present headers.

### 1.2 Non-goals

- Do not change menu layout, navigation command ids, or AM Box grouping from the desktop-shell spec.
- Do not move or rename `shell/`.
- Do not change `ui::views::MapViewport`, `content::MapContents`, or `content::ViewHost`.
- Do not add a third public namespace. Types stay in `app`.
- Do not add shim headers at the old paths.
- Do not deepen past `src/app/views/<module>/`.
- Do not add Qt, a second widget kit, or a parallel 2D/3D document type.
- Do not change paint results, gesture math, atmosphere defaults, or GDI/GPU fallback rules.

---

## 2. Locked decisions

| # | Decision |
| --- | --- |
| 1 | Physical tree in §3. Includes move with the files. No forwarding headers. |
| 2 | `MapScene` drops `pan_*`, `scale_`, `paint*`, `present_gpu`, and `<windows.h>`. |
| 3 | Hit testing and vertex move take map coordinates. The shell converts client pixels with `ViewFrame`. |
| 4 | `ViewNavigation`, `map_host_extent.h`, and the China envelope move to `camera/`. |
| 5 | `BlitFrameCache` moves to `present/`. |
| 6 | `Scene3dController` splits into `OrbitFrame` + `Scene3dPresenter`. The controller type name goes away. |
| 7 | `input/map_hwnd_gestures.*` stays. Callbacks remain `std::function`. |
| 8 | Shell owns composition: one `MapScene`, one `ViewFrame` for Map/Data, one `OrbitFrame` for the 3D page, one presenter per mode, three `MapHwndGestures`. |
| 9 | GN test output names stay `map_scene_test`, `view_navigation_test`, `scene3d_controller_test`. Source-set labels may split so `:map_scene` no longer links GDI or RHI. |
| 10 | `BrowserView::document()` still returns `MapScene*`. `scene3d()` returns `Scene3dPresenter*`. Camera reads use `view_frame()` and `orbit_frame()`. Self-test call sites update in the same change. No new `--self-test` scenes. |
| 11 | Git: work on `master`. No feature branch. |

---

## 3. Target tree

```
src/app/views/
  document/
    map_scene.h / .cc
    map_scene_test.cc
  camera/
    view_frame.h / .cc
    view_frame_test.cc              # part of view_navigation_test
    orbit_frame.h / .cc
    view_navigation.h / .cc
    view_navigation_test.cc
    map_host_extent.h
  present/
    map2d_presenter.h / .cc
    map2d_presenter_test.cc         # part of map_scene_test
    blit_frame_cache.h / .cc
    scene3d_presenter.h / .cc
    scene3d_controller_test.cc      # binary name unchanged
    scene3d_rhi_session.h / .cc
    scene3d_stereo_session.h / .cc
  input/
    map_hwnd_gestures.h / .cc
  shell/                            # unchanged
```

Include examples:

- `content/browser/document/map_scene.h`
- `content/browser/camera/view_frame.h`
- `content/browser/camera/orbit_frame.h`
- `app/views/present/map2d/map2d_presenter.h`
- `app/views/present/scene3d/scene3d_presenter.h`

Cartography helpers that today sit next to `MapScene` (`map_scene_label_*`, `map_scene_line_*`, fill/stroke colors, label-box collision) move with `Map2dPresenter`. They are paint policy.

---

## 4. Type ownership

### 4.1 `document/MapScene`

Keeps: layer CRUD, `open_path` / `write_path` / `seed_default`, style document, basemap provider pointer, feature selection, `append_from_draft`, `copy_feature_xy`, `add_triangle_layer`, feature envelopes (`compute_extent`, `world_extent`, active-layer and selection envelopes), `export_land_rings`, inspector field helpers, feature tokens.

Hit test becomes map-space: the caller supplies the point already converted from the view. `move_selected_vertex` takes map coordinates the same way.

`set_basemap_provider` stays. Drawing tiles does not.

### 4.2 `camera/ViewFrame`

Owns 2D `pan_x`, `pan_y`, `scale`. Methods: `apply_pan`, `apply_zoom_at`, `apply_pinch`, `fit_extent`, `apply_world_extent`, `view_world_extent`, `map_to_view`, `view_to_map`, `scale`. It reads a feature envelope from `MapScene` when fitting. It does not store features.

### 4.3 `camera/OrbitFrame`

Owns `yaw`, `pitch`, `distance`, and the framed `content::Extent2`. Methods: `apply_wheel_at`, `apply_pan`, `apply_pinch`, `apply_nav_key`, `apply_world_extent`, `camera_matrices`, `camera_matrices_ortho`, `project`, `project_lon_lat`. Default yaw stays `gis::kDemDefaultOrbitYaw`.

`camera_matrices_ortho` is the ortho of that shared lon/lat extent (self-test and the 3D host). Map and Data pages do not use it; they use `ViewFrame` plus `Map2dPresenter`.

`push_extent_to_contents` / `pull_extent_from_contents` move to the shell. The frame does not hold a `MapContents*`.

### 4.4 `camera/ViewNavigation`

Moves unchanged: extent stack (cap 32), bookmarks, zoom-to-layer / zoom-to-selection status strings, `format_view_scale`.

### 4.5 `present/Map2dPresenter`

Owns `effect::map::Pass`, GDI `paint` / annotation / flash overlays, `present_gpu`, `export_bmp`, basemap tile drawing, and `paint_labels_projected`. Inputs are `const MapScene*`, `const ViewFrame*` (or an `OrbitFrame` projector for 3D labels), and the `HDC` or `Device` the shell passes in. `last_gpu_present_ok()` stays here.

### 4.6 `present/Scene3dPresenter`

Owns `gis::World`, `effect::scene::GpuScene`, atmosphere `Environment` and passes, wireframe flag, engine-name HUD, logo overlay HWND, `present_gpu`, GDI mesh `paint`, `paint_hud`, `abandon_mesh`, `bind_map` (land rings), `bind_contents` (shared DEM session). `run_m3_self_test_hooks` stays on this type.

It reads `const OrbitFrame*` for the camera. It does not apply pan, wheel, or pinch.

### 4.7 Present backends

`Scene3dRhiSession` and `Scene3dStereoSession` move to `present/` with the same attach / resize / present contracts. `BlitFrameCache` moves unchanged.

### 4.8 `input/MapHwndGestures`

Unchanged. The shell lambdas call `ViewFrame` or `OrbitFrame`.

---

## 5. Dependencies

```
shell    → input, present, camera, document
present  → camera, document, render/rhi, effect
camera   → content::Extent2
document → content, gis style/tile, tool::Draft
input    → tool::PointerPinchTracker, Win32
```

`document` does not include `camera`, `present`, or `<windows.h>`. `input` does not include `document`, `camera`, or `present`. `camera` does not include `present` or HWND types.

GN, after the split:

| Label | Sources | Drops |
| --- | --- | --- |
| `:map_scene` | `document/map_scene.*` | GDI, RHI, `effect/map` |
| `:map_camera` | `camera/*` except tests | — |
| `:map_present` | `present/map2d_presenter.*`, `present/blit_frame_cache.*` | — |
| `:map_hwnd_gestures` | unchanged | — |
| `:scene3d_present` | `present/scene3d_presenter.*`, both sessions | Dep on `:map_scene` for land rings stays; camera dep is `:map_camera` |

`:views` depends on all five. `view_navigation_test` depends on `:map_camera`, not `:map_scene`.

---

## 6. Data flow

**Pan / pinch.** `MapHwndGestures` invokes the shell callback. The shell picks `ViewFrame` on Map/Data and `OrbitFrame` on the 3D page, then invalidates. The active presenter paints.

**Open.** Shell calls `MapScene::open_path`, then `ViewFrame::fit_extent` (and `OrbitFrame::apply_world_extent`) from `MapScene::world_extent()`, then the presenter repaints. `ViewNavigation::commit` stays in the shell, as it is today.

**Hit / identify.** Shell converts client pixels with `ViewFrame::view_to_map`, then `MapScene` hit-tests in map space.

**3D present.** Shell passes the orbit frame into `Scene3dPresenter::present_gpu`. On failure, GDI wireframe paint runs. Stereo `try_present_sot` stays the product SoT when `legacy_render` is beside the PE; the session still does not link that DLL.

---

## 7. Failure behavior (unchanged)

| Case | Result |
| --- | --- |
| `open_path` cannot read vectors | Sample layer, `last_open_was_ogr() == false` |
| `Map2dPresenter::present_gpu` returns false | Full GDI `paint`, including labels |
| RHI or stereo attach/present fails | GDI mesh wireframe; HUD still draws |
| Navigation target empty or stack exhausted | Status text, extent unchanged |
| Atmosphere | Still off until `ensure_atmosphere` / demo seed |

---

## 8. Landing order

Each step leaves the exe behavior the same. Includes update in the same step that moves a file. No shim.

1. Move files whose API does not change: `view_navigation`, `map_host_extent.h`, `blit_frame_cache`, both sessions. Fix includes.
2. Extract `ViewFrame` from `MapScene`. Shell calls it. Delete the pan/zoom methods on `MapScene` in that same step.
3. Extract `Map2dPresenter`. `MapScene` loses `paint*` / `present_gpu` / `<windows.h>`.
4. Extract `OrbitFrame`. Extent push/pull moves to the shell.
5. Rename the remainder to `Scene3dPresenter` and delete `Scene3dController`.
6. Update `src/app/views/README.md` so the directory paragraph lists `document` / `camera` / `present` / `input`.

---

## 9. Tests

Existing assertions stay. Only include paths and the type that owns the method change. No new GN test label.

| Binary | Sources after the split | Covers |
| --- | --- | --- |
| `map_scene_test` | `document/map_scene_test.cc`, `present/map2d_presenter_test.cc` | Document: OGR, layers, selection, map-space hit test. Presenter file: the current `MapScene::present_gpu` Null-RHI cases |
| `view_navigation_test` | `camera/view_navigation_test.cc`, `camera/view_frame_test.cc` | Stack, bookmarks, scale text. `view_frame_test.cc` is required: `apply_pan` changes `map_to_view`; `fit_extent` with no vertices leaves pan and scale unchanged |
| `scene3d_controller_test` | `present/scene3d_controller_test.cc` | `OrbitFrame` pan and `camera_matrices_ortho`; `Scene3dPresenter::present_gpu` on Null RHI; atmosphere flags |

`map_scene_test` depends on `:map_scene` and `:map_present`. `view_navigation_test` depends on `:map_camera`. `scene3d_controller_test` depends on `:map_camera` and `:scene3d_present`.

Human runs, from the repo root:

```bat
build.bat
build.bat te
```

---

## 10. Docs when the code lands

- `src/app/views/README.md`: directory paragraph lists `document` / `camera` / `present` / `input`.
- This spec stays **active** until that change, then **landed** and archived with its plan.
- Desktop-shell spec stays **active** for menus. It does not regain the old `scene3d/` tree.
