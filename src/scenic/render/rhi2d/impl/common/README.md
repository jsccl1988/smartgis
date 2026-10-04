<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# rhi2d `impl/common`

Shared leftover 2D host tree for `scenic_rhi2d_gdi` / `_gdiplus` / `_skia`.

## Concepts (keep thin)

Three axes only — do not add parallel Chromium names for the same job:

| Axis | Dir | Owns |
| --- | --- | --- |
| HWND / ABI | `host/` | `Rhi2dRenderDevice`, sealed `PresentController` (debounce + present-on-gen), `preview_transform` helpers, published surfaces, host `CartoDraw` + overlay painter |
| Frame beat | `cc/` | `LayerTreeHost` (sole map `Painter` + `back_buf_` + `share_from` alias of host front), `Scheduler`, `LayerTreeImpl` (gen/damage) |
| Paint | `paint/` | `backend/` · `carto/{encode,draw,frame,style}` · `map/` (Painter + prep/play) |
| Buffer / compose | `surface/` | `dib/` · `composer/` |

GIS map layers live on `SmtMap` and are walked by `paint/map` — there is **no** second `cc::Layer` list.

**Ownership:** Host owns published HBITMAPs. LTH `shared_buf_` is `share_from(map_front_)` only. Full-map frames use `LayerTreeHost::paint_map_sync` / `submit_frame`; Host never keeps a second map-frame painter.

## Pipeline

`PresentController` → `LayerTreeHost::stage/submit` → activate →
`Painter` encode (serial OGR) → parallel mode via `SMT_RHI2D_PARALLEL`:

| Mode | Execute | Compose |
| --- | --- | --- |
| `tile` (default) | `execute_tile` on `Rhi2dTileGraphRunner` | blit dirty tile centers |
| `layer` | per-GIS-layer `execute` on same runner | ocean clear + opaque SRCCOPY z-order |
| `serial` | full-frame `execute` | n/a |

Legacy: `SMT_RHI2D_TILE_RASTER=0` → serial when `PARALLEL` unset.
`SMT_RHI2D_PARALLEL_LOG=1` prints `execute_ms` on stderr.

Port selection is LoadLibrary by `chAPI`, not compile macros.

Tile helpers: `cc/raster_tile.*`, `cc/tile_graph_runner.*`. Default tile
256px, outset 16px (`SMT_RHI2D_TILE_SIZE` / `SMT_RHI2D_TILE_OUTSET`).
Layer mode is for wall-clock A/B; overlapping layers will not match serial pixels.