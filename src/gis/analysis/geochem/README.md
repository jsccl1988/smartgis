<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `gis/analysis/geochem`

Geochemical sample / anomaly kernels (no product plugin UI). Layered per
analysis kernel convention (model → load/io → kernel → op).

| Layer | Path | Role |
| --- | --- | --- |
| model | `model/` | `GeochemSample` / `GeochemSampleSet`, `element_index`, `values_for_element` |
| load | `load/` | CSV / OGR → sample set |
| stats / idw / grade | `stats/` `idw/` `grade/` | Pure kernels (no JSON, no GeoTIFF write) |
| io | `io/` | GeoTIFF write for IDW (+ optional anomaly mask) |
| op | `op/` | `native.geochem_stats` / `native.geochem_idw` via `ops_args` |

Catalog ids (unchanged): `native.geochem_stats`, `native.geochem_idw`.

CSV columns (header required): `lon`/`lat` (or `x`/`y`) + optional `id` + numeric element columns.

In-memory APIs under `gis::detail`: `load_csv`, `load_vector`, `compute_stats`,
`run_idw`, `build_grade_legend`, `heat_score`, …

Map2d viz (via product `GeochemWriter` / present): graded sample circles over a
padded-bbox IDW heat raster (`geochem_heat_raster`, style interpolate on numeric
`heat` 0..100).
