<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `paint/carto/`

Leftover 2D carto paint helpers shared by `legacy_rhi2d_{gdi,gdiplus,skia}`.

Equal peers (no root sources, no forwarding headers):

| Dir | Owns |
| --- | --- |
| `encode/` | `Rhi2dCommandBuffer` / `Rhi2dCommandEncoder` / encoder TLS |
| `draw/` | `Rhi2dCartoDraw` façade + `draw_*` + geom helpers |
| `frame/` | `GdiCartoFrame` / `SmtRenderContext` / preview xform |
| `style/` | pen/brush style cache + LP↔DP xform |

Map layer walk stays in sibling `paint/map/`. Backend execute is `paint/backend/`.
