<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# rhi3d `impl/common`

Shared leftover 3D **FrameJob / CPU prep** for `scenic_render_gl` and
`scenic_render_d3d`. This directory is **not** a scene graph.

## Owns

| Dir | Owns |
| --- | --- |
| `frame/` | `FrameScheduler` (serial Render worker; HWND never joins) · `PrepRunner` (AABB frustum, N=1..4) · `FrameRequest` |

## Does not own (stay in `src/scenic/scene3d`)

The Vista peer of Scenic 3D **objects** is `src/vista/scene` (GpuScene). That
tree lives at **`src/scenic/scene3d/`**, a sibling of `render/rhi3d`, not under
this folder.

Do **not** move here:

- `Scene` / `Object3d` / octree / primitive / map attach / CPU tess (`feature_mesh`)
- `stereo_hwnd_view` (owns `Scene` + map attach + device; host glue, not a device part; lives in `scene3d/host/`)
- `d3d_deferred_objects.h` (partitions `Object3d*` / `MapLabelBatch`; uses D3D C
  exports). The **device pool** is `impl/d3d/host/deferred_draw.*`.

Sinking the scene graph here would invert layers (`rhi3d` would `#include`
scene types) and break the locked Scenic ↔ Vista `scene` peer.

Living lock: [`docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md`](../../../../../../docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md)
**§Scenic** · diagram [`legacy-render-architecture.html`](../../../../../../docs/superpowers/diagrams/legacy-render-architecture.html).
