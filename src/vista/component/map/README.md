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
Diagram: [`vista-map-component-layers.html`](../../../../docs/superpowers/diagrams/vista-map-component-layers.html)
· [`vista-map-layout-layers.html`](../../../../docs/superpowers/diagrams/vista-map-layout-layers.html).

## Layers

Five orthogonal subdirs under the public root. `layout/` nests four role dirs.

| Path | Owns |
| --- | --- |
| `ir.h` | Umbrella. Hosts keep `#include "vista/component/map/ir.h"`. |
| `view.h` / `draw.h` / `batch.h` / `layout.h` | Public types. `layout.cc` orchestrates collect → pack → emit → coalesce. |
| `batch.cc` | POD `BatchLayer` → owned OGR `LayerBatchSet`. Declaration stays on `batch.h`. |
| `color.h` | Shared `0xAARRGGBB` unpack (`vista::detail`). Header-only. |
| `carto/` | Style / scale / role / stem (`filter.*`, `style.cc`) and label declutter (`collision.*`). No OGR construction. |
| `shade/` | Hillshade luma coverage (`multiply.*`, vir-simd AVX2-width TU) and bake / `TileSlot` (`bake.*`). Shade math stays in `vista/terrain`. |
| `place/` | MapIR → ortho meshes (no RHI). |
| `mvt/` | MVT → batches / `MapIR`. Decode stays in `gis/tile`. |
| `layout/` | Nested roles: `stage/` · `paint/` · `geom/` · `policy/`. |

`layout/` deps: stage → paint → geom → policy. Painters do not include stage
headers (`layout_gen_stale` lives in `policy/gen.h`). `carto` does not construct
OGR and does not include stage headers. `shade` does not include `place/` or
layout painters.

| Role | Path |
| --- | --- |
| stage | `layout/stage/{collect,pack,emit,coalesce}.*` |
| paint | `layout/paint/{fill,line,point,symbol,raster}.*` + `source_index.h` + `tess_jobs.h` |
| geom | `layout/geom/{geom_walk,mesh_emit,attrs,clip}.*` |
| policy | `layout/policy/{slice_key,tess_grain,view_metrics,gen}.*` |

Do not add `vista::map` as a public third namespace. Internals stay `vista::detail`.
No forwarding headers at retired `detail/`, `collision/`, flat `layout/*.h`, or
root `place.h` / `multiply.h`.

Tests: `//src/vista/component/map:frame_test` (`frame_test.exe`).
Benchmark: `//src/vista/component/map:multiply_benchmark` (`multiply_benchmark`). Scalar, AVX2 kernel, and public dispatch at 64²–1024².

---

**最后更新：** 2026-10-08
