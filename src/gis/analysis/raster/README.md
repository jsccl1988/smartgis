<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `gis/analysis/raster`

Future home for **raster / cost-surface / flood** analysis objects and kernels
(e.g. reserved `native.cost_path`, `native.flood_fill`).

## Rules

- Orchestration may stay in Python plugins; kernels and typed objects land
  **here**, then register through processing / `ops` catalog.
- Do **not** implement core raster analysis under `src/plugin/`.
