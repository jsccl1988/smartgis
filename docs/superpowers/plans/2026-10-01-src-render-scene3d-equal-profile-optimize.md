<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# src_render Scene3d equal-profile optimize — Implementation Plan

> Checklist hung off living [`../specs/2026-09-13-render-rhi-scene-design.md`](../specs/2026-09-13-render-rhi-scene-design.md) §src_render Scene3d equal-profile optimize.

**Goal:** Under the same 640×480 atmosphere-showcase=legacy profile as leftover scene3d, bring **warm** `src_render` `ms_per_present` into leftover’s order (~10 ms), without dropping ocean sea plane / hypsometric DEM.

**Architecture:** Keep `Scene3dGpuPresent` → `GpuScene` → FlyCube. Optimize by **fair phase timing**, **DEM StaticReuse**, and **one-shot remesh after ocean/sky resource alloc** — not by stripping ocean.

**Tech Stack:** C++23, content scene3d present, effect/scene, harness `--atmosphere-showcase=legacy`.

## Baseline (2026-10-01 → 2026-10-02, Debug)

| Path | Metric | ms | Notes |
| --- | --- | ---: | --- |
| leftover scene3d | warm present | ~10 | HWND present |
| src_render legacy (before) | `ms_per_present` | ~160–444 | every-frame `rebuild_meshes` + DEM cache miss |
| src_render legacy (after) | `ms_per_present` | **~14.2** | rebuild_count=0; ocean_prep=0 warm; record≈8 |

Artifact: `out/Debug/captures/atmosphere/atmosphere-showcase-perf.json`.

## Tasks

### Task 1: Pin every-frame rebuild

- [x] Root cause: `mark_meshes_dirty` whenever `ocean_on || sky_on`; `rebuild_local_mesh` clears xyz before DEM cache check; overlay remove/reattach bumps World generation
- [x] Gate remesh to first ocean-height / sky-depth sync (`dem_gpu_synced_after_*`)
- [x] DEM cache: trim overlay fold instead of clear; overlay attach only when DEM rebuilt / dirty

### Task 2: Present phase clocks

- [x] `Scene3dPhaseSample`: mesh / sync / rebuild / ocean_prep / record / present_swap + `rebuild_count`
- [x] Atmosphere showcase JSON + stderr print last-frame phases

### Task 3: Warm acceptance

- [x] Timed frames `rebuild_count=0` (Null RHI bench 2026-10-01 23:45 — phase mesh/sync/rebuild=0)
- [x] `SMT_ATMOSPHERE_SHOWCASE_GPU=1` + `LINGER_MS=0` + `PRESENT_COUNT=30` warm `ms_per_present` ≈ leftover order (**14.2 ms**, was 160–444; leftover ~10)
- [x] Visual: legacy PASS (landish / black-clear) on GPU path
- [x] Timed loop: skip `pump_messages` when `present_pump_ms==0` (was dispatching main map2d GDI paint ~160 ms/frame)
- [x] Hot path: stop double Gerstner (`OceanPass::record` after `prepare_gpu`); warm skip `prepare_gpu` once `dem_gpu_synced_after_ocean_`

**Bench notes (2026-10-01 / 2026-10-02):**

| Run | gpu | ms/p | rebuild_count | Notes |
| --- | ---: | ---: | ---: | --- |
| prior baseline | 1 | ~444 | every frame | mark_dirty + DEM cache miss |
| Null after remesh fix | 0 | 159.8 | **0** | wall dominated by `pump_messages(0)`→map2d paint |
| after pump skip (GPU) | 1 | 27.5 | **0** | ocean_prep=9 record=6 swap=1; PASS BMP |
| after ocean single-bake | 1 | **14.2** | **0** | ocean_prep=0 warm; record=8 swap=1; PASS |

```bat
set SMT_ATMOSPHERE_SHOWCASE_PRESENT_COUNT=30
set SMT_ATMOSPHERE_SHOWCASE_GPU=1
set SMT_ATMOSPHERE_SHOWCASE_LINGER_MS=0
out\Debug\SmartGisViews.exe --atmosphere-showcase=legacy
type out\Debug\captures\atmosphere\atmosphere-showcase-perf.json
```

## Non-goals

- Matching leftover by deleting ocean / hypsometric DEM
- Full atmosphere.full warm diet (sky path) in the same slice (reuse one-shot flags; separate follow-up if needed)
- Sub-10 ms Debug warm while Gerstner+upload still run every animated ocean frame (follow-up: cheaper wave step / lower mesh_n)
