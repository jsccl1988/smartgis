<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `gis/analysis/geometry`

Geometry analysis kernels (Eigen dense SVD / least squares).

| Id | Role |
| --- | --- |
| `native.fit_line` | 2D least-squares line → GeoJSON LineString |
| `native.fit_plane` | 3D plane `ax+by+cz+d=0` → JSON |
| `native.affine_align` | Paired 2D affine (`source`/`target`) → JSON `m[6]` |

In-memory APIs: `fit_line_2d`, `fit_plane_3d`, `affine_align_2d` in `fit.h`.

## Rules

- Core algorithms live **here** (not under `src/plugin/`).
- `plugin/runtime/processing` only forwards `plugin::` → `gis::detail`.
