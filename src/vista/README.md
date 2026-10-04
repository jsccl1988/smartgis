<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/vista` — viewport vista layer (`vista.dll`)

CPU map frame / logical World **and** GPU map / GpuScene / atmosphere passes live in this tree. One product DLL: GN `//src/vista:vista`, `dll_stem=vista` (`vista.dll` / `vista_d.dll`).

The previous-generation engine is **Scenic** (`src/scenic` / `scenic.dll`, hosted by `src/content`). Leftover `src/legacy/render` is **frozen** until the cut completes. Scenic is **not** compiled into `vista.dll`. Leftover adapters under `legacy/gis/vista` are **not** Scenic.

Living layout lock: [`docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md`](../../docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md) **§Vista subdirectory tighten**. Diagram: [`docs/superpowers/diagrams/vista-subdirectory-layers.html`](../../docs/superpowers/diagrams/vista-subdirectory-layers.html). Layer table: [`docs/superpowers/src-layout.md`](../../docs/superpowers/src-layout.md).

## Owns / does not own

| Owns | Does not own |
| --- | --- |
| `vista::Layout` → `MapFrame` / `DrawItem` | `gis::style` / `gis::tile` / OGR open (those stay `gis.dll`) |
| `vista::World` + DEM tessellate + point-cloud buffers | HWND, Views chrome, `ui/gfx` widgets |
| CPU `vista::atmosphere` session (`domain/`) | FlyCube types in public headers |
| GPU `vista::Pass`, `GpuScene`, `AtmosphereFrame` | Frame-graph vtable (`render/graph/frame_graph.h`) |
| Leftover adapters compiled **in** (`legacy/gis/vista`) | `#include "legacy/…"` from product TUs here |

`gis.dll` must not depend on this DLL (`assert_no_deps`). Product TUs must not include leftover. Leftover → product is allowed.

## Modules

| Dir | Role | Include |
| --- | --- | --- |
| `frame/` | CPU cartography once per frame | `"vista/frame/frame.h"` |
| `frame/detail/` | `carto_filter`, `collision` (flat files) | `"vista/frame/detail/collision.h"` |
| `frame/detail/layout/` | Per-geom emit | `"vista/frame/detail/layout/fill.h"` |
| `world/` | Logical scene graph | `"vista/world/world.h"` |
| `world/terrain/{dem,mesh,process}/` | DEM raster/cache, tessellate, hillshade | `"vista/world/terrain/dem/dem_raster.h"` |
| `world/pointcloud/` | Buffer + ingest (no `io/`) + process | `"vista/world/pointcloud/point_cloud.h"` |
| `assets/` | CPU ModelAsset / Tileset | `"vista/assets/model/model.h"` |
| `domain/` | `DomainSession` seam | `"vista/domain/domain.h"` |
| `domain/atmosphere/` | CPU FieldStore / Environment (flat) | `"vista/domain/atmosphere/environment.h"` |
| `map/` | 2D GPU pass (consumes `DrawItem`) | `"vista/map/pass.h"` |
| `scene/` | GpuScene sync + record | `"vista/scene/scene.h"` |
| `atmosphere/` | GPU pass order + ocean/cloud/sky/fog/globe | `"vista/atmosphere/frame/atmosphere_frame.h"` |

Headers sit next to their `.cc`. No `gis/vista/` or `effect/` trees or forwarding headers.

## Namespaces

Public C++ stays two levels: `vista` (CPU frame / World / GPU) and `vista::atmosphere` (CPU domain). Internals: `vista::detail` or an anonymous namespace. `gis::style` / `gis::tile` / leftover `gis::Smt*` stay in `gis.dll`. Do not add `vista::frame` as a public third layer.

## GN

- DLL: `//src/vista:vista`
- Tests: `//src/vista:vista_test_all`
- Per-module `*_sources` compile into the DLL. CPU sets must not grow a `//src/render:render` dep (`assert_no_deps`). GPU sets may depend on `render` + CPU sets they consume (`map` → `frame`, `scene` → `world`).
