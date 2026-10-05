<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/vista/component/map`

CPU cartography: `Layout::build` → `MapIR` / `DrawItem`. No RHI, HWND, or
`CameraMatrices`. Ortho and perspective share this Layout; `View::mode` is a
host hint. Compiled into `vista.dll` (`//src/vista/component/map:map_sources`).
`assert_no_deps` `//src/render:render`.

Living lock: [`docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md`](../../../../docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md)
**§Vista map deep split** · **§Vista map/frame scenic-peer** · **§Vista map/frame scenic damage**.
Diagram: [`vista-map-frame-scenic-peer.html`](../../../../docs/superpowers/diagrams/vista-map-frame-scenic-peer.html).

## Layers

| Path | Owns |
| --- | --- |
| `view.h` / `draw.h` / `batch.h` / `layout.h` | Public types. Hosts keep `#include "vista/component/map/ir.h"` (umbrella). |
| `layout.cc` | Orchestrator: TLS arena → `collect_visible` → `pack_geoms` → `emit_visible_layers` → `coalesce_draw_items`. |
| `multiply.h` | Hillshade luma coverage (CPU; GPU upload and GDI both call it). |
| `place.*` | MapIR → ortho meshes (no RHI). |
| `detail/carto_filter.*` | Scale / role / stem policy. No OGR construction. |
| `detail/batch_build.cc` | POD `BatchLayer` → owned OGR `LayerBatchSet`. |
| `detail/collision.*` | `LabelGrid` (serial after emit). |
| `detail/hillshade_bake.*` / `mvt_layout.*` / `default_style.cc` | Bake orchestration, MVT→batches, carto JSON. |
| `layout/collect.*` / `emit.*` / `coalesce.*` | Visible layers, painter dispatch, DrawItem merge. |
| `layout/pack.*` | Envelope / sub-pixel / overview hole prep before tess. |
| `layout/{fill,line,point,symbol,raster}.*` | Per-geom emit. Tile clip + `layout_gen` abort inside tess jobs. |
| `layout/slice_key.h` | World AABB tile × style cache key + 16px skirt; retained splice. |
| `layout/tess_grain.h` | Parallel tess thresholds only. |
| `layout/geom_walk.*` | OGR walks (`for_each_line`, anchor, codepoint). |
| `layout/mesh_emit.*` | Quads, tess meshes, circle fans, line options. |

Do not add `vista::map` as a public third namespace. Internals stay `vista::detail`.
No forwarding headers at the old `vista/component/map/{carto_filter,collision,hillshade_bake,mvt_layout}.h` paths.

Tests: `//src/vista/component/map:frame_test` (`frame_test.exe`).

---

**最后更新：** 2026-10-05
