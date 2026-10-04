<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `gis/analysis/raster`

Raster / flood analysis kernels, split by responsibility:

| Subdir | Id | Role |
| --- | --- | --- |
| `dem/` | `native.flood_fill` | DEM inundation (connected cells under a water surface) |
| `dem/` | `native.storm_surge` | Coastal storm-surge (ocean/coast seeds + surge series → mask/depth) |
| `dem/` | `native.storm_surge_stats` | Inundation area, depth classes, buffer/overlap vs impact layer |
| `dem/` | `native.dem_gradient` | DEM slope/aspect via Eigen `Map` finite differences |
| `dem/` | `horn_lambert_shade` | Horn Lambert factor on a float grid (no RGBA, no plugin op) |
| `mask/` | `fill_ring_mask` | Even-odd cell mask for lon/lat rings (no CUDA, no plugin op) |
| `filter/` | `native.raster_convolve` | Float raster convolution (default 3×3 box) |
| `filter/` | `native.raster_smooth` | Interior Laplace smooth via Eigen SparseLU |

Includes: `gis/analysis/raster/dem/…`, `gis/analysis/raster/mask/…`, `gis/analysis/raster/filter/…`.
`horn_lambert_shade_grid` and `fill_ring_mask` are the numeric kernels behind vista terrain bake. RGBA contrast, `DemRaster`, and Thrust stay in `vista/terrain`. Orchestration for `native.*` stays in product plugins / Python.
