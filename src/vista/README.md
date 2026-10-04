<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/vista` — viewport vista layer (`vista.dll`)

CPU map frame / logical World **and** GPU map / GpuScene / atmosphere passes live in this tree. One product DLL: GN `//src/vista:vista`, `dll_stem=vista` (`vista.dll` / `vista_d.dll`).

The previous-generation engine is **Scenic** (`src/scenic` / `scenic.dll`, hosted by `src/content`). Leftover `src/legacy/render` is **frozen** until the cut completes. Scenic is **not** compiled into `vista.dll`. Leftover adapters under `legacy/gis/vista` are **not** Scenic.

Living layout lock: [`docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md`](../../docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md) **§Vista subdirectory tighten** · **§Vista map/frame scenic-peer**. Diagrams: [`vista-subdirectory-layers.html`](../../docs/superpowers/diagrams/vista-subdirectory-layers.html) · [`vista-map-frame-scenic-peer.html`](../../docs/superpowers/diagrams/vista-map-frame-scenic-peer.html). Layer table: [`docs/superpowers/src-layout.md`](../../docs/superpowers/src-layout.md).

## Owns / does not own

| Owns | Does not own |
| --- | --- |
| `vista::Layout` → `MapFrame` / `DrawItem` | `gis::style` / `gis::tile` / OGR open (those stay `gis.dll`) |
| `vista::World` node graph; DEM domain; CPU mesh; point-cloud codecs | HWND, Views chrome, `ui/gfx` widgets |
| CPU `vista::atmosphere` session (`atmosphere/session/`) | FlyCube types in public headers |
| GPU `vista::FramePass`, `GpuScene`, `AtmosphereFrame` | Frame-graph vtable (`render/graph/frame_graph.h`) |
| Leftover adapters compiled **in** (`legacy/gis/vista`) | `#include "legacy/…"` from product TUs here |

`gis.dll` must not depend on this DLL (`assert_no_deps`). Product TUs must not include leftover. Leftover → product is allowed.

## Modules

| Dir | Role | Include |
| --- | --- | --- |
| `map/` | CPU Layout → MapFrame (place has no RHI) | `"vista/map/frame.h"` umbrella; types in `view.h` / `draw.h` / `batch.h` / `layout.h` |
| `map/` internals | `carto_filter`, `collision`, `place` | `"vista/map/collision.h"` |
| `map/layout/` | collect / emit / coalesce + per-geom emit | `"vista/map/layout/fill.h"` |
| `world/` | Logical node graph + `dem_seed` | `"vista/world/world.h"` |
| `world/pointcloud/` | Chunk / LOD buckets on a node | `"vista/world/pointcloud/chunk.h"` |
| `assets/` | Model, 3D Tiles, point-cloud file codecs | `"vista/assets/model/model.h"` |
| `assets/pointcloud/` | `PointCloud` + LAS / LAZ / PDAL / text | `"vista/assets/pointcloud/point_cloud.h"` |
| `mesh/` | CPU xyz+indices from GIS geometry | `"vista/mesh/tessellate.h"` |
| `terrain/` | DEM raster, hillshade bake, land mask (no `World` in headers). Horn shade and even-odd mask call `gis/analysis` | `"vista/terrain/dem/dem_raster.h"` |
| `domain/` | `DomainSession` seam | `"vista/domain/domain.h"` |
| `atmosphere/session/` | CPU FieldStore / Environment | `"vista/atmosphere/session/environment.h"` |
| `frame/` | GPU FramePass recorder (upload / encode) | `"vista/frame/pass.h"` |
| `scene/` | GpuScene facade + CPU IR / tess / upload / record | `"vista/scene/scene.h"` |
| `scene/cull/` | frustum POD + prep_cull | `"vista/scene/cull/prep_cull.h"` |
| `scene/index/` | unibn AABB octree | `"vista/scene/index/aabb_octree.h"` |
| `atmosphere/` | GPU pass order + ocean/cloud/sky/fog/globe | `"vista/atmosphere/frame/atmosphere_frame.h"` |

Headers sit next to their `.cc`. No `gis/vista/` or `effect/` trees or forwarding headers.

## Namespaces

Public C++ stays two levels: `vista` (CPU frame / World / GPU) and `vista::atmosphere` (CPU domain). Internals: `vista::detail` or an anonymous namespace. `gis::style` / `gis::tile` / leftover `gis::Smt*` stay in `gis.dll`. Do not add `vista::frame` as a public third layer.

## GN

- DLL: `//src/vista:vista`
- Tests: `//src/vista:vista_test_all`
- Per-module `*_sources` compile into the DLL. CPU sets must not grow a `//src/render:render` dep (`assert_no_deps`). `assets` / `mesh` / `terrain` do not depend on `world`. `world` depends on `assets` + `terrain`. `map` depends on `mesh` + `terrain`. GPU sets may depend on `render` + CPU sets they consume (`frame` → `map`, `scene_sources` → `scene_cpu_sources` + `world` + `mesh`). `map_sources` / `scene_cpu_sources` `assert_no_deps` `//src/render:render`. `//src/vista/atmosphere:atmosphere_sources` depends on `atmosphere_cpu_sources` and must not depend on `session_sources`.
