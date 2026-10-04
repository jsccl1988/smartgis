<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `scenic/render/rhi2d/impl/common/paint/map/`

Map / layer / feature **drawing strategy** for Scenic rhi2d. Sibling of
`paint/carto/` (primitives + encode) and consumer of `cc/` (FrameJob +
tile graph). Not a third public namespace.

| File | Role |
| --- | --- |
| `map_painter.*` | Map-level façade (`Rhi2dPainter` / `MapPainter`): visible-layer walk, cross-layer prep wave, ordered encode, then submit |
| `map_feature_prep.*` | Feature CPU prep (LP→DP + thin) |
| `map_draw_batch.*` | Feature encode: line/road PolyPolyline coalesce, same-fill polygon style runs |
| `map_geom_trace.*` | Per-geom timing |
| `map_prep_pipeline.h` | Chunked parallel prep helper |

Carto does **not** walk `gis::Map`. `cc/` does **not** own GIS layers.
`MapPainter` stays the Host / LayerTreeHost seam name.

Vector collect uses the viewport LP envelope (padded for stroke/label bleed)
as an OGR spatial filter so drivers with an R-tree / `.qix` skip off-screen
features. Tile images are skipped when `world_rect` misses the view.
