<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `scenic/render/rhi2d/impl/common/paint/carto/`

Carto encode / draw / style / frame for Scenic map paint. Sibling of
`paint/map/` under the rhi2d host tree (not a top-level `scenic/map` peer).

| Subdir | Role |
| --- | --- |
| `frame/` | Per-frame carto context + `carto_frame` |
| `encode/` | Command buffer / encoder |
| `draw/` | Device / OGR / mesh / primitives |
| `style/` | Style + xform |

Map layer walk stays in sibling `paint/map/map_*.*`. Backend execute stays in
`paint/backend/`.
