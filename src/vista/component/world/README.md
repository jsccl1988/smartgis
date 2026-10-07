<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/vista/component/world`

CPU world graph: `World` / `Node` and the frame IR `Instance`. No RHI.
Compiled into `vista.dll` (`//src/vista/component/world:world_sources`).
`assert_no_deps` `//src/render:render`. Device upload is `vista/pass/world`.

Living lock: [`docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md`](../../../../docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md)
**§Vista world component layers**. Diagram:
[`vista-world-component-layers.html`](../../../../docs/superpowers/diagrams/vista-world-component-layers.html).

## Layers

| Path | Owns |
| --- | --- |
| `world.*` / `instance.h` | Public graph and frame IR. Hosts include `"vista/component/world/world.h"`. |
| `space/coord.*` · `space/envelope.*` | Y-up↔GIS and view / vertex AABB. Foundation; no cull / index. |
| `space/cull/` | Frustum planes (`frustum_aabb`), mesh POD (`mesh_cull`), prep workers (`prep_cull`). |
| `space/index/` | AABB octree over unibn. Used by prep; not stored on `WorldPass`. |
| `instance/` | Fill `Instance`: node copy (`sync`), lit-PSO policy (`pipelines`), paint (`paint`), vector tess (`tessellate`), per-`NodeKind` dispatch (`kind`). |
| `pointcloud/` | Chunk buckets and point LOD on a node. |
| `terrain/` | `TerrainPayload`, LOD policy, nested grid, seed by source. |

`space/` does not include `instance/`. `instance/` does not include `space/`.
Inside `instance/`, `kind` → `tessellate` → `paint`.
Inside `space/`, `cull/prep_cull` → `index/aabb_octree` → `cull/frustum_aabb`.
`coord` / `envelope` stay at the `space/` root (no `coord/coord.h` same-name nest).

Public namespaces stay `vista` / `vista::detail`. No forwarding headers at the
old flat `space/*.h` cull/index paths, or at retired `coord/` / `cull/` /
`index/` / `mirror/` world roots.

Tests: `world_test`, `seed_tin_test`, `pointcloud_test`.

---

**最后更新：** 2026-10-07
