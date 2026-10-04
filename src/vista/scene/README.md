<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/vista/scene`

Caller-linked `vista::GpuScene` + `OpaqueEffect` (not in `render.dll`).

## Env (prep / cull honesty)

| Env | Default | Semantics |
| --- | --- | --- |
| `SMT_SCENE3D_FRUSTUM_CULL` | **off** | Opt in (`=1`) to AABB-in-frustum cull on 3D meshes. Product DEM AABB mismatch historically blanked terrain when default-on. |
| `SMT_SCENE3D_NO_CULL` | — | Force cull off when set to `1` (legacy escape). |
| `SMT_GPUSCENE_PREP_PARALLEL` | **off** | Opt in (`=1`) for `prep_cull_parallel` workers (clamp 2–4). **Requires** `SMT_SCENE3D_FRUSTUM_CULL=1`; otherwise prep is a no-op / serial (no worker fan-out). |

Implementation: `detail/prep_cull.*` fills `visible[]` before serial `record_kind`. Workers never touch `rhi::Device` / CommandList.

Leftover `SMT_RHI3D_PREP_PARALLEL` does **not** apply here.

Tests: `//src/vista/scene:scene_gpu_test`, `unified_draw_test`.

---

**最后更新：** 2026-10-03
