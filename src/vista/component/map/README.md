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
**§Vista map component layers** · **§Vista map layout layers**.
Diagram: [`vista-map-component-layers.html`](../../../../docs/superpowers/diagrams/vista-map-component-layers.html).

## Layers

Five orthogonal subdirs under the public root. No nested dirs inside `layout/`.

| Path | Owns |
| --- | --- |
| `ir.h` | Umbrella. Hosts keep `#include "vista/component/map/ir.h"`. |
| `view.h` / `draw.h` / `batch.h` / `layout.h` | Public types. `layout.cc` orchestrates collect → pack → emit → coalesce. |
| `batch.cc` | POD `BatchLayer` → owned OGR `LayerBatchSet`. Declaration stays on `batch.h`. |
| `color.h` | Shared `0xAARRGGBB` unpack (`vista::detail`). Header-only. |
| `carto/` | Style / scale / role / stem (`filter.*`, `style.cc`) and label declutter (`collision.*`). No OGR construction. |
| `shade/` | Hillshade luma coverage (`multiply.*`, AVX2 TU) and bake / `TileSlot` (`bake.*`). Shade math stays in `vista/terrain`. |
| `place/` | MapIR → ortho meshes (no RHI). |
| `mvt/` | MVT → batches / `MapIR`. Decode stays in `gis/tile`. |
| `layout/` | Flat pipeline + painters + geom helpers + policy headers. |

`layout/` file roles (include rules, not subdirs): stage (`collect` / `pack` / `emit` / `coalesce`) → paint (`fill` / `line` / `point` / `symbol` / `raster`) → geom (`geom_walk` / `mesh_emit` / `attrs` / `clip`) → policy (`slice_key` / `tess_grain` / `view_metrics` / `gen`). Painters do not include stage headers. `carto` does not construct OGR and does not include stage headers. `shade` does not include `place/` or layout painters.

Do not add `vista::map` as a public third namespace. Internals stay `vista::detail`.
No forwarding headers at retired `detail/`, `collision/`, `layout/{stage,paint,geom,policy}/`, or root `place.h` / `multiply.h`.

Tests: `//src/vista/component/map:frame_test` (`frame_test.exe`).
Benchmark: `//src/vista/component/map:multiply_benchmark` (`multiply_benchmark`). Scalar, AVX2 kernel, and public dispatch at 64²–1024².

---

**最后更新：** 2026-10-07
