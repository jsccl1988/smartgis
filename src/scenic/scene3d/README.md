<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/scenic/scene3d` — Scenic 3D scene (leftover copy)

Peer of [`src/vista/scene`](../vista/scene/) (GpuScene). SmartGIS leftover 3D scene
library (`Scene`, objects, octree). Product World / GpuScene path is separate
(`vista::World` / `vista::GpuScene`).

**As-built layout:** `host/` `scene/` `primitive/{mesh,feature,surface}/` `test/`.  
Device-free DEM / World adapters: `//src/vista/world` → **`vista.dll`**.  
Device-free feature tess: `scene3d/primitive/feature/tess_{map,world}.cc`.  
Aggregate GN: `//src/scenic/scene3d:scene3d_sources` → `//src/scenic:scenic_impl`.  
Includes: `scenic/scene3d/<module>/…` (e.g. `primitive/feature/geo_object.h`,
`host/stereo_hwnd_view.h`, `scene/d3d_deferred_objects.h`).  
Scene object list: `scene/octree.*` (flat AABB frustum cull; no per-point index).  
`scenic_copy` TUs must not `#include "legacy/…"`.

## Layout vs vista/scene

| Scenic `scene3d/` | Vista `scene/` |
| --- | --- |
| `host/` (stereo HWND) | (HWND glue; not a GpuScene type) |
| `scene/` (Scene, map attach, octree) | `scene.h` / `scene_draw` / sync |
| `primitive/feature/` (geo objects + OGR tess) | `detail/` (draw_pass, paint, upload…) |
| `primitive/surface/` (terrain, pointcloud) | (`vista/terrain`, `vista/assets/pointcloud`, `vista/world`) |

## Namespace

`scenic::detail` (scene types + tess). Directory does not create `scenic::scene3d`.
