<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `gis/analysis/geochem`

Geochemical sample / anomaly kernels (no product plugin UI).

| Id | Role |
| --- | --- |
| `native.geochem_stats` | CSV → histogram, background/threshold, optional Pearson `r` |
| `native.geochem_idw` | CSV → IDW Float32 full-extent heat surface (+ optional Byte anomaly mask) |

CSV columns (header required): `lon`/`lat` (or `x`/`y`) + optional `id` + numeric element columns.

In-memory APIs: `load_geochem_csv`, `load_geochem_vector`, `compute_geochem_stats`,
`run_geochem_idw`, `build_geochem_grade_legend`, `geochem_heat_score` under `gis::detail`.

Map2d viz (via `GeochemWriter`): graded sample circles over a padded-bbox IDW heat
raster (`geochem_heat_raster`, style interpolate on numeric `heat` 0..100).