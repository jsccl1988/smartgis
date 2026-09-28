<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `gis/datasource` layout

One DLL (`gis`). Top-level dirs are dependency layers; L3 backends live under L2 as `impl/`.

```
session ──► provider ──► provider/impl/{sdbd, ogr, gdal}
pipeline  (consumes OGRLayer*; independent of session)
```

| Dir | Layer | Role |
| --- | --- | --- |
| `session/` | L1 product | `DataSession`, `DatasetHandle`, `ConnectionSpec` → `MapLayer` / `Feature` |
| `provider/` | L2 adapters | `ProviderRegistry`, Local/Remote SDBD open |
| `provider/impl/sdbd/` | L3 backend | `client/` · `driver/` · `remote/` · `codec/` |
| `provider/impl/ogr/` | L3 backend | `codec/` · `text/` · `raster/` |
| `provider/impl/gdal/` | L3 register | `register_gdal_driver()` (+ pulls sdbd/ogr into DLL) |
| `pipeline/` | L4 exec | mogu-style produce/decode/sink feature load |

**Out of tree:** leftover catalog singleton at `src/legacy/datasource/mgr/` (`DataSourceMgr`). New code must not depend on it.

**Do not** add a new top-level dir without updating this table and `docs/build/src-layout.md`.
