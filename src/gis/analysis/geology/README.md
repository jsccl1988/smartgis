<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `gis/analysis/geology`

Geology / stratum analysis kernels (no product plugin UI). Layered as
**model → io → kernel → op**. Pure XY hull/area lives in `gis/geo/xy`
(`geo::convex_hull_xy` / `polygon_area_xy`); TIN triangulation uses
`geo::delaunay_triangles`.

| Id | Role |
| --- | --- |
| `native.stratum_interpolate` | Borehole CSV → Delaunay TIN for one `stratum_id` |
| `native.stratum_prism_volume` | Average top/bottom thickness × hull/bbox XY area |

CSV columns (header required): `hole_id,x,y,z,stratum_id`.

| Path | API |
| --- | --- |
| `model/borehole.h` | `BoreholeContact` / `BoreholeSet` |
| `io/borehole_csv.h` | `load_boreholes_csv` |
| `kernel/stratum_tin.h` | `interpolate_stratum_tin` |
| `kernel/prism_volume.h` | `prism_volume_between` |
| `op/stratum_ops.h` | `run_stratum_interpolate_op` / `run_stratum_prism_volume_op` |

Diagram: [`docs/superpowers/diagrams/gis-analysis-geology.html`](../../../../docs/superpowers/diagrams/gis-analysis-geology.html).
