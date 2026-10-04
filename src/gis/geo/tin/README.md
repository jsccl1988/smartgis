<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# tin（`src/gis/geo/tin`）

Thin C++23 wrapper around **`geos_c.h`** (same `gdal_sdk` GEOS as `geo::buffer`). Public API is `geo::delaunay` / `geo::delaunay_constrained` / `geo::IndexedTriangle`. No leftover headers or types. No in-tree Delaunay engine, no `tin::` namespace, no 3D tetrahedralizer.

GN：`//src/gis/geo:tin` → `//src/gis:gis`。测试：`tin_delaunay_test`。
