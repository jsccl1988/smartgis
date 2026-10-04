<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/scenic/scene3d` — Scenic 3D scene (leftover copy)

Peer of [`src/vista/scene`](../vista/scene/) (GpuScene). SmartGIS leftover 3D scene
library (`Scene`, objects, octree). Product World / GpuScene path is separate
(`vista::World` / `vista::GpuScene`).

**As-built layout:** `scene/` `index/` `primitive/{mesh,feature,surface}/` `seed/`
`detail/` `test/`.  
Device-free DEM / World adapters: `//src/vista/world` → **`vista.dll`**.  
Device-free feature tess: `scene3d/detail/tess_{map,world}.cc` (no leftover).  
Aggregate GN: `//src/scenic/scene3d:scene3d_sources` → `//src/scenic:scenic_impl`.  
Includes: `scenic/scene3d/<module>/…` (e.g. `primitive/feature/geo_object.h`,
`scene/stereo_hwnd_view.h`, `detail/d3d_deferred_objects.h`).  
Spatial index: `//third_party:octree` (jbehley/unibn).  
`scenic_copy` TUs must not `#include "legacy/…"`.

## Layout vs vista/scene

| Scenic `scene3d/` | Vista `scene/` |
| --- | --- |
| `scene/` (Scene, stereo HWND) | `scene.h` / `scene_draw` / sync |
| `detail/` (deferred D3D + OGR tess) | `detail/` (draw_pass, paint, upload…) |
| `primitive/surface/` (terrain, pointcloud) | (logic in `vista/world/terrain|pointcloud`) |
| `index/` / `seed/` | frustum / rebuild helpers |

## Namespace

`scenic::detail` (scene types + tess). Directory does not create `scenic::scene3d`.
