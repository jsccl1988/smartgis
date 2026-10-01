<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# rhi2d Eigen / base/math deepen Implementation Plan

> **For agentic workers:** implement task-by-task. Spec § in [`../specs/2026-09-13-render-rhi-scene-design.md`](../specs/2026-09-13-render-rhi-scene-design.md) (§rhi2d Eigen / base/math deepen).

**Goal:** Unify leftover rhi2d LP↔DP + multi-point paint onto `base/math` (`LpToDp2` / `inverse_xy` / `transform_xy_batch` / `Vector2`), with Eigen only behind `base/math`.

**Architecture:** Extend `affine2.h`; rewrite `style/xform.cc` + device `DPToLP`; batch spline / primitives / mesh / MBR in `impl/common/` (GDI product path).

**Tech Stack:** C++23, `//src/base/math:math`, leftover rhi2d common.

## Global Constraints

- Work on **`master`** only.
- No `#include <Eigen/…>` under `rhi2d/`.
- Do not force 2D through 4×4 `Matrix`.
- Do not enable `smt_render_math_simd` by default.
- Do not `git commit` unless asked.

## Tasks

### Task 1: `inverse_xy` + tests

- [x] `affine2.h`: `inverse_xy` matching leftover DPToLP unflip.
- [x] `math_test` round-trip.
- [x] `build.bat debug math_test`.

### Task 2: Unify call sites

- [x] `paint/carto/style/xform.cc` → `make_lp_to_dp` + `transform_xy` / `inverse_xy`.
- [x] `host/device_interact.cc` `DPToLP` → `inverse_xy`.

### Task 3: Batch remaining multi-point

- [x] `draw_ogr` spline; `draw_primitives` polyline; `draw_mesh` tin/grid; `map_painter` MBR.
- [x] `carto_frame` / anno offset use `Vector2` helpers.
- [x] `build.bat debug legacy_rhi2d_gdi` green.

## Done when

- Hand LP↔DP formulas gone from xform + device DPToLP.
- Multi-point leftover draws use `transform_xy_batch` where N≥2 loops existed.
- Debug `math_test` + `legacy_rhi2d_gdi` green.
