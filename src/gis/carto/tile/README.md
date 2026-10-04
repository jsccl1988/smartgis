<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `gis/carto/tile`

**Diagram:** [`docs/superpowers/diagrams/gis-vista-architecture.html`](../../../../docs/superpowers/diagrams/gis-vista-architecture.html)

HTTP(S) XYZ / WMTS `TileProvider`、进程内/磁盘缓存、Style `sources` 绑定、MVT decode。命名空间 **`gis::tile`**（`TileProvider`、`TileCache`、`SourceRegistry`、`TileCoord`、`Mvt*` 等）。Style JSON 在并列的 [`../style/`](../style/)（`gis::style`）。产品树无 `gis/present/`。进 **`gis` DLL**（deps `//src/net:net`）。

| 区域 | 职责 |
| --- | --- |
| colocated files | `TileProvider`, `SourceRegistry`, `ProviderTileLayer` / `MapLayer` helpers, MVT, LRU/disk cache, XYZ/WMTS |

**不是** content `browser/present`（上屏），也不是 `legacy/gis/present/carto`（制图 POD）。
