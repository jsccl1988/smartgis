<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/vista/terrain/dem`

DEM domain for Vista: load / sample / mesh / albedo bake / hillshade / land
mask / isolines / disk caches. No `World` / RHI types here. Seed into World
lives in `vista/component/world/terrain`.

Living lock: [`docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md`](../../../../docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md)
(DEM / hillshade / globe albedo rows).

## Layers

| Path | Owns |
| --- | --- |
| `raster/` | `DemRaster` public header + load / grid / mesh / hypso / map drape / sample paths |
| `shade/` | `shade_dem_rgba` + AVX2 lit / Lambert pack (`lit_*.cc`) |
| `mask/` | Lon/lat land mask (CPU + Thrust) |
| `bake/` | `BakeBackend` / parallel bake helpers (header-only) |
| `nv/` | Thrust GIS kernels + shared `bake_pixel.h` |
| `cache/` | Mem/disk bake caches (`dem_bake_cache.h` umbrella at dem root) |
| `contour/` | Isoline msquares → paint / mesh (`dem_contour.h` at dem root) |
| `height/` | Leftover `render::DemHeightField` shell over `DemRaster` |

## Public includes

```cpp
#include "vista/terrain/dem/raster/dem_raster.h"
#include "vista/terrain/dem/shade/dem_hillshade.h"
#include "vista/terrain/dem/mask/land_mask.h"
#include "vista/terrain/dem/dem_contour.h"
#include "vista/terrain/dem/dem_bake_cache.h"
#include "vista/terrain/dem/height/dem_height_field.h"
```

## Albedo

1. Prefer `DemRaster::bake_map_drape_rgba` (`china_rs` / orthophoto + Horn lit).
2. Else `bake_hypsometric_rgba` + elevation overlay (jet / isolines).

Terrain seed uses `detail::apply_terrain_albedo_texture`.

## SIMD + `base::execution`

AVX2 TUs (`shade/lit_avx2.cc`, `detail/dem_simd_avx2.cc`) use `immintrin.h`
(equal-profile showed vir-simd slower on gather-heavy DEM packs) and compile
with `/arch:AVX2`. Runtime CPUID in `lit_dispatch.cc` / `dem_simd.cc` skips
YMM when unavailable.
Row walks use `base::execution::parallel_for` + `GlobalNThreadPoolExecutor`
via `for_each_bake_row` when `bake_rows_should_parallel` holds.

Hot paths:

- LOD bilinear: `detail::fill_lod_bilinear_grid` (parallel rows + AVX2 lerp)
- Hillshade CPU: `pack_lambert_from_shade` over Horn shade grid
- Map drape: `apply_rgb_lit_mul` after orthophoto sample
- Range / downsample: `minmax_f32` + parallel nearest downsample

Equal-profile overrides (env or `set_bake_*_override`):

| Var | Effect |
| --- | --- |
| `BAKE_BACKEND` | `auto` / `cpu` / `cuda` |
| `BAKE_PARALLEL=0` | force serial row walks (also honored by gis Horn grid) |
| `BAKE_SIMD=0` | force scalar lit / LOD / minmax |

CUDA Thrust remains the preferred path when `BAKE_BACKEND` allows it.

## Benchmark matrix

```bat
build.bat debug //src/vista/terrain:dem_kernel_benchmark
out\Debug\dem_kernel_benchmark.exe
REM or:
python testing/tools/harness/browser/run_terrain_kernel_matrix.py
```

Datasets × schemes × kernels →
`out/<config>/captures/analysis/terrain_kernel/matrix.{json,md}`.

| Axis | Values |
| --- | --- |
| Dataset | `synth_{256,512,1024}`, `china_e{256,512,768}` |
| Scheme | `serial_scalar`, `parallel_scalar`, `parallel_simd`, `cuda` |
| Kernel | `shade_dem_rgba`, `build_mesh`, `bake_hypsometric`, `fill_lod_bilinear`, `pack_lambert` |

## GN

`//src/vista/terrain:terrain_sources` → `vista.dll`. Tests:
`land_mask_test`, `dem_raster_test`, `dem_kernel_benchmark`.

---

**最后更新：** 2026-10-07
