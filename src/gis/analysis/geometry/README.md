<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `gis/analysis/geometry`

Geometry analysis kernels (Eigen dense SVD / least squares).
GEOS set/shape/predicate ops live under `gis/geo/ops` + thin `analysis/ops` IR — **not** here.

| Id | Role |
| --- | --- |
| `native.fit_line` | 2D least-squares line → GeoJSON LineString |
| `native.fit_plane` | 3D plane `ax+by+cz+d=0` → JSON |
| `native.affine_align` | Paired 2D full affine (`source`/`target`) → JSON `m[6]` |
| `native.similarity_align` | Umeyama similarity (scale+rotation+translation) → JSON |
| `native.rigid_align` | Umeyama rigid (unit scale) → JSON |
| `native.pca_axes` | PCA principal axes / extents → JSON |
| `native.oriented_bbox` | Oriented rectangle (PCA points or GEOS MRR) → GeoJSON |
| `native.translate` | Constructive translate (`dx`, `dy`) → GeoJSON (IR) |
| `native.rotate` | Constructive rotate (`angle` rad, `origin_x/y`) → GeoJSON (IR) |
| `native.scale` | Constructive scale (`scale_x/y`, origin) → GeoJSON (IR) |

In-memory APIs: `fit_line_2d`, `fit_plane_3d`, `affine_align_2d`, `similarity_align_2d`,
`rigid_align_2d`, `pca_axes_2d`, `oriented_bbox_from_points_2d`, and `affine_*_2d` in
`fit.h` / `affine_construct.h`.

## Rules

- Core Eigen fit/transform algorithms live **here** (not under `src/plugin/`).
- `plugin/runtime/processing` only forwards `plugin::` → `gis::detail`.
- Industry gap / next ops:  
  [`docs/superpowers/plans/2026-10-25-geometry-analysis-gap-pin.md`](../../../../docs/superpowers/plans/2026-10-25-geometry-analysis-gap-pin.md) **P1-B** · living [`algorithm-layer-oss` §9](../../../../docs/superpowers/specs/2026-09-13-algorithm-layer-oss-design.md).
