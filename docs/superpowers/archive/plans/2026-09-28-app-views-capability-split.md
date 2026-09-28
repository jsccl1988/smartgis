<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# App views capability split Implementation Plan


> **Status: superseded** (2026-09-28 merge). Merged into views-desktop-shell plan. Do not revise here except mechanical link fixes.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Split `src/app/views` so document, camera, and present are separate types, with `input/` left as the HWND adapter.

**Architecture:** Two writers create `camera/` and `present/` without editing originals. A coordinator then deletes the moved methods from `MapScene` and `Scene3dController`, retargets `shell/` and `BUILD.gn`, and removes the old files. No shim headers.

**Tech Stack:** C++23, GN, `app` namespace, existing `MapScene` / `Scene3dController` behavior.

## Global Constraints

- Namespace stays `app`. New functions are `snake_case`. Types stay PascalCase.
- No third public namespace. No Qt. No shim headers. Do not nest past `src/app/views/<module>/`.
- Do not change paint results, gesture math, atmosphere defaults, or GDI/GPU fallback.
- Do not move `shell/` or `input/`. Do not change `ui::views::MapViewport` or `content::MapContents`.
- Agents do not run `build.bat`, `gn`, `ninja`, `cl`, or test binaries. The human compiles.
- Work on `master`. Do not create a branch. Do not commit unless the user asks.
- Copyright year 2026. New comments in English. Colocate each header with its `.cc`.

**Spec:** `docs/superpowers/specs/2026-09-28-app-views-capability-split-design.md`

## Tracks

Shared files (`BUILD.gn`, `shell/`, `document/map_scene.*`, `scene3d/scene3d_controller.*`, this plan) belong to the coordinator. Track writers only create files under their directory and leave the originals in place.

### Track A — `src/app/views/camera/`

- [ ] Copy `document/map_host_extent.h` → `camera/map_host_extent.h`
- [ ] Copy `document/view_navigation.h/.cc` and `view_navigation_test.cc` → `camera/`, includes point at `app/views/camera/map_host_extent.h` only if they included the old header
- [ ] Add `ViewFrame` and `view_frame_test.cc`
- [ ] Add `OrbitFrame` (camera half of `Scene3dController`)

`ViewFrame` (`app/views/camera/view_frame.h`):

```cpp
class ViewFrame {
 public:
  void apply_pan(int dx_px, int dy_px);
  void apply_zoom_at(int view_x, int view_y, double factor);
  void apply_pinch(int view_x, int view_y, double scale);
  void apply_world_extent(const content::Extent2& e, int view_w, int view_h);
  void fit_extent(const MapScene& scene, int view_w, int view_h);
  content::Extent2 view_world_extent(int view_w, int view_h) const;
  void map_to_view(double mx, double my, int* vx, int* vy) const;
  void view_to_map(int vx, int vy, double* mx, double* my) const;
  double scale() const;
  double pan_x() const;
  double pan_y() const;
};
```

Copy the bodies from `MapScene::apply_pan`, `apply_zoom_at`, `apply_pinch`, `apply_world_extent`, `view_world_extent`, `map_to_view`, `view_to_map`. Defaults: `pan_x_ = 0`, `pan_y_ = 0`, `scale_ = 1`. `fit_extent`: if `scene.has_china_extent()` call `apply_world_extent(kChinaLonLatExtent, ...)`; else `scene.polygon_fit_box` and keep the `< 1.0` pad plus `frame_world_extent` margin `0.08`. Empty box returns without changing pan or scale.

`OrbitFrame` (`app/views/camera/orbit_frame.h`): yaw/pitch/distance/extent, `apply_wheel_at`, `apply_pan`, `apply_pinch`, `apply_nav_key` (W/S/A/D and arrows only; K/J return false), `apply_draft` (no wireframe), `camera_matrices`, `camera_matrices_ortho`, `project`, `project_lon_lat`, `remember_view_size`, `world_extent` (`china_or`), `apply_world_extent`, `reset`. Defaults: yaw `kScene3dDefaultYaw`, pitch `0.4f`, distance `3.2f`. `kFovY = 0.785398f`. No `MapContents*`. No `push_extent_to_contents` / `pull_extent_from_contents`.

### Track B — `src/app/views/present/`

- [ ] Copy blit cache and both sessions into `present/`
- [ ] `Scene3dRhiSession::present` takes `Scene3dPresenter*`
- [ ] Add `Map2dPresenter` by copying `MapScene` paint / `present_gpu` / `export_bmp` and the `map_scene_*` free functions
- [ ] Add `Scene3dPresenter` as `Scene3dController` minus the camera fields
- [ ] `present/map2d_presenter_test.cc` holds the current Null-RHI `present_gpu` cases
- [ ] `present/scene3d_controller_test.cc` is the existing 3D test retargeted at `OrbitFrame` + `Scene3dPresenter`

`Map2dPresenter` binds `const MapScene*` and `const ViewFrame*`. Pan, scale, and `map_to_view` come from the frame. Layers, style colors, and `basemap_provider()` come from the scene. `layers()` and `style_colors_for_feature` are already public.

`Scene3dPresenter` holds `const OrbitFrame* orbit_`. `present_gpu` uses `orbit_->camera_matrices`. K/J wireframe stays in `Scene3dPresenter::apply_draft` before `orbit_->apply_draft`. Do not copy `push_extent_to_contents` or `pull_extent_from_contents`.

### Track C — coordinator, after A and B

- [ ] Point `shell/` and `main.cc` at the new types. `BrowserView` owns one `ViewFrame`, one `OrbitFrame`, one `Map2dPresenter`, one `Scene3dPresenter`.
- [ ] Move extent push/pull onto the shell, reading `OrbitFrame`.
- [ ] Delete camera and paint methods from `MapScene`. Delete `scene3d/scene3d_controller.*` and the old copies under `document/` (`view_navigation`, `blit_frame_cache`, `map_host_extent.h`).
- [ ] `hit_test` / `move_selected_vertex` take map coordinates. Shell converts with `ViewFrame`.
- [ ] Split GN: `:map_scene` drops GDI/RHI; add `:map_camera`, `:map_present`, `:scene3d_present`. Test output names stay `map_scene_test`, `view_navigation_test`, `scene3d_controller_test`.
- [ ] Update `src/app/views/README.md`.

Human verify:

```bat
build.bat
build.bat te
```
