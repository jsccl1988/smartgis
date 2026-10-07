<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Algorithm-layer modernization（几何体 ⊥ 算法 · gdal_sdk GEOS + PROJ 9）

**Status:** accepted  
**Date:** 2026-09-13  
**Updated:** 2026-10-05 — vista terrain bake calls `horn_lambert_shade_grid` / `fill_ring_mask` in `gis/analysis`（RGBA、`DemRaster`、Thrust 留在 vista）。Prior same day — scene `vector_traits` canonical in `base/math/traits`（`geo::` 再导出）。Prior: 2026-10-04 — fold `gis/geo/mesh` into `gis/geo/ops`（`copy_envelope` / `k_ok` 在 `ops/geometry_traits.h`；`geo_mesh_test` 在 `ops/mesh_test.cc`）。Prior same day — P5：`geo::HexGrid` 从 `gis.dll` 删除；3D 结构化格网是 `OGRMultiPoint` XYZ + nx/ny/nz，由 `plugin/product/world3d/hexgrid` 拥有（`HexLattice` 非导出）。`geo::Grid` 保留（2D Feature sidecar）。`mesh_codec` 不再特化 HexGrid。不恢复 `SmtHexGrid`；不把 lattice 放回 `gis/geo` 几何头。`gis/geo/grid` 承接结构化椭圆光滑（`geo::solve_laplace` 2D 五点 / 3D 七点；`geo::solve_elliptic` 2D Thompson P=Q=0）与正交性热力（`geo::compute_orthogonality` 2D 结点+单元 |90−θ|、3D 单元 skew；`geo::sample_orthogonality_raster` 轴对齐双线性采样）。插件不再保留 `GridField` / `VolumeField` / 稀疏装配副本。不并入 `tin/`。TFI / 聚类 P,Q / Thomas–Middlecoff / 3D Thompson 未实现。`gis/geo/tin` 仅为 `geo::delaunay`（`geos_c` 2D）；无 `tin::` 命名空间、无 traits 后端、无 3D stub、无 XYZ、无 Div/Inc。P4：OGC TIN 身份锁定为 `OGRTriangulatedSurface` + `OGRTriangle`（`wkbTIN`）；`geo::Tin` / `geo::Surface3d` 弃用门面（索引 staging 不是 OGR 类型）；`geo::Grid` 保留。`gis/geo/proj` 仅保留 `geo::CoordinateTransform` 薄封装（`<proj.h>` 隔离在 `.cc`）；删除 `Projection` / `proj_backend_traits` / `proj_runtime` / `gaussprj`。  
**Tree:** 源码在 `src/gis/{model,datasource,style,tile,geo/{ops,proj,tin,grid},stat,analysis}`，链入 `gis.dll`（`//src/gis:gis`；`//src/gis:algorithm` / `//src/gis/geo:geo` 仅转发）。CPU MapFrame / tessellate 在 **`src/vista`**（`vista.dll`），见 RHI **§Vista subdirectory tighten**。顶层 `src/algorithm/**` 为 **pre-move** 叙述，勿重建。  
**Related:** 模型/渲染/计算伞状 → [`2026-09-13-render-rhi-scene-design.md`](2026-09-13-render-rhi-scene-design.md)；图层/Feature/WKB → [`2026-09-13-gdal-layer-management-design.md`](2026-09-13-gdal-layer-management-design.md)；Python 双运行时 → plugin-host。As-built：[`src/gis/geo/README.md`](../../../src/gis/geo/README.md)、[`src/gis/geo/proj/README.md`](../../../src/gis/geo/proj/README.md)、[`src/gis/geo/tin/README.md`](../../../src/gis/geo/tin/README.md)、[`src/gis/geo/grid/README.md`](../../../src/gis/geo/grid/README.md)、[`docs/superpowers/src-layout.md`](../src-layout.md)。  
**Diagram（主）：** [`../diagrams/gis-geo-layers.html`](../diagrams/gis-geo-layers.html) — Types / Traits / Ops / Codec / Present 分层与原理流  
**Diagram（补）：** [`../diagrams/gis-algorithm-geometry-split.html`](../diagrams/gis-algorithm-geometry-split.html) — 几何体 ↔ 算法依赖箭头与 P0–P5  
**Plans:** [`../plans/2026-09-13-algorithm-layer-oss.md`](../plans/2026-09-13-algorithm-layer-oss.md)（历史勾选；新工作只改本文 P1–P5）  
**Scope:** 锁定几何体层 vs 算法层契约；包装已随 `gdal_sdk` 交付的 GEOS C API 与 PROJ 9；不在本文实现 C++。

---

## 1. 目标与范围

**目标：** 停止自建第二套几何核、第二套投影引擎，以及「不是算法」的包。产品只有一条几何故事——**OGR 矢量是唯一几何所有权**；OGC TIN 走 `OGRTriangulatedSurface`（`wkbTIN`）。`geo::Grid` 是 **2D 计算缓冲**（格网形状是 metadata）。3D 结构化 hex **不是** `gis.dll` 对等缓冲：由 `plugin/product/world3d/hexgrid` 的非导出 `HexLattice` 持有结点，身份是 `OGRMultiPoint` XYZ + nx/ny/nz。算法经 traits/concept 分发，几何头不得反向依赖算法实现。

| 在范围内 | 说明 |
| --- | --- |
| OGC 矢量 | 直接用 `OGRGeometry` 子类；实例 `getCoordinateDimension()` = 2\|3 |
| Mesh | `geo::Grid`（`gis/geo/grid/grid.h`，2D）；3D hex 在插件 `HexLattice`（非导出） |
| Traits / ops | `geometry_traits` / `vector_traits`；`geo::buffer`；`geo::delaunay`；`geo::CoordinateTransform` |
| 分析内核 | `src/gis/analysis/**`；Python 经 `plugin::run_builtin_op` 转发 |
| OSS 包装 | 同一 `third_party/.install`（gdal_sdk）前缀上的 GEOS / PROJ 9 |

**已落地（不再展开实施清单）：** math/math3d/geo3d 并入 geo；自写 Gauss/Lambert 删除；DEM 不再是核心算法 DLL；chart 迁出 algorithm；实现编入 `gis.dll`。细节见 §7。

---

## 2. 几何体层

**一句话：** 几何体层拥有 **OGR 类型与存储**；mesh 只提供可写入的计算缓冲并可查询 dim / envelope；薄 codec 只做互操作视图；**不**内嵌 buffer / Delaunay / 投影 / GEOS。

### 2.1 所有权

| Lane | Owner | 公共面 | 不拥有 |
| --- | --- | --- | --- |
| **OGC** | GDAL/OGR | `OGRGeometry` / Point / LineString / Polygon / Multi* / GeometryCollection / `OGRTriangulatedSurface`；实例 dim 2\|3 | 产品内第二套虚几何树；`using SmtGeometry = OGRGeometry` |
| Mesh（计算缓冲） | `geo::`（`grid/grid.h` + `grid.cpp`） | `Grid`（XY `OGRMultiPoint` + rows/cols） | 几何实例所有权；`HexGrid` / lattice 回 `geometry.h`；类内 `buffer`/Delaunay/投影；平行顶点/面表；把 TIN 身份做成产品类型 |
| 3D structured hex | `plugin/product/world3d/hexgrid`（`HexLattice` 非导出） | `OGRMultiPoint` XYZ + nx/ny/nz metadata；VTK `.vts` 写出 | `geo::HexGrid` in `gis.dll`；`mesh_codec` HexGrid 特化；`SmtHexGrid` |
| **Vector math** | `base/math`（ns `base`） | `Vector2`/`3`/`4`；`base::vector_traits` compile-time `dimension` | OGC 几何身份 |
| **Traits 描述面** | `geo::`（`ops/geometry_traits.h`）；场景向量 traits 再导出 `geo::vector_traits` | 偏特化 + `geometry_like` / `ogr_geometry_like`；`fill_envelope` | 算法状态；平行 `struct mesh_traits` |
| **薄 codec** | `geo::`（`ops/indexed_tin.h`） | `add_patch` / `fill_indexed_tin` / `fill_tin_from_ogr(OGRTriangulatedSurface*)`（**无** HexGrid 特化） | 重写 WKB/WKT；Feature 持久化；`HexGrid` 特化 |
| **Feature / WKB** | datasource（`ogr_feature_codec`） | Feature↔`geo::Grid` + OGR WKB/WKT；TIN 长期应写在 `OGRFeature` 几何上 | `Feature::tin_` sidecar（P4 leftover） |
| **Present** | `vista` / `content` / `effect` | `tessellate_*`；frame layout（`OGRGeometry*` → DrawItem） | 反向依赖 kernel←UI/RHI |
| **Legacy** | leftover 直写 `geo::` / OGR | 无 `SmtTin`/`SmtGrid`/`Smt3DSurface`/`SmtHexGrid` typedef | 恢复别名 |

**Namespace（locked）：** 公共两层 `geo`（helpers 进 `geo::detail` 或匿名命名空间）。目录 `gis/geo/{ops,proj,tin,grid}` **不**升第三语义命名空间、**不**用 `namespace tin` / `namespace grid`。CRS 入口是 `geo::CoordinateTransform`。结构化格网入口是 `geo::solve_laplace` / `geo::solve_elliptic` / `geo::compute_orthogonality`。

**dim（locked）：** OGC `coordinateDimension` = 实例 2\|3（`getCoordinateDimension()`）；拓扑 `getDimension()` 不是 traits 的 `coordinate_dimension`。Compile-time `dimension` 只在 `VectorN` / `vector_traits`。无 `Geometry2`/`Geometry3`。

### 2.2 几何体依赖方向（locked）

```
ops/geometry_traits.h  →  OGR / gis::Envelope / base 类型
```

**不得** `#include`：`buffer.h`、`tin/delaunay.h`、`grid/laplace.h`、`coordinate_transform.h`、`analysis/**`、UI/RHI。  
（现状：`ops/geometry_traits.h` 无 ops 实现反向 include。）

### 2.3 Present 边界

- `gis/geo` **禁止**依赖 `ui/`、`content/`、`gpu/`、RHI、Views。
- vista 拥有 tessellate 与 layout；content present 只消费布局结果；不重新定义 Grid/Tin。

---

## 3. 算法层

**一句话：** 算法层是纯 ops + backend traits；只读 traits/concept；写出到调用方拥有的几何实例；不拥有存储，不依赖 present。

### 3.1 所有权

| 职责 | Owner | 入口 | 不拥有 |
| --- | --- | --- | --- |
| OGC 空间算子 | `geo::`（`ops/buffer.h` / `.cc`） | `geometry_like`；收紧后 `ogr_geometry_like`；GEOS **XY**，结果恢复实例 dim | 结点表；图层会话 |
| Mesh helpers | `fill_envelope`、`indexed_tin` | `ogr_geometry_like` / OGR TIN | 三角化本体；`Matrix2D` 求解（`analysis/grid`） |
| Delaunay / CDT | `geo::`（`gis/geo/tin/delaunay.h`） | `geo::delaunay` / `delaunay_constrained` → `geos_c`；写出 `OGRTriangulatedSurface*` | Tin 类型定义；自写 incremental / 3D tet / `namespace tin` |
| Structured elliptic | `geo::`（`gis/geo/grid/laplace.h`、`orthogonality.h`） | `geo::solve_laplace`（2D 五点 / 3D 七点）+ `geo::solve_elliptic`（2D Thompson，P=Q=0）+ `geo::compute_orthogonality`（2D 结点+单元 |90−θ|；3D 单元 skew）+ `geo::sample_orthogonality_raster`；`NodeField2d`/`NodeField3d` 扁平视图或 `Grid` 重载（3D 走 `NodeField3d` / 插件结点，不经 `HexGrid`） | TFI；聚类 P/Q；Thomas–Middlecoff；3D Thompson；`namespace grid`；并入 `tin/`；`geo::HexGrid`；插件内稀疏装配副本 |
| CRS 变换 | `geo::`（`gis/geo/proj/coordinate_transform.h`） | `CoordinateTransform` / `transform_xy`；`gis::CrsId` 仅名字 | 几何存储；`PJ*` 不进图层；无 traits 剧场 |
| 分析内核 | `gis::analysis`（`ops/` + `geometry/` / `network/` / `raster/{dem,filter}/`） | `OpsRunner` / `native.*` | 平行 Shapely；UI 面板 |
| 产品转发 | `plugin/runtime/processing` | `plugin::run_builtin_op` | 内核实现副本 |

**Ops 约束（locked）：** `geo::buffer` =「`geometry_like` 且可绑定 `const OGRGeometry&`」。OGR TIN **不**走 buffer；需 OGC 算子时先 `indexed_tin`。勿再引入平行 `struct mesh_traits<M>` 或 `mesh_like`（已并入 `fill_envelope` + `ogr_geometry_like`）。

### 3.2 Python / Eigen（保留锁定，细节不重复）

- **Python：** Console/Panel 空间分析必须走 `plugin::run_builtin_op`。实现在 `src/gis/analysis/`；`plugin` 只转发。禁止平行 Shapely / 第二张 C++ 算子表。Plan：[`../plans/2026-09-28-gis-python-spatial-analysis.md`](../archive/plans/2026-09-28-gis-python-spatial-analysis.md)。
- **Eigen：** `analysis/{geometry,network,raster/{dem,filter}}` 与 `gis/geo/grid` 可用已有 `//third_party:eigen`（与 `base/math` 同栈）。不另 vendor glm/第二份 Eigen。structured 求解器编进 `gis.dll`。analysis 公开面 `gis::detail` + `native.*`；测试 `analysis_eigen_test`。

| Area | Path | 代表 ops | Eigen |
| --- | --- | --- | --- |
| Geometry | `geometry/` | `fit_line_2d` / `fit_plane_3d` / `affine_align_2d` | Dense SVD/QR |
| Raster DEM | `raster/dem/` | `dem_gradient` / `flood_fill` / `horn_lambert_shade` | `Map` 差分；Horn 朗伯因子 |
| Raster mask | `raster/mask/` | `point_in_ring` / `fill_ring_mask` | 偶奇环；无 Eigen |
| Raster filter | `raster/filter/` | `raster_convolve` / `raster_smooth` | 卷积；SparseLU |
| Network | `network/` | `cost_path` | 小稠密矩阵 |
| Structured grid | `gis/geo/grid/` | `solve_laplace` / `solve_elliptic` / `compute_orthogonality` | Sparse LDLT / SparseLU |

### 3.3 Structured grid elliptic（`gis/geo/grid`）

**一句话：** 结构化格网光滑与正交性度量是 **mesh-generation 家族的 sibling**（相对 `tin/` 非结构化 Delaunay），公开面是 `geo::solve_laplace`、`geo::solve_elliptic`、`geo::compute_orthogonality`、`geo::sample_orthogonality_raster`。目录 `geo/grid` 不创造 `namespace grid`。Eigen Sparse 留在 `laplace.cc`；公开头不 include Eigen。`geo::Matrix2D` 是 leftover orthogrid 数值缓冲（`legacy/plugin/product/orthogrid/kernel/matrix2d.h`），不是产品 geo 类型、不是第三套几何。

orthogrid / orthogrid3d 插件只保留边界数字化、VTK / MapScene 写出，并 **调用** `geo::solve_*` / `geo::compute_orthogonality`（结点视图为 `NodeField2d`/`NodeField3d`，无插件 `GridField`/`VolumeField`）。StyleDocument 的 heat 字符串格式化留在 shell writer。**本切片不实现** TFI、聚类控制函数 P/Q、Thomas–Middlecoff、3D Thompson。

测试：`geo_grid_laplace_test`（单位正方形双线性恢复、过小格网拒绝、矩形上 elliptic≈Laplace、单位盒子 3D Laplace、矩形正交性≈0、轴对齐 raster、单位盒子 3D cell skew）。插件 `orthogrid_laplace_test` / `orthogrid3d_laplace_test` 只覆盖 gridbnd / hex 产品流。

### 3.4 Vista 地形烘焙调用的栅格核

**一句话：** `vista/terrain` 的山体阴影和陆地掩膜只保留呈现；数值核在 `gis/analysis`，`gis` 不依赖 `vista`。

| 核 | 路径 | Vista 仍拥有 |
| --- | --- | --- |
| Horn Lambert 因子 | `gis/analysis/raster/dem/hillshade.h`（`horn_lambert_shade` / `horn_lambert_shade_grid`） | `shade_dem_rgba` 的 RGBA、对比度、海洋 alpha、`DemRaster`、`BAKE_*` |
| 偶奇环掩膜 | `gis/analysis/raster/mask/ring_mask.h`（`point_in_ring` / `fill_ring_mask`） | `LonLatRing`、bbox 预处理、烘焙时钟 |

`dem_gradient` 的坡度/坡向（度、GeoTIFF）与 Horn 着色因子不是同一公式，不合并。CUDA Thrust（`vista/terrain/dem/nv/thrust_gis.cu`）与 CPU AVX2 lit pack（`vista/terrain/dem/shade/lit_*.cc`）是烘焙侧加速；公式与 `gis::detail::horn_lambert_shade*` 对齐，不进 `gis.dll`。无新的 `native.*` / `OpsRunner` 入口。测试：`analysis_terrain_kernel_test`。

---

## 4. 分离契约与依赖方向

**契约：** 几何体 ⊥ 算法实现——算法依赖 traits/concept 与类型头；几何头永不 `#include` 算法实现。

```
analysis / geo::ops / geo::delaunay / geo::solve_laplace / geo::compute_orthogonality / geo::CoordinateTransform
        │
        ▼
  traits + concepts
  (geometry_like / mesh_like / vector_traits / *_backend_traits)
        │
        ▼
  OGRGeometry（唯一几何所有权）
  geo::Grid           （2D 计算缓冲；写出到调用方 / clone_ogr）
  3D hex              （plugin HexLattice；OGRMultiPoint XYZ + nx/ny/nz；非 gis.dll 类型）
```

| 允许 | 禁止 |
| --- | --- |
| 算法 `#include` mesh 头（写出 OGR TIN / 读 `as_ogr`） | 几何 `#include` `geo_ops` / tin·grid·proj backend / `analysis/**` |
| 算法读 traits、写调用方实例 | 算法 `#include` vista present / `content/.../present` / `ui/views` / gpu / RHI |
| mesh → OGR 薄视图 | 几何类内嵌 `GEOS_*` / buffer / Delaunay / 投影 / Laplace |
| — | `Geometry2`/`3`、`buffer2d`/`3d`、`Foo2d`/`Foo3d`；空 `geos_backend_traits` 剧场头；geo 内重写 WKB |

### 原理流（构造 → 分发 → 边界）

```
[构造]  OGR*  几何实例  或  geo::Grid 计算缓冲（3D hex 仅插件 HexLattice）
   │
   ▼
[Traits]  geometry_traits<G>::coordinate_dimension(g)   // 实例 2|3
          vector_traits<V>::dimension                   // 仅 VectorN
   │
   ├── geometry_like + OGR ──► geo::buffer / OGR 谓词（GEOS 在 GDAL；Z 不进 GEOS）
   ├── mesh_like ───────────► fill_envelope / mesh_codec
   ├── Vector3* ────────────► geo::delaunay（GEOS XY）→ OGRTriangulatedSurface
   └── CRS 字符串 ──────────► geo::CoordinateTransform
   │
[边界]  datasource Feature codec · vista tessellate · analysis OpsRunner
```

**GN：** `geo_sources` = mesh + ops；`proj_sources` / `tin_sources` 同在 `gis/geo`（独立 source_set）；**不**把 `analysis` 编进 `geo_sources`。`//src/gis:algorithm` → `gis`（非独立算法 DLL）。

---

## 5. 升级阶段（P0–P5）

合并原 §Kernel 待收敛项与 §分离 Phase checklist；勾选状态保留。

### P0 — 文档锁定（本变更）

- [x] Living 单一叙事 + 分层图（主）/ 分离图（补）
- [x] As-built README / `src-layout` 点明「几何体 ⊥ 算法」

### P1 — Concept 收紧（优先）

| 项 | 动作 |
| --- | --- |
| [x] `ops/geometry_traits.h` | 增加 `ogr_geometry_like`（`geometry_like` ∧ `std::derived_from<G, OGRGeometry>`） |
| [x] `geo::buffer` | 约束改为 `ogr_geometry_like`（mesh 误实例化 → 编译失败） |
| [x] `mesh_traits.h` | 标明 mesh **不**走 buffer；需 OGC 算子先 codec |
| [x] `ops_test` / `indexed_tin_test` | 断言 buffer 仅 OGR；TIN 走 `indexed_tin` |
| [x] `ops/geometry_traits.h` | envelope / `k_ok` 并入 traits 头；存储只走 OGR；禁止新算法成员 |

### P2 — 物理边界硬化

| 项 | 动作 |
| --- | --- |
| [x] `geo_ops.cc` | GEOS 留在 ops（未拆 `ops/detail/`；无行为需要） |
| [x] tin / proj | `geo::delaunay` 写出 `OGRTriangulatedSurface*`；proj 不持有 Geometry |
| [x] `analysis/**` | 无 vista/present include；`fit_line_2d`/`fit_plane_3d` 增加 `ogr_geometry_like` 入口 |
| [x] `ogr_feature_codec` | `decode_smt_tin` / `decode_smt_grid` 走 `mesh_codec`（`tin_from_ogr` / `grid_from_ogr`） |
| [x] GN | 保持 mesh+ops；analysis 不进 `geo_sources` |

### P3 — 命名与 leftover 收敛

| 项 | 动作 |
| --- | --- |
| [x] `ALGORITHM_*_H_` | → `GIS_*`（tin/proj/stat 残留头） |
| [x] `SmtTin`/`SmtGrid`/… typedef | 已删；leftover 写 `OGRTriangulatedSurface` / `geo::Grid` |
| [x] Matrix2D / orthogrid PascalCase | snake_case 为唯一面；`Matrix2DLegacyApi` mixin 已删；orthogrid 改 `get_element`/`set_element`/`elements()` |
| [x] 文档路径 | `src/algorithm/...` 仅历史；清单只列 `src/gis/{geo/{mesh,ops,proj,tin,grid},stat,analysis}` |

### P4 — 原生 OGR TIN（本变更）

**锁定：** OGC TIN 身份 = `OGRTriangulatedSurface` + `OGRTriangle`（`wkbTIN`）。无产品 `geo::Tin` / `Surface3d` 门面。索引写入走 `indexed_tin`（`add_patch` / `fill_indexed_tin`）。

**保留：** `geo::Grid`（2D Feature sidecar）。OGR 没有 Grid 类型（`OGRMultiPoint` + rows/cols）。3D hex **不**再作为 `gis.dll` 计算缓冲（见 P5）。

| 项 | 动作 |
| --- | --- |
| [x] `grid/grid.h` | `geo::Grid` 公开头；envelope / `k_ok` 在 `ops/geometry_traits.h` |
| [x] `ops/indexed_tin.h` | `add_patch` / `add_triangle` / `fill_indexed_tin` / `fill_tin_from_ogr(OGRTriangulatedSurface*)` |
| [x] `geo::delaunay` / `delaunay_constrained` | `OGRTriangulatedSurface*` |
| [x] `tessellate_tin` / `tessellate_3d_surface` | `OGRTriangulatedSurface*` |
| [x] 测试 | `indexed_tin_test` / `tin_delaunay_test` / `ops_test` 走原生 TIN |

**Feature TIN：** 作为 `OGRTriangulatedSurface` 存在 `OGRFeature` 上，无 `Feature::tin_` sidecar。

**Don't：** 不恢复 `SmtTin` typedef；不引入 `Geometry2`/`Geometry3`；不删除 `geo::Grid`；不把 `HexGrid` 留在 P4 公共面（P5 删除）。

### P5 — HexGrid 退出 gis.dll（本变更）

**锁定：** `geo::HexGrid` 从 `gis.dll` / `gis/geo` **删除**。3D 结构化六面体格网由 **`plugin/product/world3d/hexgrid`** 拥有：内存面是 **非导出** `HexLattice`；交换 / Feature 身份是 **`OGRMultiPoint` XYZ + nx/ny/nz** metadata。`geo::Grid` **保留**（2D Feature sidecar）。`mesh_codec` **不再**特化 HexGrid。格子类型 **不**回到 `ops/geometry_traits.h`。

| 项 | 动作 |
| --- | --- |
| [x] 文档 | Living §2 / 原理流 / 图：Grid 是唯一 gis.dll 格网缓冲；3D hex 仅插件 |
| [x] C++（88b40857） | 已删 `HexGrid` / `hex_grid.cpp` / `mesh_codec` HexGrid 特化；orthogrid3d 拥有 `plugin::detail::HexLattice`（`OGRMultiPoint` + nx/ny/nz） |

**Don't：** 不恢复 `SmtHexGrid`；不把 lattice 放回 `geometry.h`；不把 3D hex 做成与 `Grid`/`Tin` 对等的 `gis.dll` 计算缓冲。

### 已落地（mesh cutover）

1. [x] Traits + span + envelope（`mesh_traits.h`、`ops/geometry_traits.h`、`geo_mesh_test`）
2. [x] leftover `geo::Matrix2D` + `std::vector`（`legacy/.../orthogrid/kernel/matrix2d.h`；非产品 geo）
3. [x] Legacy `DrawTin`/`DrawGrid` 主路径改 `geo::`
4. [x] 薄 `ops/indexed_tin.h`

### Done bar

- `ops/` 几何头无算法实现 include  
- `geo::buffer` 无法对 `Grid`/`Tin` 实例化  
- `analysis` / `tin` / `proj` 无 UI/present include  
- `ops_test` + `indexed_tin_test` + tin/proj 相关测试绿  
- 新代码写出 `OGRTriangulatedSurface`；无 `geo::Tin` / `Surface3d` 门面  
- 无 `geo::HexGrid` 作为 gis.dll 计算缓冲（3D hex 仅插件）  
- 无新平行 2D/3D API  

### Leftover / `Smt*`

| 阶段 | 策略 |
| --- | --- |
| Now | **已删** mesh `Smt*` typedef 与 Matrix2D PascalCase mixin；leftover 写 `geo::` / OGR |
| Mid | leftover 写 `geo::delaunay`；无 Div/Inc、无 `namespace tin` |
| End | 历史 DLL stem 见 abi-rename-map；TIN 在 `OGRFeature` |

---

## 6. OSS / 第三方归属

| 库 | 归属 | 规则 |
| --- | --- | --- |
| **GEOS** | `gdal_sdk` → `geos_c`；经 `geo_ops.cc` 与 `tin/delaunay.cc` | 不第二份 GEOS；GEOS XY-only，Z 留在产品实例 |
| **PROJ 9** | `gis/geo/proj` only（`#include <proj.h>` 仅 `.cc`，非 `proj_api.h`）；过程级 `PJ_CONTEXT` + `std::mutex`；公开面 `geo::CoordinateTransform` | 不进 `sdb/crs`；数据目录用 SDK 旁 `PROJ_DATA`/`PROJ_LIB`；无 Gauss 类层次、无 `proj_backend_traits`、无 `geo::Projection` |
| **GDAL/OGR** | `//third_party:gdal`；矢量类型 + Feature/WKB | 插件 DEM 高度图用 GDAL 打开，不恢复 `SmtDemCore` |
| **CDT** | 不单独 vendor | GEOS 已提供 `GEOSConstrainedDelaunayTriangulation_r`；勿再加 `third_party/cdt` / Triangle / CGAL |
| **Eigen** | 已有 `//third_party:eigen` | analysis + `gis/geo/grid` 可依赖；不另 vendor |
| **安装前缀** | `third_party/.install`（`out/third_party` junction） | 无第二 install root |

TIN：**只包装 `geos_c`。** 2D 点集 → `GEOSDelaunayTriangulation_r`（`geo::delaunay`）；约束环 → `GEOSConstrainedDelaunayTriangulation_r`（`geo::delaunay_constrained`）。不提供 3D 四面体、不 vendor TetGen/CGAL、不保留 `namespace tin` / traits 后端 / XYZ 解析器 / Div/Inc。`analysis/geology/stratum_tin` 走 `geo::delaunay_triangles`。

错误面（保持既有约定）：DLL 边界不抛异常；PROJ 失败返回 `false`；GDAL/XYZ 失败返回 `ERR_*` 并打日志；缺 `geos_c.h`/`proj.h` 则编译失败，不做软件 Gauss 回退。

---

## 7. 不做什么

1. 不 vendor 第二份 GEOS / PROJ / glm / Eigen / CGAL / Triangle / SOCI / Qt（含 Qt Charts）。  
2. 不引入 `Geometry2`/`Geometry3`、`buffer2d`/`buffer3d`、平行 `Foo2d`/`Foo3d`。  
3. 不在 Geometry 上放 compile-time `dimension`（属 Vector / traits）。  
4. 不重建顶层 `src/algorithm/**` 源树或独立「算法 DLL」假象（实现在 `gis.dll`）。  
5. 不在 `sdb/crs` 加投影数学（仅 `CrsId` 名字）。  
6. 不恢复 `SmtDemCore`；不在 `gis/geo` 重写 WKB/WKT。  
7. 不在本伞状实现 Views+Skia charts；不改 `content/public`。  
8. 几何类不内嵌算法；算法不 `#include` present/UI。  
9. 不恢复空的 `geos_backend_traits` 剧场；Python 侧不平行 Shapely。  
10. 不恢复 `SmtTin`/`SmtGrid`/`Smt3DSurface`/`SmtHexGrid` typedef。  
11. 不在 `tin/` 或 `analysis/geology` 恢复自写 Delaunay / 四面体化。  
12. 不在 `tin/` 放结构化椭圆求解；不在本切片实现 TFI / P,Q 聚类 / Thomas–Middlecoff / 3D Thompson。  
13. 不恢复 `geo::HexGrid`；不把 3D lattice 放回 `gis/geo` 几何头；`mesh_codec` 不特化 HexGrid。

（RHI / map2d frame / atmosphere 细节 → render-rhi-scene umbrella，本文不复制。）

---

## 8. 附录：已落地历史迁移（2026-09 压缩）

下列为 **已完成** 的 OSS 现代化结果，供对照旧路径；实施 agent 分区与空 checklist 已删除。

| 历史 | 今日 |
| --- | --- |
| `src/algorithm/{geo,proj,tin,baogrid,stat}` + 独立 DLL | `src/gis/geo/{mesh,ops,proj,tin,grid}` + `gis/stat` + `analysis/**` → `gis.dll` |
| `SmtMathLib` / `Smt3DMathLib` / `Smt3DGeoCore` | 吸收进 geo；场景向量在 `base/math`（Eigen） |
| 自写 Gauss/Lambert | 删除（含 `api/gaussprj.cpp`）；仅 PROJ 9 `geo::CoordinateTransform` |
| `algorithm/dem` / `SmtDemCore` | 删除；`plugin/dem` 用 GDAL + tin |
| `algorithm/chart` | 迁出（legacy UI chart / `SmtStaDiagram`）；数据仍在 stat |
| 早期「traits 全删」表述 | **已取代**：C++23 traits+concepts 为锁定公共面（见 §2–§3） |

**Success（历史门槛，仍有效）：** 无第二 GEOS/PROJ；无平行 Geometry2/3；`build.bat te` 含 proj/tin 与 geo 测试；chart/dem 不在算法核心包。

**测试锚点：** `ops_test`、`indexed_tin_test`、`proj_test`、`tin_delaunay_test`、`geo_grid_laplace_test`、`analysis_eigen_test`；vista/effect 既有 tessellate / unified draw。

**Plan：** [`../plans/2026-09-13-algorithm-layer-oss.md`](../plans/2026-09-13-algorithm-layer-oss.md)（历史勾选；新工作只改本文 P1–P5）。
