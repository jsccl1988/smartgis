<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# smartgis.map2d

Product seed for `--map2d-showcase=china|align|orthogrid`. HWND present, FPS
bench, and BMP capture stay in the Views harness (`app/views/harness/showcase/map2d`
session/present/capture). This package owns the **document payload**.

| Id | Role |
| --- | --- |
| `map2d.seed` `{mode}` | `china` — `present_dataset` china_city GPKG/GeoJSON; `align` — same + `style_align.json`; `orthogrid` — `orthogrid.create_orth_grid` on `orthogrid_sample.gridbnd` |

Export frames: `map2d_china` (mainland framing), `map2d_orthogrid` (unit square).
