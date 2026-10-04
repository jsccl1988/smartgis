<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# GIS test matrix (`src/gis`)

Functional coverage against industry-shaped surfaces (GDAL / OGR / GEOS / PROJ / QGIS-like product APIs). Compiler coverage: `testing/coverage/gis_coverage.ps1`.

**Living decision:** [`specs/2026-09-13-gdal-layer-management-design.md`](specs/2026-09-13-gdal-layer-management-design.md) §GIS coverage + performance benchmarks.

Legend: **Y** = covered by automated test · **P** = partial · **N** = gap · **B** = has QPC benchmark.

## Kernel

| Capability (industry ref) | Product surface | Test | Bench |
| --- | --- | --- | --- |
| Geometry buffer (GEOS/OGR) | `geo::buffer` | Y `ops_test` | B `buffer_benchmark` |
| Geometry predicates (Intersects) | OGR via traits | Y `ops_test` | N |
| CRS transform EPSG (PROJ) | `geo::CoordinateTransform` / `geo::transform_xy` | Y `proj_test` | B `proj_benchmark` |
| Stat expression | `gis/stat` | Y `stat_expr_test` | N |
| TIN Delaunay / XYZ | `gis/geo/tin` | Y `tin_delaunay_test` / `indexed_tin_test` | N |

## Datasource

| Capability | Product surface | Test | Bench |
| --- | --- | --- | --- |
| Session / ConnectionSpec | `DataSession` | Y `datasource_session_test` | B `datasource_benchmark` |
| GDAL open path | `sde_gdal` | Y `sde_gdal_test` | N |
| SDBD client / live | `SdbdClient` | Y `sdbd_*_test` | N |
| OGR text encoding | ogr text | Y `ogr_text_encoding_test` | N |
| Feature load pipeline | pipeline | Y `feature_load_pipeline_test` | N |

## Model / present

| Capability | Product surface | Test | Bench |
| --- | --- | --- | --- |
| Feature composition | `gis::Feature` | Y `feature_test` | N |
| Select / query | map select | Y `select_query_test` | N |
| Edit conflict | edit session | Y `edit_conflict_test` | N |
| Style document | present/style | Y `style_test` | N |
| Tile provider | present/tile | Y `tile_test` | N |

## Vista

Vista tests live under `//src/vista:vista_test_all` (not `gis_test_all`).

| Capability | Product surface | Test | Bench |
| --- | --- | --- | --- |
| Map frame | vista/frame | Y `frame_test` | N |
| MVT → MapFrame | vista/frame/mvt_layout | Y `tile_test` (deps vista.dll) | N |
| World / DEM / land mask | vista/world | Y `world_*` / dem / land_mask | N |
| Assets model / tileset | vista/assets | Y `model_test` / `tileset_test` | N |
| Atmosphere field / systems | domain/atmosphere | Y field/procedural/ingest + cloud/ocean/env | N |
| Map GPU pass | vista/map | Y `map_effect_test` | N |
| GpuScene | vista/scene | Y `scene_gpu_test` / `unified_draw_test` | N |
| Atmosphere GPU | vista/atmosphere | Y cloud/ocean/sky/fog/globe/frame | N |

## Entry

| Entry | Command |
| --- | --- |
| Unit tests | `build.bat debug te` → `//src/gis:gis_test_all` + `//src/vista:vista_test_all` |
| Benchmarks | `build.bat debug b` → `//src/gis:gis_benchmark_all` |
| Coverage | `powershell -File testing/coverage/gis_coverage.ps1` |
| Console | `:gis test` / `:gis bench` (DebugAgent) |

**最后更新:** 2026-10-04
