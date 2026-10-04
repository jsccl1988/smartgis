<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/vista/scene`

Caller-linked `vista::GpuScene` + `OpaqueEffect` (not in `render.dll`).

Public `GpuScene` TUs sit at the **module root** (same pattern as `vista/map`). Specialized subdirs: `cull/`, `index/`. No `gpu/` folder — that name collides with content's gpu/software present split and mislabels CPU tess TUs.

## Layers

| Path | Owns | RHI |
| --- | --- | --- |
| `scene.h` / `opaque_effect.*` / `gpu_instance.h` / tess / upload | `GpuScene` facade + IR + record | CPU TUs no; upload/record yes |
| `cull/` | frustum POD, `MeshCullItem`, `prep_cull`, CameraMatrices → planes | planes yes (camera header only) |
| `index/` | unibn AABB octree | no |

GN: `scene_cpu_sources` `assert_no_deps` `//src/render:render`. `scene_sources` public_deps the CPU set. `cull/frustum_camera.*` is in the GPU set because it names `CameraMatrices`.

## Env (prep / cull honesty)

| Env / switch | Default | Semantics |
| --- | --- | --- |
| `SMT_SCENE3D_FRUSTUM_CULL` / `--scene3d-frustum-cull=1` | **off** | Opt in to AABB-in-frustum cull on 3D meshes. Product DEM AABB mismatch historically blanked terrain when default-on. |
| `SMT_SCENE3D_NO_CULL` / `--scene3d-no-cull=1` | — | Force cull off. |
| `SMT_GPUSCENE_PREP_PARALLEL` / `--gpuscene-prep-parallel=1` | **off** | Opt in for `prep_cull` workers (clamp 2–4). **Requires** frustum cull on; otherwise prep is a no-op / serial. |

Implementation: `cull/prep_cull.*` fills `visible[]` from `MeshCullItem` (no `Buffer*`). Workers never touch `rhi::Device` / CommandList. `CameraMatrices` → planes in `cull/frustum_camera.*` before fan-out.

Spatial index: `index/aabb_octree.*` wraps MIT header-only **unibn** (`jbehley/octree`) over AABB **centers**. With cull on, `prep_cull_meshes` builds a **frame-local** mesh octree (not stored on `GpuScene`), radius-queries the frustum AABB, then `aabb_intersects_frustum`. `Octree.hpp` is not included from public headers.

Leftover `SMT_RHI3D_PREP_PARALLEL` does **not** apply here.

Tests: `//src/vista/scene:scene_gpu_test`, `unified_draw_test`.

Public include: `"vista/scene/scene.h"` (scheme C, no root shim).

---

**最后更新：** 2026-10-05
