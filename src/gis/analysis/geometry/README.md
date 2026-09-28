<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `gis/analysis/geometry`

Future home for **geometry analysis objects** (typed operators, options, result
handles) that are not the file-level `ops/` runner.

## Rules

- New core geometry analysis types and algorithms land **here** (or under
  `gis/kernel/geo` when they are low-level GEOS/OGR helpers).
- Do **not** put core algorithms or domain objects under `src/plugin/`.
- `plugin/runtime/processing` only forwards public `plugin::` entry points to
  `gis::detail` (see `ops/`).
