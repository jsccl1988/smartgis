<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# src_render Scene3d equal-profile optimize — Implementation Plan

> Checklist hung off living [`../specs/2026-09-13-render-rhi-scene-design.md`](../specs/2026-09-13-render-rhi-scene-design.md) §src_render Scene3d equal-profile optimize.  
> **Parallel / prep default-off:** [`2026-10-02-src-render-vista-parallel-accelerate.md`](2026-10-02-src-render-vista-parallel-accelerate.md) Task 4 (`prep_cull_parallel` · `GPUSCENE_PREP_PARALLEL`).  
> **Diagram:** [`../diagrams/render-accelerate-topology.html`](../diagrams/render-accelerate-topology.html) §8 Scene3d cold vs warm.

**Goal:** Keep atmosphere-showcase=legacy **warm** present in leftover order; drive **world3d** equal-profile matrix so Vista **cold** first-frame matches leftover order without dropping DEM / stripping materials on smoke rows.

**Architecture:** Keep `Scene3dGpuPresent` → `GpuScene` → Vista. Optimize by **fair phase timing** (cold vs warm), **DEM StaticReuse**, **cold cache+upload merge**, and **honest prep** (parallel only after frustum cull) — not by stripping ocean/DEM.

**Tech Stack:** C++23, content scene3d present, effect/scene, harness `--atmosphere-showcase=legacy` · `--plugin-showcase=world3d` · `run_world3d_backend_matrix.py`.

## World3d matrix (normative · 2026-10-03)

| Axis | Lock |
| --- | --- |
| Primary metric | **warm** `ms_per_present` — **n=5**, **discard_cold=1** |
| Perf rows | `flycube`, `prep_par_off`, `prep_par_on`, `gl_scenic`, `d3d_scenic` |
| Smoke | `null` only (full materials; never a performance peer) |
| Warm status | Already peer ~**5–6.5 ms** — protect |
| Cold P0 | Vista ~**1.8–2 s** vs leftover ~**90 ms** |
| Prep | `prep_par_on` currently slower; product `GPUSCENE_PREP_PARALLEL` **default off** until frustum cull |
| Artifacts | `out/Debug/captures/analysis/world3d_opt/matrix/` |

```bat
.\build.bat debug src/app/views:views
.\build.bat debug SmartGis
py -3 testing/tools/harness/plugin/run_world3d_backend_matrix.py
```

## Baseline — atmosphere legacy (2026-10-01 → 2026-10-03, Debug)

| Path | Metric | ms | Notes |
| --- | --- | ---: | --- |
| leftover scene3d | warm present | ~10 | HWND present |
| src_render legacy (before) | `ms_per_present` | ~160–444 | every-frame `rebuild_meshes` + DEM cache miss |
| src_render legacy (after) | `ms_per_present` | **~10–14** | rebuild_count=0; ocean_prep=0 warm |

Artifact: `out/Debug/captures/browser/atmosphere-showcase-perf.json`.

## Milestones M1–M4 (world3d · active)

### M1 — Cold phases in JSON

- [ ] Matrix / `*-perf.json` expose named **cold** phase clocks (upload / mesh / rebuild / record / present as applicable)
- [ ] Warm primary remains n=5 discard_cold=1; cold reported separately (not folded into warm mean)
- [ ] `MATRIX.md` / CSV distinguish warm vs cold columns

### M2 — Cache + upload (cold ≤ 300 ms)

- [ ] Vista first-frame **cold ≤ 300 ms** (baseline ~1.8–2 s; leftover peer ~90 ms)
- [ ] Prefer StaticReuse / batch upload / cache hit — do **not** strip DEM
- [ ] Re-run matrix; artifacts under `out/Debug/captures/analysis/world3d_opt/matrix/`

### M3 — Prep honesty

- [ ] `prep_par_on` not slower than `prep_par_off` without real frustum work
- [ ] Product default: `GPUSCENE_PREP_PARALLEL` **off** until `SCENE3D_FRUSTUM_CULL` + §vista parallel Task 4
- [ ] Cross-check [`2026-10-02-src-render-vista-parallel-accelerate.md`](2026-10-02-src-render-vista-parallel-accelerate.md) Task 4 checkboxes

### M4 — Non-bare budgets

- [ ] Document / gate **non-**`PERF_BARE` (full atmosphere materials) warm + cold budgets
- [ ] Smoke `null` stays full-materials; perf rows may stay bare until M4 budgets land
- [ ] Visual gates unchanged (DEM present; no fabricated BMP)

## Tasks (atmosphere warm — done)

### Task 1: Pin every-frame rebuild

- [x] Root cause: `mark_meshes_dirty` whenever `ocean_on || sky_on`; `rebuild_local_mesh` clears xyz before DEM cache check; overlay remove/reattach bumps World generation
- [x] Gate remesh to first ocean-height / sky-depth sync (`dem_gpu_synced_after_*`)
- [x] DEM cache: trim overlay fold instead of clear; overlay attach only when DEM rebuilt / dirty

### Task 2: Present phase clocks

- [x] `Scene3dPhaseSample`: mesh / sync / rebuild / ocean_prep / record / present_swap + `rebuild_count`
- [x] Atmosphere showcase JSON + stderr print last-frame phases

### Task 3: Warm acceptance

- [x] Timed frames `rebuild_count=0` (Null RHI bench 2026-10-01 23:45 — phase mesh/sync/rebuild=0)
- [x] `ATMOSPHERE_SHOWCASE_GPU=1` + `LINGER_MS=0` + `PRESENT_COUNT=30` warm `ms_per_present` ≈ leftover order (**14.2 ms**, was 160–444; leftover ~10)
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
| 2026-10-03 solid-cache + no timed marks | 1 | **10.1** | **0** | albedo/`_putenv_s` cached; timed mark I/O off; record≈5 swap=1; PASS |

```bat
set ATMOSPHERE_SHOWCASE_PRESENT_COUNT=30
set ATMOSPHERE_SHOWCASE_GPU=1
set ATMOSPHERE_SHOWCASE_LINGER_MS=0
out\Debug\SmartGIS.exe --atmosphere-showcase=legacy
type out\Debug\captures\atmosphere\atmosphere-showcase-perf.json
```

## Non-goals

- Matching leftover by deleting ocean / hypsometric DEM
- Ranking matrix rows by process `wall_ms`
- Treating `null` / GDI as performance peers
- Enabling `GPUSCENE_PREP_PARALLEL` by default before frustum cull honesty (M3)
- Full atmosphere.full warm diet in the same slice as M2 cold upload (M4 follow-up)
- Sub-10 ms Debug warm while Gerstner+upload still run every animated ocean frame (follow-up: cheaper wave step / lower mesh_n)
