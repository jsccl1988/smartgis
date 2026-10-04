---
name: harness-auto-world3d-opt
description: >-
  Runs the world3d equal-profile harness: same True-Earth Scene3D rendering
  materials and effects across parallel strategies x image-driven backends
  (Leftover/GL, Leftover/D3D11, FlyCube/DX12; Null smoke-only), then emits
  screenshots and an execution performance comparison table. Use when the user
  invokes /harness-auto-world3d-opt, or says world3d 矩阵, 同等渲染物料,
  并行策略×图像驱动, GL/D3D/FlyCube 对比, world3d equal-profile, or asks for
  world3d screenshots + performance comparison.
---

<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Harness auto world3d opt（同等物料 × 并行×图像驱动）

**同等渲染物料及效果，并行策略×图像驱动（GL D3D FlyCube等），并给出截图及执行性能对比表**

Closed loop: build → run matrix → inspect PNGs → performance table → (optional) optimize against equal-profile budgets → re-run.

Sibling of `harness-auto-map2d-opt` (2D china). This skill owns **plugin.world3d / Scene3D** (+ leftover stereo GL/D3D legs).

## Authorization

When this skill is invoked, attached (`@harness-auto-world3d-opt` / `/harness-auto-world3d-opt`), or followed, the agent **MUST** run the matrix (and rebuild if binaries are missing) and deliver **screenshots + comparison table** — do not defer the harness to the user.

## Hard rules

1. **Equal profile (locked) on FlyCube perf rows:** `--plugin-showcase=world3d` with **`SMT_PLUGIN_WORLD3D_PERF_BARE=1`** (sky/ocean/cloud/fog off, no pointcloud overlay, warmup `pump_ms=0`). Primary metric = **warm `ms_per_present`** (5 frames, discard first cold). Do **not** strip DEM. Product smoke (`null`) keeps full materials.
2. **Image-driven gate:** non-trivial BMP required (except documented `null`). Prefer `*.inspect.png` for `Read`.
3. **Axes (perf):** FlyCube/DX12 · **Leftover/GL** (`gl_leftover`) · **Leftover/D3D11** (`d3d_leftover`); FlyCube prep parallel on/off. **No GDI row** (not a 3D GPU peer). **`null` = smoke-only** — run + gate, never a performance peer.
4. Prefer `build.bat debug src/app/views:views` **and** `build.bat debug SmartGis`. Compile lock **OFF**. Stay on **`master`**.
5. CBM first (`smartgis`). Product default remains **FlyCube/DX12**.
6. Fairness: FlyCube perf rows use DEM-only bare; leftover is china stereo DEM — compare `ms_per_present` / phases, not process wall.

## Entry commands

```bat
.\build.bat debug src/app/views:views
.\build.bat debug SmartGis
py -3 testing/tools/harness/plugin/run_world3d_backend_matrix.py
```

Artifacts: `out/Debug/captures/analysis/world3d_opt/matrix/`

### M4 full-materials (separate table — not bare peers)

Product atmo/ocean/sky + pointcloud (`SMT_PLUGIN_WORLD3D_PERF_BARE` **unset**). Writes under `matrix/full_materials/` with `role=full_materials`. **Never** fold these rows into the bare warm peer ranking.

```bat
py -3 testing/tools/harness/plugin/run_world3d_full_materials_matrix.py
py -3 testing/tools/harness/plugin/run_world3d_full_materials_matrix.py --row flycube
```

Artifacts: `out/Debug/captures/analysis/world3d_opt/matrix/full_materials/` (`MATRIX.md`, csv/json). Primary columns: `ms_per_present` + atmosphere-aligned phases (`ocean_prep_ms`, `record_ms`, `present_swap_ms`).

## Env (benchmark wiring)

| Env | Values | Effect |
| --- | --- | --- |
| `SMT_SCENE3D_ENGINE` | `flycube` / `stereo_gl` / `stereo_d3d` | `apply_scene3d_engine_from_env` — wins over showcase GDI default |
| `SMT_STEREO_API` | `OpenGL` / `Direct3D` | Leftover `smt_stereo_hwnd_create` (also forced by `stereo_gl` / `stereo_d3d`) |
| `SMT_SCENE3D_SHOWCASE_D3D` | `0` / `1` | Leftover D3D alias |
| `SMT_PLUGIN_WORLD3D_GPU` | `0` = Null smoke | Views world3d GPU gate |
| `SMT_PLUGIN_WORLD3D_PERF_BARE` | `1` | Bare peer ranking: DEM-only (no sky/ocean/cloud/fog / pointcloud); `pump_ms=0` |
| `SMT_PLUGIN_WORLD3D_PERF_BARE` | *(unset)* | M4 full-materials sibling only — product materials; **not** a bare peer |
| `SMT_GPUSCENE_PREP_PARALLEL` | `0` / `1` | FlyCube prep parallel |
| `SMT_SCENE3D_FRUSTUM_CULL` | `1` on `prep_par_on` only | Required for honest prep parallel (`prep_cull` no-ops when cull off); `prep_par_off` leaves cull unset/off |

Cold first-present (empty DEM seed/hypso bake cache) still lands ~0.8–1.1s (`tess`/`hypso`); warm ms/p is the peer metric. Do not chase cold ocean unless budgets require it.

## Workflow

```
World3d opt progress:
- [ ] 1. Build SmartGisViews + SmartGis
- [ ] 2. Run run_world3d_backend_matrix.py
- [ ] 3. Confirm *.inspect.png (incl. gl_leftover / d3d_leftover)
- [ ] 4. Print performance comparison table (perf rows only)
- [ ] 5. Read flycube + gl_leftover + d3d_leftover inspect PNGs
- [ ] 6. Optimize mode (if asked): fix → rebuild → re-run
```

Runnable rows: `flycube`, `prep_par_off`, `prep_par_on`, **`gl_leftover`**, **`d3d_leftover`** (perf); `null` (smoke). Do **not** mark GL/D3D as N/A when PE is merely unbuilt. Do **not** add a `gdi` row.

### Performance table

Primary metric: **`ms_per_present`** (from `*-perf.json`). Do **not** rank by process `wall_ms`.

| Row | Backend | Parallel | ms/present | pass | inspect |
| --- | --- | --- | ---: | --- | --- |
| flycube | FlyCube/DX12 | prep_default | | | |
| prep_par_off / prep_par_on | FlyCube/DX12 | prep_0 / on | | | |
| gl_leftover | Leftover/GL | leftover_serial | | | |
| d3d_leftover | Leftover/D3D11 | leftover_serial | | | |

Perf JSON leaves: `plugin-showcase-world3d-perf.json` (FlyCube warm `ms_per_present` + `cold_phase` dem/tess/hypso/upload/pso); `legacy-scene3d-showcase-china-{gl,d3d}-perf.json` (leftover).

### Smoke (not performance)

| Row | Backend | Parallel | note |
| --- | --- | --- | --- |
| null | Null | gpu_off | smoke-only; BMP optional |

### M4 full-materials budgets (Debug, FlyCube only)

Separate from bare peer table. Do **not** compare full-materials `ms_per_present` to bare warm ranks.

| Axis | Guidance |
| --- | --- |
| Entry | `run_world3d_full_materials_matrix.py` (PERF_BARE unset) |
| Rows | `flycube` · `prep_par_off` · `prep_par_on` (`role=full_materials`) |
| Phases | Reuse scene3d-frame-opt fields: `ocean_prep_ms` / `record_ms` / `present_swap_ms` / `rebuild_count` |
| Timed loop | Product non-bare: `pump_ms=50`, `discard_cold=0` (honest product path) |
| null / leftover | Stay on bare matrix only (`null` smoke; `gl_leftover` / `d3d_leftover` naming locked) |

## Related

- Per-frame profile + opt loop: `.cursor/skills/harness-auto-scene3d-frame-opt/SKILL.md`
- Bare runner: `testing/tools/harness/plugin/run_world3d_backend_matrix.py`
- Full-materials runner: `testing/tools/harness/plugin/run_world3d_full_materials_matrix.py`
- Policy: `src/content/browser/present/scene3d/session/scene3d_rhi_session.*`
- Leftover suites: `legacy.scene3d.china` · `legacy.scene3d.china.d3d`
- Detail: [reference.md](reference.md)
