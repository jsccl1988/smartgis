<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# geo（`src/gis/geo`）

几何核（编进 `gis.dll`）。本目录四块：

1. **OGR 封装 + 几何体** — `ops/`（OGC 类型以 `OGRGeometry` 为唯一所有权。2D XY lattice 在插件 `OrthoLattice`；3D hex：`plugin/product/world3d/hexgrid` `HexLattice`）
2. **坐标投影** — `proj/`（PROJ 9 CRS 变换；图层 CRS 走 `OGRSpatialReference`）
3. **TIN 生成（OSS 包装）** — `tin/`（GEOS 2D Delaunay/CDT；写出调用方的 `OGRTriangulatedSurface*`）
4. **结构化格网椭圆光滑 + 正交性** — `grid/`（2D 五点 Laplace / Thompson P=Q=0；3D 七点 Laplace；`geo::compute_orthogonality` 热力场）。与 `tin/` 并列，**不**并入 `delaunay.h`。XY 缓冲类型在插件 `OrthoLattice`。

没有产品内第二套虚几何树，也没有 `SmtGeometry` / `SmtTin` 别名。调用方 `#include "ogr_geometry.h"` 并写 OGR 类名。

**OGC TIN（P4）：** 身份是 `OGRTriangulatedSurface` + `OGRTriangle`（`wkbTIN`）。2D XY lattice 在插件 `OrthoLattice`（OGR 无 Grid 类型）。**P5：** `geo::HexGrid` **已从 gis.dll 删除**；3D 结构化格网由 `plugin/product/world3d/hexgrid` 的 `HexLattice` 拥有。空间算子走 `geo::ops`；Delaunay 走 `geo::delaunay`；结构化光滑走 `geo::solve_laplace` / `geo::solve_elliptic`（`NodeField*`）；正交性走 `geo::compute_orthogonality`；CRS 走 `geo::CoordinateTransform`。

**分层一句话：** OGR 拥有矢量几何实例（含 TIN）；2D XY lattice 是插件 `plugin::detail::OrthoLattice`；3D hex 仅 `HexLattice`；traits 统一实例 `coordinate_dimension`；薄 `indexed_tin` 只做 TIN 缓冲↔OGR；WKB/Feature 归 datasource。

**算法分离：** `ops/` 提供 OGR 集合包装、envelope helpers 与 `buffer`；`tin/` / `grid/` / `proj/`（及 `analysis`）是算法，依赖 concept，**几何头不得反向 `#include` 算法实现**。`geo::buffer` 约束为 `ogr_geometry_like`（TIN 须先 `indexed_tin`）。公开命名空间两层：`geo` / `geo::detail`（目录 `geo/tin`、`geo/grid` 不升第三语义层、不用 `namespace tin` / `namespace grid`）。见 living **§2–§4**、**P4–P5**。

Living：[`2026-09-13-algorithm-layer-oss-design.md`](../../../docs/superpowers/specs/2026-09-13-algorithm-layer-oss-design.md)。  
图（主）：[`gis-geo-layers.html`](../../../docs/superpowers/diagrams/gis-geo-layers.html) ·（补）：[`gis-algorithm-geometry-split.html`](../../../docs/superpowers/diagrams/gis-algorithm-geometry-split.html)。

## 分层

| Lane | Owner | 说明 |
| --- | --- | --- |
| Types · OGC | GDAL/OGR | 唯一几何所有权；TIN = `OGRTriangulatedSurface`；实例 dim 2\|3 |
| Types · Mesh（计算缓冲） | plugin | `OrthoLattice` / `HexLattice`（world3d）；不在 `gis.dll` |
| Traits | `geo::` | `geometry_traits` + `geometry_like` / `ogr_geometry_like` + `vector_traits` |
| Ops | `geo::` | `buffer`（仅 OGR）；`geo::delaunay`（geos_c）；`geo::solve_laplace` / `solve_elliptic` / `compute_orthogonality`（结构化格网）；CRS `geo::CoordinateTransform` |
| Codec | geo 薄 + datasource | `indexed_tin`；Feature/WKB 在 `ogr_feature_codec` |
| Present | vista / content | `tessellate_*`（TIN 首选 OGR）→ layout；geo 不依赖 UI/RHI |

## 类型

| 类型 | 头 | 用途 | OGR 映射 | dim |
| --- | --- | --- | --- | --- |
| `OGRTriangulatedSurface` | `ogr_geometry.h` | **OGC TIN** | `wkbTIN` + `OGRTriangle` | 实例 2\|3 |
| `plugin::detail::OrthoLattice` | `plugin/product/world3d/grid/orthogrid/lattice/ortho_lattice.h` | 2D XY 格网缓冲 | `OGRMultiPoint` + nx/ny | 2 |
| `HexLattice` | `plugin/product/world3d/grid/hexgrid/lattice/hex_lattice.h`（非导出） | 3D 结构化六面体结点；**不在** `gis.dll` | `OGRMultiPoint` XYZ + nx/ny/nz | 3 |
| `geo::NodeField2d` / `NodeField3d` | `grid/laplace.h` | 求解器用的扁平结点视图 | 非几何 | 2 / 3 |

批量写入优先 `std::span`（仍保留指针+count 重载）。`fill_envelope` 见 `ops/geometry_traits.h`；薄 TIN 写入见 `ops/indexed_tin.h`。

## 文件

| 路径 | 内容 |
| --- | --- |
| `ops/geometry_traits.h` | `k_ok` / `k_fail` + `fill_envelope` / `fill_envelope3d` + `geometry_traits` |
| `ops/indexed_tin.h` / `vector_traits.h` / `buffer.h` | TIN write + `vector_traits` + `buffer()` |
| `proj/coordinate_transform.h` | PROJ 9 RAII 封装；详见 [`proj/README.md`](proj/README.md) |
| `tin/delaunay.h` | `geo::delaunay` / `delaunay_constrained`（`geos_c`）；详见 [`tin/README.md`](tin/README.md) |
| `grid/laplace.h` | `geo::solve_laplace` / `geo::solve_elliptic`；详见 [`grid/README.md`](grid/README.md) |
| `grid/orthogonality.h` | `geo::compute_orthogonality` / `sample_orthogonality_raster` |

## 用法

```cpp
#include "ogr_geometry.h"
#include "gis/geo/ops/buffer.h"
#include "gis/geo/ops/indexed_tin.h"
#include "gis/geo/proj/coordinate_transform.h"
#include "gis/geo/tin/delaunay.h"
#include "gis/geo/grid/laplace.h"
#include "gis/geo/grid/orthogonality.h"

OGRPoint pt(116.0, 39.0);
OGRGeometry* buffered = geo::buffer(pt, 100.0);  // ogr_geometry_like only

OGRTriangulatedSurface tin;
OGRPoint a(0, 0, 1), b(1, 0, 1), c(0, 1, 1);
geo::add_patch(&tin, a, b, c);
geo::delaunay(&tin, points, count);  // GEOS XY Delaunay → OGC TIN

geo::NodeField2d field{nx, ny, xs, ys};
geo::solve_laplace(field, is_unknown);
geo::solve_elliptic(field, is_unknown, /*iters=*/4);
const geo::Orthogonality2d orth2 = geo::compute_orthogonality(field);
geo::NodeField3d vol{nx, ny, nz, xs, ys, zs};
geo::solve_laplace(vol, is_unknown);  // 7-point; 3D hex nodes live in plugin HexLattice
const geo::Orthogonality3d orth3 = geo::compute_orthogonality(vol);
```

GN：`//src/gis/geo:geo` / `:proj` / `:tin` / `:grid` → `//src/gis:gis`（`gis_d.dll` / `gis.dll`）。测试：`ops_test`、`indexed_tin_test`、`proj_test`、`tin_delaunay_test`、`geo_grid_laplace_test`。
