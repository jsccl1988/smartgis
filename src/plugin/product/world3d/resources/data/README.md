<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# world3d optional data (stub)

GN copies UI markup to `out/plugins/world3d/`. Place optional Earth datasets
next to that tree (or under shared `out/data/`):

| File | Role |
| --- | --- |
| `global_dem.tif` | Global / regional DEM GeoTIFF for `world3d.load_global_dem` |
| `satellite_cloud.tif` | Single-band cloud cover for `world3d.set_satellite_cloud` |

Until these files exist, the Browser writers fall back to China sample DEM
and procedural atmosphere clouds (see package README).
