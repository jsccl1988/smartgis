<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# 3D DEM align SmartGis.exe (A+B) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** DemRaster is DEM authority; hosts match SmartGis.exe framing/cutline; leftover keeps GL StereoTerrain but per-scene seeds from DemRaster (2a).

**Architecture:** Shared cutline + frame constants in `gis/`; `Scene3dController` consumes them; leftover `DemHeightField` thin-filled from DemRaster; remove global DEM short-circuit on re-seed.

**Tech Stack:** C++23, GN, `gis::DemRaster`, `app::Scene3dController`, leftover `StereoTerrain` / GL.

## Global Constraints

- Stay on **master**; no new branches.
- **Do not commit** unless the user explicitly asks.
- Comments English; user docs Chinese OK.
- Copyright year **2026** The Mogu Authors on new/touched headers.
- New-tree functions `snake_case`; leftover export names unchanged.
- No Qt. Views must not `#include "legacy/…"`.
- Path partition for parallel agents: Task G/A vs Task L must not edit the same files.

## File map

| Area | Paths |
|------|--------|
| Spec | `docs/superpowers/specs/2026-09-19-leftover-scene3d-dem-unify-design.md` |
| Plan | `docs/superpowers/plans/2026-09-19-leftover-scene3d-dem-unify.md` |
| Task G (gis) | `src/gis/world/dem_raster.{h,cc}`, optional `dem_frame.h`, `dem_raster_test.cc`, BUILD.gn if needed |
| Task A (hosts) | `src/app/views/scene3d_controller.{h,cc}`, `scene3d_controller_test.cc`, `main.cc` only if self-test yaw asserts |
| Task L (leftover 2a) | `src/legacy/render/scene3d/dem_height_field.{h,cc}`, `map_to_scene.{h,cc}`, `dem_stereo_test.cc`, maybe `dem_to_world.*` |
| Do not touch | `src/legacy/render/bridge/**`, leftover 2b OnDraw→controller |

---

## Task G: DemRaster cutline + shared frame constants — PARALLEL with L

**Paths only:** `src/gis/world/**` (+ this plan/spec). Do **not** edit `src/legacy/**` or `src/app/**`.

- [x] **G1** Add shared default orbit yaw constant (e.g. `kDemDefaultOrbitYaw = π − 0.55f`) in a small gis header; comment ties to leftover south-of-target / 上北下南.
- [x] **G2** Align `seed_china_dem_into_world` cutline with leftover: if loaded path contains `china_dem`, **skip** `mask_outside_rings` even when rings are passed; synthetic / other rasters may mask with mainland-contains caution (document in comment).
- [x] **G3** Tests in `dem_raster_test`: mesh lon on X / lat on +Z; Tibet > Jiangsu; cutline skip behavior when path is china_dem (mock or conditional on fixture).
- [x] **G4** Build/run `dem_raster_test` green. Do not commit.

**Done when:** Hosts and leftover can both rely on one cutline + yaw constant from gis.

---

## Task A: Host Scene3dController parity — after G1 (or parallel if constant name agreed)

**Paths only:** `src/app/views/scene3d_controller.*`, `scene3d_controller_test.cc`, `main.cc` (self-test only). Prefer not editing `map_scene.*` unless required.

- [x] **A1** Use gis shared yaw constant (replace local `kScene3dDefaultYaw` or make it alias).
- [x] **A2** Remove temporary `present_gpu` stderr bisect `fprintf` marks.
- [x] **A3** Ensure `rebuild_local_mesh` benefits from G2 cutline (no extra remask logic in views).
- [x] **A4** `scene3d_controller_test` (+ main self-test if present) assert default yaw ≈ shared constant.
- [x] **A5** Build/run `scene3d_controller_test` green. Do not commit.

**Done when:** Views 3D default framing matches leftover intent; no debug spam.

---

## Task L: Leftover 2a DemRaster-backed per-scene seed — PARALLEL with G

**Paths only:** `src/legacy/render/scene3d/**`. Do **not** edit `src/gis/**` or `src/app/**` (consume existing DemRaster APIs; if G2 not landed yet, call DemRaster load + local cutline copy matching map_to_scene today, then switch to seed_china helper when available).

- [x] **L1** Thin path: load via `gis::DemRaster` then fill/adapt `DemHeightField` (keep export API; rgb/nrm `build_mesh` may stay local). Prefer one `find_sample_dem_path` — forward leftover to gis if both exist.
- [x] **L2** Per-scene terrain: `seed_stereo_underlay` must **Add3DObject(StereoTerrain)** for the passed `scene` every successful seed; height field lifetime owned so terrain’s const pointer stays valid (heap per seed or scene-associated store). Eliminate “global already non-empty → skip attaching terrain to this scene”.
- [x] **L3** `leftover_has_scene_dem` / `leftover_dem_aabb` must not block a second view’s seed; update callers in the same files only.
- [x] **L4** `dem_stereo_test`: two scenes (or clear + re-seed) both get DEM; load-via-DemRaster still passes Tibet/Jiangsu and mesh tests.
- [x] **L5** Build/run `dem_stereo_test` (and scene3d ninja target if needed) green. Do not commit.

**Done when:** SmartGis.exe second 3D view/scene can seed DEM; heights from DemRaster authority.

---

## Integration (parent)

- [x] Confirm G + A + L no file overlap conflicts; resolve merge if any.
- [x] Optional: `build.bat` / relevant tests once (`dem_stereo` / `dem_raster` / `scene3d_controller` exit 0; prior e2e+te exit 0 after Views flat-map overlay fix).
- [x] Spec/plan checkboxes updated; **no commit** unless user asks.
