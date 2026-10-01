<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `gis/analysis/geology`

Geology / stratum analysis kernels (no product plugin UI).

| Id | Role |
| --- | --- |
| `native.stratum_interpolate` | Borehole CSV → Delaunay TIN for one `stratum_id` |
| `native.stratum_prism_volume` | Average top/bottom thickness × hull/bbox XY area |

CSV columns (header required): `hole_id,x,y,z,stratum_id`.

In-memory APIs: `load_boreholes_csv`, `interpolate_stratum_tin`,
`prism_volume_between` in `borehole.h` / `stratum_tin.h` / `prism_volume.h`.
