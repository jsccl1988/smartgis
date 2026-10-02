# Smt3DBaseLib

<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

**As-built layout (2026-10-02):** `scene/` `index/` `primitive/{mesh,feature,surface}/` `seed/` `test/`.  
Device-free DEM / World adapters: `//src/legacy/gis/vista` → **`gis.dll`**.  
Device-free feature tess: `//src/legacy/gis/feature` → **`gis.dll`**.  
Aggregate GN: `//src/legacy/render/scene3d:scene3d_sources` → `//src/legacy/render:legacy_render`.  
Includes: `legacy/render/scene3d/<module>/…` (e.g. `primitive/feature/geo_object.h`, `scene/stereo_hwnd_view.h`).  
Spatial index: `//third_party:octree` (jbehley/unibn).

SmartGIS leftover 3D scene library (`SmtScene`, objects, octree). Product World / GpuScene path is separate (`gis::World`).

## Layout

| Dir | Contents |
| --- | --- |
| `scene/` | `SmtScene`, `Smt3DObject`, `vertex3d`, `stereo_hwnd_view` (HWND C ABI), deferred D3D helper |
| `index/` | scene / vertex octree (SP4b) |
| `primitive/mesh/` | cube / sphere / water / northarray |
| `primitive/feature/` | `SmtGeoObject`, `MapLabelBatch` |
| `primitive/surface/` | `SmtSurfaceObject` base · `SmtTerrain` · pointcloud |
| `seed/` | map→scene seeding + `SmtScene` AABB→World shell |
| `test/` | `dem_stereo_test`, `pointcloud_load_test` |

## Namespace

`Smt_3DBase` / `render` (seed helpers).
