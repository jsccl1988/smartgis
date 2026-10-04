<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# proj（`src/gis/geo/proj`）

Thin C++23 RAII wrapper around the shipped PROJ 9 C API (`#include <proj.h>` in the `.cc` only). Compiled into `gis.dll`. Layer identity is `OGRLayer::GetSpatialRef()`; this tree owns **transforms**. Do not put projection math in `sdb/crs` or `content/public`. Do not vendor a second PROJ.

Public surface: `geo::CoordinateTransform` and `geo::transform_xy`. CRS strings are EPSG / WKT / PROJ. `PJ*` / `PJ_CONTEXT*` stay inside the wrapper.

Living：[`2026-09-13-algorithm-layer-oss-design.md`](../../../docs/superpowers/specs/2026-09-13-algorithm-layer-oss-design.md) §3–§6.

## 用法

```cpp
#include "gis/geo/proj/coordinate_transform.h"

double x = 116.0;
double y = 39.0;
geo::CoordinateTransform webmerc("EPSG:4326", "EPSG:3857");
if (webmerc.is_valid()) {
  webmerc.transform_xy(x, y);
}
```

GN：`//src/gis/geo:proj` → `//src/gis:gis`。测试：`proj_test`。基准：`proj_benchmark`。
