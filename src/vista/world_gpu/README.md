<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/vista/world_gpu`

Caller-linked `vista::WorldPass` + `OpaqueEffect` (not in `render.dll`).

CPU sync, tessellation, cull, and the spatial index live in `vista/world`
(`Instance`; `world_sources` `assert_no_deps` `//src/render:render`). This
directory is the device-thread upload and record path. `GpuMesh` stays here
because it holds device buffers.

Public include: `"vista/world_gpu/pass.h"` (scheme C, no forwarding header).
Namespace stays `vista`.

## Layers

| Path | Owns | RHI |
| --- | --- | --- |
| `pass.h` / `opaque_effect.*` / `gpu_mesh.h` / upload / tint / draw | `WorldPass` record | yes |
| `cull/frustum_camera.*` | `CameraMatrices` → frustum planes | camera header only |

`WorldPass::sync_from(const World&)` and `record_draws` stay on this object.
Content holds one long-lived `WorldPass`.

GN: `world_gpu_sources` depends on `//src/vista/world:world_sources` and
`//src/render:render`.

## Env

| Env / switch | Default | Semantics |
| --- | --- | --- |
| `SMT_SCENE3D_FRUSTUM_CULL` / `--scene3d-frustum-cull=1` | **off** | Opt in to AABB-in-frustum cull on 3D meshes. |
| `SMT_SCENE3D_NO_CULL` / `--scene3d-no-cull=1` | — | Force cull off. |
| `SMT_GPUSCENE_PREP_PARALLEL` / `--gpuscene-prep-parallel=1` | **off** | Drives `WorldPass` prep (`prep_cull` workers, clamp 2–4). Requires frustum cull; otherwise prep stays serial. |

The env string `SMT_GPUSCENE_PREP_PARALLEL` is unchanged so harness switches keep working.

CPU index `vista/world/index/aabb_octree.*` wraps unibn. With cull on, `prep_cull_meshes` builds a frame-local octree (not stored on `WorldPass`).

Tests: `//src/vista/world_gpu:scene_gpu_test` (`scene_gpu_test`), `unified_draw_test`.

---

**最后更新：** 2026-10-05
