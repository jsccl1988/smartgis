# geo（`src/algorithm/geo`）

几何核 DLL。**OGC 几何类型直接用 GDAL/OGR**（`OGRGeometry` / `OGRPoint` / `OGRLineString` / `OGRPolygon` / `OGRTriangulatedSurface` …），没有 `Smt*` 别名。调用方 `#include "ogr_geometry.h"`（或 `ogrsf_frmts.h`）并写 OGR 类名。

本目录只保留 **网格/TIN 容器**（接到 OGR），以及 `copy_envelope` / `SpatialRelation`。谓词与 overlay 走 OGR（内部已接 GEOS）。Delaunay 在 `src/algorithm/tin`。折线贴边辅助在 `src/plugin/orthogrid/detail`。

## 类型

| 类型 | 头 | 用途 | OGR 映射 |
| --- | --- | --- | --- |
| `geo::Grid` | `geometry.h` | 规则结点 **XY 矩阵**（正交网格 / 投影格网 / GDI 画线） | `OGRMultiPoint`（行优先）+ `width`/`height`。OGR 没有 `OGRGrid`；DEM 栅格仍用 `GDALDataset`。 |
| `geo::Tin` | `geometry.h` | 三角网（Delaunay **输出** / 绘制） | 包装 `OGRTriangulatedSurface`。Delaunay 在 `algorithm/tin`（GEOS）。`SmtTriangle.bDelete` 是旁路数组，不是第二套几何引擎。 |
| `geo::Surface3d` | `geometry.h` | XYZ 三角网（地形 / model3d） | 薄派生 `Tin`（结点带 Z）。已删 `3dgeometry.h`。 |

`geometry.h` 另有 `copy_envelope`（OGR → `base::Envelope`）和查询位 `SpatialRelation`。不要把它当成 OGR 兼容层。

## 文件

| 文件 | 内容 |
| --- | --- |
| `geometry.h` | Grid / Tin / `Surface3d` + 包络 / 空间关系位 |
| `grid.cpp` / `tin.cpp` | 实现 |
| `geometry_traits.h` / `vector_traits.h` / `geo_ops.h` | traits + `buffer()` 模板 |

## 用法

```cpp
#include "ogr_geometry.h"
#include "algorithm/geo/geo_ops.h"
#include "algorithm/geo/geometry.h"

OGRPoint pt(116.0, 39.0);
OGRGeometry* buffered = geo::buffer(pt, 100.0);

geo::Tin tin;
tin.add_point(&pt);
```

GN：`//src/algorithm/geo:geo` → `geo_d.dll` / `geo.dll`。
