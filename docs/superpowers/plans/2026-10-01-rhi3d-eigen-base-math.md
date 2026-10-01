<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# rhi3d Eigen / base/math frustum Implementation Plan

> **For agentic workers:** implement task-by-task. Spec § under [`../specs/2026-09-13-render-rhi-scene-design.md`](../specs/2026-09-13-render-rhi-scene-design.md) (§rhi3d Eigen / base/math frustum).

**Goal:** Replace leftover hand frustum / `SmtFrustum` with Eigen-backed `base::Frustum` + `Matrix::view_look_at`, update GL/D3D + scene3d callers.

**Architecture:** Shared extract in `base/math/frustum` (column-major clip → outward planes). D3D `GetFrustum` via `from_view_proj(MV*P)`; GL via clip from `glGetFloatv`. Cull via `Frustum::intersects(Aabb)`.

**Tech Stack:** C++23, Eigen via `base/math` only in rhi3d/scene3d.

## Global Constraints

- Work on **`master`** only.
- No Eigen includes in rhi3d/scene3d — only `base/math`.
- Preserve leftover MVP cull semantics (golden gate).
- Do not `git commit` unless asked.

## Tasks

### Task 1: base/math frustum + view_look_at + golden test

- [x] Fix `Frustum::from_view_proj` (+ `from_column_major_clip`) to match leftover extract, store **outward** planes (header-inline).
- [x] Add `Matrix::view_look_at` (gluLookAt RH).
- [x] Extend `math_test` with golden AABB in/out + timing print.

### Task 2: Purge SmtFrustum; device API

- [x] Delete `SmtFrustum` / `FrustumSide` / `PlaneData` from `rhi3d/public/device/base.h`.
- [x] `GetFrustum(Frustum&)` on `Smt3DRenderDevice` + GL/D3D overrides.
- [x] D3D: `SetViewLookAt` → `view_look_at`; `GetFrustum` → `from_view_proj`.
- [x] GL: `GetFrustum` → `from_column_major_clip` from GL clip product.

### Task 3: scene3d + tests

- [x] octree / scene / pointcloud: `Frustum` + `intersects(Aabb)`.
- [x] Update `gl_texture_test` / `d3d_texture_test`.
- [x] `build.bat debug math_test legacy_render legacy_render_gl legacy_render_d3d` green.

## Done when

- No `SmtFrustum` in tree under `src/legacy/render`.
- Golden + texture tests pass; living § acceptance boxes checked.
