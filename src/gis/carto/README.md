<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `gis/carto`

Cartographic **inputs** to CPU `MapFrame` / Map2d — not content `browser/present` and not leftover POD brushes.

| Child | Namespace | Role |
| --- | --- | --- |
| [`style/`](style/) | `gis::style` | MapLibre-subset `StyleDocument`, symbols, paint resolve |
| [`tile/`](tile/) | `gis::tile` | HTTP(S) XYZ/WMTS `TileProvider`, cache, MVT |

Directory grouping only — no third public namespace `gis::carto`. Leftover style POD stays at `legacy/gis/present/carto` (name collision is intentional: product JSON vs leftover brushes). Product tree has **no** `gis/present/` (present lives in content / `render::graph`).

Into **`gis.dll`**. Diagram: [`gis-vista-architecture.html`](../../../docs/superpowers/diagrams/gis-vista-architecture.html).
