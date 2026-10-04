<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/vista` — viewport vista layer (`vista.dll`)

CPU IR (`map` `MapIR`, `world` `World` + `Instance`) and GPU passes (`map_gpu` `MapPass`, `world_gpu` `WorldPass`, atmosphere) live in this tree. One product DLL: GN `//src/vista:vista`, `dll_stem=vista` (`vista.dll` / `vista_d.dll`).

The previous-generation engine is **Scenic** (`src/scenic` / `scenic.dll`, hosted by `src/content`). Leftover `src/legacy/render` is **frozen** until the cut completes. Scenic is **not** compiled into `vista.dll`. Leftover adapters under `legacy/gis/vista` are **not** Scenic.

Living layout lock: [`docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md`](../../docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md) **§Vista IR/GPU lanes**. Diagram: [`vista-subdirectory-layers.html`](../../docs/superpowers/diagrams/vista-subdirectory-layers.html). Layer table: [`docs/superpowers/src-layout.md`](../../docs/superpowers/src-layout.md).

Target names below are authoritative even while sources are mid-move. `frame/` and `scene/` are not part of this layout.

## Owns / does not own

| Owns | Does not own |
| --- | --- |
| `vista::Layout` → `MapIR` / `DrawItem` | `gis::style` / `gis::tile` / OGR open (those stay `gis.dll`) |
| `vista::World` node graph; CPU `Instance`, sync, tess, cull, index; DEM domain; CPU mesh; point-cloud codecs | HWND, Views chrome, `ui/gfx` widgets |
| CPU `vista::atmosphere` session (`atmosphere/session/`) | FlyCube types in public headers |
| GPU `vista::MapPass`, `WorldPass`, `AtmosphereFrame` | Frame-graph vtable (`render/graph/frame_graph.h`) |
| Leftover adapters compiled **in** (`legacy/gis/vista`) | `#include "legacy/…"` from product TUs here |

`gis.dll` must not depend on this DLL (`assert_no_deps`). Product TUs must not include leftover. Leftover → product is allowed. `sync_from` and `record_draws` stay methods on `WorldPass`.

## Modules

| Dir | Role | Include |
| --- | --- | --- |
| `map/` | CPU IR: `Layout` → `MapIR` (place has no RHI) | `"vista/map/ir.h"` umbrella; types in `view.h` / `draw.h` / `batch.h` / `layout.h` |
| `map/` internals | `carto_filter`, `collision`, `place` | `"vista/map/collision.h"` |
| `map/layout/` | collect / emit / coalesce + per-geom emit | `"vista/map/layout/fill.h"` |
| `map_gpu/` | GPU `MapPass` (upload / encode / record) | `"vista/map_gpu/pass.h"` |
| `world/` | `World` node graph + `dem_seed`; CPU `Instance`, sync, tess, cull, index | `"vista/world/world.h"` |
| `world/cull/` | frustum POD + prep_cull (not `frustum_camera`) | `"vista/world/cull/prep_cull.h"` |
| `world/index/` | unibn AABB octree | `"vista/world/index/aabb_octree.h"` |
| `world/pointcloud/` | Chunk / LOD buckets on a node | `"vista/world/pointcloud/chunk.h"` |
| `world_gpu/` | GPU `WorldPass` (`sync_from`, `record_draws`); `GpuMesh` device buffers; `cull/frustum_camera` | `"vista/world_gpu/pass.h"` |
| `assets/` | Model, 3D Tiles, point-cloud file codecs | `"vista/assets/model/model.h"` |
| `assets/pointcloud/` | `PointCloud` + LAS / LAZ / PDAL / text | `"vista/assets/pointcloud/point_cloud.h"` |
| `mesh/` | CPU xyz+indices from GIS geometry | `"vista/mesh/tessellate.h"` |
| `terrain/` | DEM raster, hillshade bake, land mask (no `World` in headers). Horn shade and even-odd mask call `gis/analysis` | `"vista/terrain/dem/dem_raster.h"` |
| `domain/` | `DomainSession` seam | `"vista/domain/domain.h"` |
| `atmosphere/session/` | CPU FieldStore / Environment | `"vista/atmosphere/session/environment.h"` |
| `atmosphere/` | GPU pass order + ocean/cloud/sky/fog/globe | `"vista/atmosphere/frame/atmosphere_frame.h"` |

No `map/gpu` or `world/gpu` nested directories. Headers sit next to their `.cc`. No `gis/vista/` or `effect/` trees or forwarding headers.

## Namespaces

Public C++ stays two levels: `vista` (CPU IR / World / GPU passes) and `vista::atmosphere`. Internals: `vista::detail` or an anonymous namespace. Do not add `vista::map_gpu` or any other public third layer. `gis::style` / `gis::tile` / leftover `gis::Smt*` stay in `gis.dll`.

## GN

- DLL: `//src/vista:vista`
- Tests: `//src/vista:vista_test_all`
- Per-module `*_sources` compile into the DLL. CPU sets must not grow a `//src/render:render` dep (`assert_no_deps`). `assets` / `mesh` / `terrain` do not depend on `world`. `world_sources` depends on `assets` + `terrain` + `mesh`, and `assert_no_deps` `//src/render:render`. `map_sources` depends on `mesh` + `terrain` and `assert_no_deps` `//src/render:render`.
- GPU sets may depend on `render` plus the CPU set they consume: `map_gpu_sources` → `map_sources` + `render`; `world_gpu_sources` → `world_sources` + `render`.
- `//src/vista/atmosphere:atmosphere_sources` depends on `atmosphere_cpu_sources` and must not depend on `session_sources`.
- Env strings `SMT_GPUSCENE_PREP_PARALLEL` and `SMT_VISTA_LAYOUT_PARALLEL` stay. They drive `WorldPass` prep and `Layout` emit.
- Test executable `output_name` values stay (`map2d_pass_test`, `scene_gpu_test`, `unified_draw_test`). `content` `present/map2d` and `present/scene3d` directory names stay.
