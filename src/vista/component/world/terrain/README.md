<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/vista/component/world/terrain`

CPU terrain IR under World: `TerrainPayload` (mesh + drape + `lod_key` +
`TerrainSource`), discrete LOD policy, nested-grid selection, and seed
entry points. No RHI. `assert_no_deps` `//src/render:render` via
`world_sources`.

Living lock: [`docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md`](../../../../../docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md)
**§Vista world LOD terrain component+pass** · **§Vista world component deep split**.
Diagram: [`world-lod-terrain.html`](../../../../../docs/superpowers/diagrams/world-lod-terrain.html).

Bake domain stays in `vista/terrain` (`DemRaster` / hillshade). This directory
only writes `kTerrain` nodes.

## Layers

| Path | Owns |
| --- | --- |
| `payload.h` | `TerrainPayload` nested on `Node` / `Instance` |
| `policy.*` | LOD numbers: raster `max_edge`, TIN stride, surface edge, cache keys, morph weight |
| `grid.*` | `NestedGridTile` selection and Y-up edge skirts |
| `mesh.*` | Internal payload writers (stamp, thin, window node, patch continuity) |
| `seed.h` | Public seed API |
| `raster.cc` / `nested.cc` / `surface.cc` / `tin.cc` | One translation unit per source |

## LOD

| Source | Policy | Seed API |
| --- | --- | --- |
| Raster grid | `terrain_lod_max_edge` + view tiles | `seed_dem_raster_lod_into_world` / `seed_dem_view_tiles_into_world` |
| Nested grid (CPU CDLOD rings) | per-tile `max_edge` from `terrain_lod_patch_distance`; CPU `morph_weight` + Y-up edge skirts | `seed_dem_nested_grid_into_world` |
| Heightfield surface | `terrain_lod_surface_edge` → `DemHeightField::build_mesh` | `seed_dem_surface_lod_into_world` |
| TIN (`OGRTriangulatedSurface`) | stride fallback, or centroid-to-camera spatial thin | `seed_tin_lod_into_world` |

**Gap:** RHI/`TerrainPass` has no hull tessellation or geometry clipmap. Nested-grid is CPU rings + discrete `lod_max_edge` + skirts/morph IR — not GPU clipmap tess or shader morph.

Public includes: `"vista/component/world/terrain/seed.h"`,
`"vista/component/world/terrain/policy.h"`,
`"vista/component/world/terrain/grid.h"`,
`"vista/component/world/terrain/payload.h"`.
`mesh.h` is internal. Namespace `vista` / `vista::detail`. No forwarding
header at the old `terrain/lod.h` path.

GPU upload/record: `vista/pass/world/terrain/TerrainPass` (composed by `WorldPass`).

---

**最后更新：** 2026-10-07
