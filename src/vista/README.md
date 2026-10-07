<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/vista` �?viewport vista layer (`vista.dll`)

CPU components (`component/map` `MapIR`, `component/world` `World` + `Instance`, `component/world/atmosphere` session) and device passes (`pass/map` `MapPass`, `pass/world` `WorldPass`, `pass/world/atmosphere`) live in this tree. One product DLL: GN `//src/vista:vista`, `dll_stem=vista` (`vista.dll` / `vista_d.dll`).

The previous-generation engine is **Scenic** (`src/scenic` / `scenic.dll`, hosted by `src/content`). Leftover `src/legacy/render` is **frozen** until the cut completes. Scenic is **not** compiled into `vista.dll`. Leftover adapters under `legacy/gis/vista` are **not** Scenic.

Living layout lock: [`docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md`](../../docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md) **§Vista IR/Pass lanes**. Diagram: [`vista-subdirectory-layers.html`](../../docs/superpowers/diagrams/vista-subdirectory-layers.html). Layer table: [`docs/superpowers/src-layout.md`](../../docs/superpowers/src-layout.md).

Target names below are authoritative even while sources are mid-move. `frame/` and `scene/` are not part of this layout.

## Owns / does not own

| Owns | Does not own |
| --- | --- |
| `vista::Layout` �?`MapIR` / `DrawItem` | `gis::style` / `gis::tile` / OGR open (those stay `gis.dll`) |
| `vista::World` node graph; CPU `Instance`, sync, tess, cull, index; DEM domain; CPU mesh; point-cloud codecs | HWND, Views horizon, `ui/gfx` widgets |
| CPU `vista::atmosphere` (`FieldStore` / `Environment`) | Vista types in public headers |
| GPU `vista::MapPass`, `WorldPass`, `AtmosphereFrame` | Frame-graph vtable (`render/graph/frame_graph.h`) |
| Leftover adapters compiled **in** (`legacy/gis/vista`) | `#include "legacy/�?` from product TUs here |

`gis.dll` must not depend on this DLL (`assert_no_deps`). Product TUs must not include leftover. Leftover �?product is allowed. `sync_from` and `record_draws` stay methods on `WorldPass`.

## Modules

| Dir | Role | Include |
| --- | --- | --- |
| `component/map/` | CPU IR: `Layout` �?`MapIR`；顶�?`carto` / `shade` / `place` / `mvt` / `layout` | `"vista/component/map/ir.h"` umbrella；types in `view.h` / `draw.h` / `batch.h` / `layout.h` |
| `component/map/carto/` | filter / style / label collision | `"vista/component/map/carto/collision.h"` |
| `component/map/layout/` | 扁平：collect / emit / coalesce + per-geom emit | `"vista/component/map/layout/fill.h"` |
| `pass/map/` | `MapPass` (upload / encode / record) | `"vista/pass/map/pass.h"` |
| `component/world/` | `World` node graph + `Instance` (public root) | `"vista/component/world/world.h"` |
| `component/world/space/` | `coord` / `envelope`; `cull/` (frustum / mesh / prep); `index/` (AABB octree) | `"vista/component/world/space/cull/prep_cull.h"` |
| `component/world/instance/` | Instance fill: copy, lit policy, paint, kind tess | `"vista/component/world/instance/paint.h"` |
| `component/world/pointcloud/` | Chunk / LOD buckets on a node | `"vista/component/world/pointcloud/chunk.h"` |
| `component/world/terrain/` | `TerrainPayload` + `policy` / `grid` / seed by source | `"vista/component/world/terrain/seed.h"` |
| `pass/world/` | `WorldPass` (`sync_from`, `record_draws`); composes `TerrainPass`; `GpuMesh`; `opaque_effect` | `"vista/pass/world/pass.h"` |
| `pass/world/terrain/` | `TerrainPass` (upload / solid gate / kTerrain) | `"vista/pass/world/terrain/pass.h"` |
| `pass/world/detail/` | upload / tint / rebuild / draw / record | `"vista/pass/world/detail/upload.h"` |
| `pass/world/cull/` | `frustum_camera` (CameraMatrices �?planes) | `"vista/pass/world/cull/frustum_camera.h"` |
| `assets/` | Model, 3D Tiles, point-cloud file codecs | `"vista/assets/model/model.h"` |
| `assets/pointcloud/` | `PointCloud` + LAS / LAZ / PDAL / text | `"vista/assets/pointcloud/point_cloud.h"` |
| `mesh/` | CPU xyz+indices from GIS geometry (public API) | `"vista/mesh/tessellate.h"` |
| `mesh/fill/` | Polygon / ring fill tess | `"vista/mesh/fill/fill_tess.h"` |
| `mesh/line/` | Stroked ribbon tess (cap/join/dash) | `"vista/mesh/line/line_tess.h"` |
| `mesh/detail/` | Types, append, scratch pools, process-trace | `"vista/mesh/detail/mesh_types.h"` |
| `terrain/` | DEM raster, hillshade bake, land mask (no `World` in headers). Horn shade and even-odd mask call `gis/analysis` | `"vista/terrain/dem/dem_raster.h"` |
| `domain/` | `DomainSession` seam | `"vista/domain/domain.h"` |
| `component/world/atmosphere/` | CPU `Environment` + params | `"vista/component/world/atmosphere/environment.h"` |
| `component/world/atmosphere/field/` | FieldStore / ingest / procedural seed | `"vista/component/world/atmosphere/field/field_store.h"` |
| `component/world/atmosphere/ocean/` | `OceanSystem` + `cpu_waves` (no RHI) | `"vista/component/world/atmosphere/ocean/ocean_system.h"` |
| `component/world/atmosphere/cloud/` | CPU `CloudSystem` | `"vista/component/world/atmosphere/cloud/cloud_system.h"` |
| `pass/world/atmosphere/` | Facade: `AtmosphereFrame` / effects | `"vista/pass/world/atmosphere/atmosphere_frame.h"` |
| `pass/world/atmosphere/{ocean,cloud,sky,fog,globe}/` | Per-kind recorders; HLSL lives in sibling `*.hlsl` (embed via `embed_hlsl.gni` → `kVs*` / `kPs*` / `kCs*` in `hlsl.h`) | `"vista/pass/world/atmosphere/ocean/ocean_pass.h"` |

No `map/gpu`, `world/gpu`, or `atmosphere/gpu` nested directories. Headers sit next to their `.cc`. Include guards match the path (`VISTA_COMPONENT_…` / `VISTA_PASS_…`). No `gis/vista/` or `effect/` trees or forwarding headers.

## Namespaces

Public C++ stays two levels: `vista` (components and passes) and `vista::atmosphere`. Internals: `vista::detail` or an anonymous namespace. Do not add `vista::component`, `vista::pass`, or any other public third layer. `gis::style` / `gis::tile` / leftover `gis::Smt*` stay in `gis.dll`.

## GN

- DLL: `//src/vista:vista`
- Tests: `//src/vista:vista_test_all`
- Per-module `*_sources` compile into the DLL. CPU sets must not grow a `//src/render:render` dep (`assert_no_deps`). `assets` / `mesh` / `terrain` do not depend on `world`. `world_sources` depends on `assets` + `terrain` + `mesh`, and `assert_no_deps` `//src/render:render`. `map_sources` depends on `mesh` + `terrain` and `assert_no_deps` `//src/render:render`.
- GPU sets may depend on `render` plus the CPU set they consume: `map_pass_sources` �?`map_sources` + `render`; `world_pass_sources` �?`world_sources` + `render`; `atmosphere_pass_sources` �?`atmosphere_cpu_sources` + `render`.
- `//src/vista/pass/world/atmosphere:atmosphere_pass_sources` must not depend on `session_sources`.
- Env strings `GPUSCENE_PREP_PARALLEL` and `VISTA_LAYOUT_PARALLEL` stay. They drive `WorldPass` prep and `Layout` emit.
- Test executable `output_name` values stay (`map2d_pass_test`, `scene_gpu_test`, `unified_draw_test`). `content` `present/map2d` and `present/scene3d` directory names stay.
