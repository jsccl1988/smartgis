<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# 3D DEM align SmartGis.exe (A+B, DemRaster authority)

**Status:** accepted  
**Date:** 2026-09-19  
**Supersedes prior scope:** hosts-only unify; leftover deferred 2a / cancelled 2b.

## Goal

Product hosts and leftover `SmartGis.exe` share one DEM authority and match leftover 3D framing:

1. **A — Host parity:** Views / WinUI / CEF / Cs 3D DEM look and default orbit match `SmartGis.exe` (`leftover_frame_pose` 上北下南, cutline, exaggeration).
2. **B — Leftover 2a:** Keep GL `StereoTerrain` / `SmtScene` shell; terrain heights from `gis::DemRaster`; **per-scene** seed; **no** process-global `g_scene_dem` short-circuit that skips a second view’s seed.

## Approach (locked)

**DemRaster is authority.** Leftover `DemHeightField` is a thin export shell (ABI names kept) that loads/forwards from `gis::DemRaster` (or is filled from it once at seed). `Scene3dController` already seeds via `seed_china_dem_into_world`. Shared framing constants live under `gis/` so Views never `#include` leftover.

```
gis::DemRaster  (+ shared frame constants / cutline policy)
        │
        ├─► Scene3dController → World → GpuScene / GDI paint
        │
        └─► DemHeightField (thin) → StereoTerrain + drape / labels
              owned per seed into SmtScene (not a sticky global skip)
```

## Scope

| In | Out |
| --- | --- |
| `src/gis/world/**` DemRaster cutline + optional frame helpers | Leftover **2b** (controller replaces GL draw) — cancelled |
| `src/app/views/**` (and CEF/Cs only if yaw/cutline API reuse needs it) | SP2 `src/legacy/render/bridge/**` present |
| `src/legacy/render/scene3d/**` 2a seed / DemHeightField shell | Atmosphere/ocean port into leftover GL |
| Tests: `dem_raster_test`, `scene3d_controller_test`, `dem_stereo_test` | Force-commit |

## Locked decisions

| Topic | Choice |
| --- | --- |
| Authority | `gis::DemRaster` for load / synthetic China / `fit_vertical_exaggeration` / china_dem cutline skip |
| Leftover draw | Keep `StereoTerrain` + GL; fill from DemRaster-backed height field |
| Global DEM | Remove “already have `g_scene_dem` → skip seed for this scene” behavior; each `seed_*_into_scene` must attach terrain to **that** `SmtScene` |
| `leftover_has_scene_dem` | Reflect whether the **relevant** scene/world has DEM (not a sticky process flag that blocks re-seed) |
| Host camera | Default orbit yaw = south-of-target (`π − 0.55`), shared constant with leftover framing intent |
| Mesh X | `X=-lon` (not `+lon`): RH lookAt looking north has camera-right=`-X`, so east sits on screen-right |
| Cutline | Real `china_dem*` path → do **not** remask with prefecture rings (match `seed_stereo_underlay`); synthetic → optional rings with mainland-contains guard |
| Views ↔ leftover | Views must not include `legacy/…` |
| ABI | Keep `DemHeightField` / `SCENE3D_EXPORT_*` / `leftover_frame_pose` / `seed_sample_map_into_scene` names |

## Components

### gis

- Single cutline policy used by `seed_china_dem_into_world` (parity with leftover).
- Optional small `dem_frame` / constants header: default orbit yaw (and comment tying to `leftover_frame_pose`).

### Hosts (`Scene3dController`)

- Default `yaw_` from shared constant; mesh +Z = north; drop temporary `present_gpu` stderr bisect marks when done.
- Mesh / exaggeration via existing DemRaster path; max_edge may stay 96 unless leftover mesh density must match for A (document if intentionally lower for GPU).

### Leftover (`map_to_scene` / `DemHeightField`)

- Load path: DemRaster → fill `DemHeightField` (or member forwarder).
- `seed_stereo_underlay(device, scene, …)` always adds `StereoTerrain` for **this** scene from a height field tied to that seed (heap/scene-owned), not “global already filled → return without adding”.
- Drape / labels keep using `DemHeightField::sample`.
- SP4 `seed_dem_height_field_into_world` / `map_seeded_world` remain; prefer DemRaster seed where one call can serve both.

## Data flow

1. Resolve `find_sample_dem_path()` (gis or leftover forward to gis).
2. `DemRaster::load_gdal_raster` or `fill_synthetic_china` + `fit_vertical_exaggeration`.
3. Apply cutline policy.
4. Hosts: `seed_dem_raster_into_world` → normalize for orbit → GpuScene / GDI.
5. Leftover: copy/adapt into `DemHeightField` → `StereoTerrain::Init/Create` → scene Add3DObject; labels drape on same field.

## Error handling

- Missing DEM file → synthetic China (both paths).
- Failed `StereoTerrain::Create` → no terrain object; seed may still return labels / world envelope if partial.
- Empty mesh → hosts `present_gpu` / `paint` no-op safely (existing).

## Tests

- `dem_raster_test`: lon/lat axes 上北下南; Tibet higher than Jiangsu; china_dem cutline skip when path matches.
- `scene3d_controller_test`: default yaw ≈ shared constant; seeded paint / present_gpu.
- `dem_stereo_test`: second seed into a fresh `SmtScene` still gets terrain (no global short-circuit); DemHeightField still builds mesh after DemRaster-backed load.
- Build: related ninja targets / `build.bat` slices green; **do not commit** unless user asks.

## Risks

- `DemHeightField` private layout change is OK inside scene3d DLL consumers; do not require external sizeof ABI.
- Dual find_sample_dem_path (gis vs leftover) — consolidate or forward to avoid drift.
- Multi-viewport leftover: scene-owned height field lifetime must outlive `StereoTerrain` (terrain holds const pointer).

## Related

- SP4: `docs/superpowers/specs/2026-09-19-scene3d-world-gpuscene-design.md`
- Plan: `docs/superpowers/plans/2026-09-19-leftover-scene3d-dem-unify.md`
- `leftover_frame_pose`: `src/legacy/render/scene3d/map_to_scene.cc`
