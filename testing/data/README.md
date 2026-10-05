<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `testing/data` sample packs

These are **not** one dataset. GN still **flattens** product copies into shared
`out/data/` (`../data/` from `out/Debug|Release`) so runtime seed paths stay
stable.

| Tree | What it is | Same as china pack? |
| --- | --- | --- |
| [`china/`](china/) | Aligned product China pack: NE 10m vectors + Mapzen DEM, EPSG:4326 / CRS84. `china_plp.geojson` is a **schematic fallback**, not the prefecture GPKG. `pointcloud_public_sample.txt` is **derived** from `china_dem.tif`. | This **is** the china pack |
| [`plugin/`](plugin/) | Product plugin smoke fixtures (tiny DEM, schematic flood/stormsurge, LAS, CSV). | **No** — `world3d_dem_sample.tif` is an 8×8 stub, not `china_dem` |
| [`fixtures/`](fixtures/) | Unrelated unit fixtures: OGR smoke GeoJSON, city 3D Tiles JSON, MVT PBF | **No** |
| [`rs/terrain/`](rs/terrain/) | Leftover AM heightmap / TIN (`ground.bmp` / `ground.dat`) | **No** |
| `build_*.py` | Generators (stay at this root). Scripts write into the trees above. | — |
| `out/data/global_dem.tif` | Optional globe splash from `build_globe_terrain.py` (not copied by GN) | **No** — may **blend** china DEM into a China AOI |

Rebuild china pack:

```
py -3 testing/data/build_china_city.py --with-dem
```

PIN / license: `china/china_city.PIN.txt`, `china/china_city.LICENSE.txt`.
