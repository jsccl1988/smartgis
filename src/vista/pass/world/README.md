<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/vista/pass/world`

Caller-linked `vista::WorldPass` + `OpaqueEffect` (not in `render.dll`).

CPU sync, tessellation, cull, and the spatial index live in `vista/component/world`
(`Instance`; `world_sources` `assert_no_deps` `//src/render:render`). This
directory is the device-thread upload and record path. `GpuMesh` stays here
because it holds device buffers.

Public include: `"vista/pass/world/pass.h"` (scheme C, no forwarding header).
Namespace stays `vista` / `vista::detail`. Do not add `vista::pass`.

## Layers

| Path | Owns | RHI |
| --- | --- | --- |
| `pass.h` / `pass.cc` | `WorldPass` facade: sync, pipelines, view/tint setters; composes `TerrainPass` | yes |
| `terrain/pass.*` | `TerrainPass`: DEM/TIN upload, solid gate, `kTerrain` record | yes |
| `gpu_mesh.h` | `GpuMesh` POD + `as_cull_item` | yes |
| `opaque_effect.*` | frame-graph `kOpaque` adapter | yes |
| `detail/upload.*` | VB/IB/texture upload | yes |
| `detail/tint.*` | paint / point / DEM default tint on `GpuMesh` | no Device calls |
| `detail/rebuild.cc` | `WorldPass::rebuild_meshes` (tess → upload) | yes |
| `detail/draw.*` | per-`NodeKind` `record_kind` | yes |
| `detail/record.cc` | `WorldPass::record_draws` / `record` | yes |
| `cull/frustum_camera.*` | `CameraMatrices` → frustum planes | camera header only |

`WorldPass::sync_from(const World&)` and `record_draws` stay on this object.
Content holds one long-lived `WorldPass`. Terrain upload / solid gate / `kTerrain`
record live in `terrain/TerrainPass` (composed internally). Paint RGBA mapping lives in
`"vista/component/world/instance/paint.h"` (`rgba_from_resolved_paint`), not on the GPU facade.

GN: `world_pass_sources` depends on `//src/vista/component/world:world_sources` and
`//src/render:render`.

## Env

| Env / switch | Default | Semantics |
| --- | --- | --- |
| `SCENE3D_FRUSTUM_CULL` / `--scene3d-frustum-cull=1` | **off** | Opt in to AABB-in-frustum cull on 3D meshes. |
| `SCENE3D_NO_CULL` / `--scene3d-no-cull=1` | — | Force cull off. |
| `GPUSCENE_PREP_PARALLEL` / `--gpuscene-prep-parallel=1` | **off** | Drives `WorldPass` prep (`prep_cull` workers, clamp 2–4). Requires frustum cull; otherwise prep stays serial. |

The env string `GPUSCENE_PREP_PARALLEL` is unchanged so harness switches keep working.

CPU index `vista/component/world/space/index/aabb_octree.*` wraps unibn. With cull on, `prep_cull_meshes` builds a frame-local octree (not stored on `WorldPass`).

Tests: `//src/vista/pass/world:scene_gpu_test` (`scene_gpu_test`), `unified_draw_test`,
`terrain_pass_test`.

---

**最后更新：** 2026-10-07
