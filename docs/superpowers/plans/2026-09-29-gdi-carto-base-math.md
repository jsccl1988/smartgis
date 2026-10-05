<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# GDI carto + base/math 2D batch Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Accelerate leftover GDI multi-point `LPToDP` and carto declutter hotspots via `base/math` 2D Affine + optional SIMD batch, without changing carto style semantics.

**Architecture:** Spec § in [`../specs/2026-09-13-render-rhi-scene-design.md`](../specs/2026-09-13-render-rhi-scene-design.md) (§GDI carto + base/math 2D). `LpToDp2` + `transform_xy_batch` in `src/base/math`; carto uses `Vector2`/`deg_to_rad` and a point cell grid; GDI device/thread multi-point draws fill float xy then batch.

**Tech Stack:** C++23, `//src/base/math:math`, leftover GDI under `legacy/render/rhi2d/impl/gdi`.

## Global Constraints

- Work on **`master`** only; non-overlapping paths with other agents.
- Do **not** change carto colors / LOD / priority / budget policy.
- Do **not** enable `base_math_simd` by default.
- Do **not** multi-thread GDI feature drawing.
- Comments in English on touched code.
- Do **not** `git commit` unless the user asks.

## File map

| Path | Role |
| --- | --- |
| `src/base/math/linear/affine2.h` | `LpToDp2` + single-point `transform_xy` |
| `src/base/math/simd/simd.h` `.cc` | `transform_xy_batch` |
| `src/base/math/math.h` | include `linear/affine2.h` |
| `src/base/math/math_test.cc` | LPToDP-equivalent rounding |
| `…/gdi/carto/map_carto2d.*` | Vector2 geometry; point grid |
| `…/gdi/device|thread/*` | batch LPToDP on multi-point draws |
| `…/gdi/BUILD.gn` | dep `//src/base/math:math` |

---

### Task 1: base/math Affine2 + batch

- [x] **Step 1:** Add `affine2.h` (`LpToDp2`, `transform_xy`) matching leftover `LPToDP` (+0.5 then Y flip).
- [x] **Step 2:** Add `transform_xy_batch` to `simd.h` / `simd.cc` (scalar default).
- [x] **Step 3:** Extend `math_test`; `build.bat debug math_test`.

### Task 2: carto geometry + point grid

- [x] **Step 1:** `label_box_rotated` / `line_label_pose` use `Vector2` + `deg_to_rad` / `rad_to_deg`.
- [x] **Step 2:** `try_keep_point` cell grid (same key style as labels).
- [x] **Step 3:** `build.bat debug map_carto2d_test`.

### Task 3: GDI multi-point batch

- [x] **Step 1:** Device + thread: build `LpToDp2` from viewport/window/fblc; multi-point loops → float xy → `transform_xy_batch` → `POINT*`.
- [x] **Step 2:** Single-point `LPToDP` may call `transform_xy` (ABI unchanged).
- [ ] **Step 3:** `build.bat debug gdi_map_paint_test` green (currently AV during zoom; layout-split collision may dominate — recheck after layout agent settles).

## Done when

- Scalar path bit-matches prior `LPToDP` for sample points. ✅ `math_test`
- `map_carto2d_test` green. ✅
- GDI paint still produces pixels. ⏳ blocked / AV at zoom (see Step 3)
- Umbrella § accepted; this plan checkboxes updated.
