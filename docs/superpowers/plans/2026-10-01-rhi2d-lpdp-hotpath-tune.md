<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# rhi2d LP↔DP hot-path tune — Implementation Plan

> **For agentic workers:** implement task-by-task. Spec § in [`../specs/2026-09-13-render-rhi-scene-design.md`](../specs/2026-09-13-render-rhi-scene-design.md) (§rhi2d LP↔DP hot-path tune).

**Goal:** Close leftover LP↔DP gaps: batch `draw_linear_ring`, cache `LpToDp2` on xform, optional AVX2 `transform_xy_batch`, keep prep play free of re-project.

**Architecture:** Stay on `LpToDp2` / `transform_xy*`. No 4×4 `Matrix`. SIMD only behind `smt_render_math_simd` (default off).

**Tech Stack:** C++23, `//src/base/math:math`, leftover rhi2d `impl/common`.

## Global Constraints

- Work on **`master`** only.
- No `#include <Eigen/…>` under `rhi2d/`.
- Do not force 2D through 4×4 `Matrix`.
- Do not enable `smt_render_math_simd` by default.
- Do not `git commit` unless asked.

## Tasks

### Task 1: P0 `draw_linear_ring` batch

- [x] Pack ring verts + `transform_xy_batch`; `bShowPoint` uses projected pts.
- [x] `build.bat debug legacy_rhi2d_gdi` green.

### Task 2: P1 xform `LpToDp2` cache

- [x] `Rhi2dCartoDrawXform`: mutable cache + fingerprint; `set_context` invalidates.
- [x] `lp_to_dp` / `dp_to_lp` use cache.

### Task 3: P2 AVX2 `transform_xy_batch`

- [x] SIMD path ≡ scalar `+0.5` / `LONG` / `flip_y`; remainder scalar.
- [x] `math_test` larger batch vs per-point `transform_xy`.

### Task 4: P3 prep play invariant

- [x] Comment on `draw_prepared_batch`: device `POINT` only, no LP→DP.

### Done when

- Checklist above green; Debug `math_test` + `legacy_rhi2d_gdi` pass.
