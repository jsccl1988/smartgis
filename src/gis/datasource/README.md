<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `gis/datasource` layout

One DLL (`gis`). Session/provider/pipeline stay as layers; OGR/SDBD/GDAL backends sit as siblings of `provider/` (no `impl/` wrapper).

```
session ──► provider ──► {sdbd, ogr, gdal}
pipeline  (consumes OGRLayer*; independent of session)
```

| Dir | Role |
| --- | --- |
| `session/` | `DataSession`, `DatasetHandle`, `ConnectionSpec` (+ `DbProvider` / `eDSType`) → `MapLayer` / `Feature` |
| `provider/` | `ProviderRegistry`, Local/Remote SDBD open |
| `sdbd/` | Client, GDAL `"SDBD"` driver, remote dataset, JSON codec |
| `ogr/` | Feature codec, connect traits, text encoding, raster scratch |
| `gdal/` | `register_gdal_driver()` |
| `pipeline/` | Produce/decode/sink feature load |

**Out of tree:** leftover catalog singleton at `src/gis/datasource/mgr/` (`DataSourceMgr`). New code must not depend on it.

**Do not** add a new top-level dir without updating this table and `docs/superpowers/src-layout.md`.
