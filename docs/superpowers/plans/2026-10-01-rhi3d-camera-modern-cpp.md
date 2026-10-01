<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# rhi3d public/camera modern C++ Implementation Plan

> **For agentic workers:** implement task-by-task. Spec § under [`../specs/2026-09-13-render-rhi-scene-design.md`](../specs/2026-09-13-render-rhi-scene-design.md) (§rhi3d camera modern C++).

**Goal:** Modernize leftover `rhi3d/public/camera` with C++23 + `base::math::Vector3`, snake_case methods, merge Combined into Persp, `unique_ptr` factory.

**Architecture:** Declarations in `camera.h`, bodies in `camera.cc`. Keep `Smt*` type names. Delete `combined_camera.h`. Update all call sites.

**Tech Stack:** C++23, `base/math`, Win32 cursor APIs for FPS only.

## Global Constraints

- Work on **`master`** only.
- Keep class names `SmtCamera` / `SmtPerspCamera` / `SmtFPSCamera` / `SmtArbvCamera` / `SmtOrthCamera`.
- Methods `snake_case`; `const&` inputs; getters return `const&` where safe.
- `make_view3d_camera` → `std::unique_ptr<SmtPerspCamera>`; tool may `.release()` into raw owner.
- No new dated design twin — living § only.
- Do not `git commit` unless asked.

## Tasks

### Task 1: camera.h + camera.cc + delete combined

- [x] Rewrite `camera.h` (decls only) + `camera.cc` (impl + detail orbit helpers).
- [x] Fold Combined APIs onto `SmtPerspCamera`.
- [x] Delete `combined_camera.h`.
- [x] Add `camera.cc` to `rhi3d/BUILD.gn` `rhi_sources`.

### Task 2: Call sites

- [x] `3dviewctrltool.cpp`, `map_to_scene.cc`, `scene.cpp`, `northarray.cpp`, `stereo_hwnd_view.cc`, `view_3d.cpp`.
- [x] Living § + Active row date.

### Task 3: Build

- [x] `build.bat debug legacy_render` (and dependents that touch camera) green.

## Done when

- No `combined_camera.h`; no PascalCase camera methods.
- Factory returns `unique_ptr`.
- Debug `legacy_render` links green.
