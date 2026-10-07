<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# GDAL 作为唯一图层管理后端

**Status:** accepted  
**Date:** 2026-09-13  
**Updated:** 2026-10-05 — `testing/data` pack split (`china/` vs `plugin/` vs `fixtures/`; GN still flattens `out/data/`). Prior 2026-10-04 — Nest `gis/style/{document,eval,symbol}`（根上留 leftover-stable `paint_resolve.h`；无转发头）。Prior same day — Hoist `gis/carto/{style,tile}` → `gis/style` + `gis/tile`（删空 `carto/`；不拆进 datasource/map；无转发头）。Prior same day — Nest `gis/tile/{protocol,cache,provider,layer}`（撤销同日 flatten；无转发头）。Prior same day — Tighten `src/gis` dirs: drop `layer/` `crs/`; datasource backends `ogr/` `sdbd/` `gdal/` as siblings of `provider/` (no `impl/`); flatten `stat/{detail,eval,value}`; leftover `Style` OGR blob I/O in `legacy/gis/present/carto/smt_style_ogr.*`. Prior same day — Flatten `gis/model/*` → `gis/{feature,map,edit,envelope.h}`；`edit/` 按职责拆 mutation / undo_log / command / memory / map session；leftover catalog `CatalogSource` / `FeatureAdapter`。Prior same day — Task 5: 产品 `gis::Map`；PascalCase Feature → leftover `FeatureAdapter`；`CatalogSource`/`*Info`/`leftover_layer_feature_type` → `legacy/gis/layer/layer.h`。`copy_envelope` 在 `gis/geo/ops/geometry_traits.h`。Prior same day — **§ gis/model product surface vs leftover + OGR Map/Layer/Feature**。Prior same day — CPU MapFrame / World 在 **`src/vista`**（`vista.dll`）。Prior 2026-10-03 — product Style/tile under `gis/{style,tile}`。  
**Diagram:** [`../diagrams/gis-vista-architecture.html`](../diagrams/gis-vista-architecture.html)（浅色 SVG：GIS 泳道 + LayerBatch→present 流水线；边界 `gis` ↛ `render/rhi`）· 瓦片子目录 [`../diagrams/gis-carto-tile.html`](../diagrams/gis-carto-tile.html) · vista 子目录收紧图 [`../diagrams/vista-subdirectory-layers.html`](../diagrams/vista-subdirectory-layers.html)（RHI umbrella §，非本文件新 spec）· **model 终局** [`../diagrams/gis-model-ogr-layers.html`](../diagrams/gis-model-ogr-layers.html)（产品 `Map`/`Layer`/`Feature` ↔ OGR；leftover ABI 迁出）  
**Plans:** Session+Provider [`../plans/2026-09-28-datasource-session-provider.md`](../plans/2026-09-28-datasource-session-provider.md)（含 **Task 5** leftover-ABI 迁出核对）· OGR DB [`../plans/2026-09-13-ogr-db-datasource.md`](../plans/2026-09-13-ogr-db-datasource.md) · sdbd [`../plans/2026-09-19-sdbd-wsl-client.md`](../plans/2026-09-19-sdbd-wsl-client.md)
**Scope:** 图层的打开 / 创建 / 列举 / 编辑 / 查询 / 关闭一律走 GDAL Dataset / Layer（矢量）或 GDAL raster（栅格）。本文件管 `sdb` 数据源与图层，不管桌面 chrome。新树编排入口见文末 **§ DataSession / Provider facade**。

**Sibling (folded — see §Folded topics; revise this file):**

- 数据库路径（PostGIS / GeoPackage / SpatiaLite）→ archive [`2026-09-13-ogr-db-datasource-design.md`](../archive/specs/2026-09-13-ogr-db-datasource-design.md)。
- **2D 地图瓦片（XYZ/WMTS）** → archive [`2026-09-13-tile-layer-provider-design.md`](../archive/specs/2026-09-13-tile-layer-provider-design.md)；新需求写在本伞 `§`，不另开 dated twin。

ADO 源码删除由另一条工作流负责。本文不恢复、不重写、不阻挡那条删除。

## Goal

停止按驱动复制一套 C++ 图层子类，也**不再**用 `SmtDataSource` / `SmtVectorLayer` / `SmtFeature` 当产品 ABI。

- **事实源与调用方类型** 都是 `GDALDataset` + `OGRLayer` + `OGRFeature`（矢量）或 `GDALRasterBand` / 子数据集（栅格）。v1 **不是**适配器层。
- **sdbd 是 GDAL 驱动**（`GDALDriverManager` 名 `"SDBD"`，连接前缀 `SDBD:`，实现于 `gis/datasource/sdbd/`）。`GDALAllRegister()` + `register_sdbd_driver()`。打开：`GDALOpenEx("SDBD:MEM:name")` 或 `SDBD:GPKG:path` / `SDBD:PostgreSQL:PG:…`。mgis 没有同名 GDAL 驱动（只有 HTTP `:8021` / `sdbd://`）；本仓 `sdbd://` 在进程内落到 Memory。**无**进程内 HTTP `SdbdHandler`（已删）；远程 mogu 见 [`2026-09-19-sdbd-wsl-client-design.md`](../archive/specs/2026-09-19-sdbd-wsl-client-design.md)（`PROVIDER_SDBD` + `SdbdClient`）。
- 文件 / 库 / 内存共用这一套 GDAL 对象。Memory = GDAL Memory 驱动（经 `SDBD:MEM:` 或直接 `Memory`）。
- 驱动是否编进当前 `gdal_sdk` 是运行时问题。缺 GPKG / PG 时 `Open` 失败并打日志，不另写 C++ 读写器。

产品调用方 ABI 是 `sdb::Feature` / `sdb::MapLayer`（组合持有 `OGRFeature*` / `OGRLayer*`）；事实源与 I/O 仍是 GDAL/OGR。细节见 [`2026-09-13-sdb-feature-maplayer-composition-design.md`](../archive/specs/2026-09-13-sdb-feature-maplayer-composition-design.md)。不要再维护 `SmtAttribute` 第二套字段存储（已删除；MFC att-struct UI 改读 `OGRLayer`）。

## Non-goals

- 不要再 vendor 一份 GDAL / GEOS / PROJ。只链 `//third_party:gdal`（`third_party/.install`）。
- 不要 Qt。桌面终局是 Views + Skia；本文不改 `src/ui` / `src/app` chrome。
- 不要保留 `SmtFeature` / `SmtVectorLayer` / `SmtDataSource` 作为图层 ABI（几何算法 / `Style` 可独立留下）。
- 不要在本文周期实现 WMS / WFS / XYZ 瓦片，也不要把 `SmtTileLayer` 硬塞进 OGR。瓦片见 sibling [`2026-09-13-tile-layer-provider-design.md`](../archive/specs/2026-09-13-tile-layer-provider-design.md)；**禁止**用 OGR Memory / `SDBD:MEM` 冒充瓦片。
- 不要恢复 ADO / `msado15` / `DS_TB` / 按类型 `*Fcls` SQL。另一 agent 删 leftover ADO 文件时不要冲突。
- 不要为缺 GPKG / PG 的 SDK 手写读写器。
- 不要改 `content/public` 的稳定嵌入 API。
- 不要把 `Map` 再设计成只持 `SmtLayer*`；地图文档改持 `OGRLayer*` / `GDALDataset*`（或同等 GDAL 句柄）。

## 现状 vs 目标

### 现状（2026-09-14 树）

```
Map  (src/sdb/map)          矢量走 OGRLayer* / MapLayer；raster/tile leftover 仍 SmtLayer*
        ^
SmtDataSourceMgr
        |
        +-- 文件/库/内存  --> open_sdbd_dataset → SdbdDataset（内层 Memory / GPKG / PG / 文件）
        |                     + SdbdLayer（矢量）
        |                     + OgrRasterLayer   GDAL MEM / 文件栅格；CreaterRaster 经 /vsimem
        +-- CreateMemVecLayer --> SDBD:MEM + OGRLayer scratch
        +-- CreateMemRasLayer --> OgrRasterLayer + GDAL MEM（已离开 SmtMemRasLayer）
        +-- *(已删)* CreateMemTileLayer / SmtMemTileLayer（瓦片见 tile-layer-provider；不进 SDBD:MEM）

Select / Flash：CreateMemVecLayer()
```

阶段 4（栅格 MEM 等价）已落地：`OgrRasterLayer::Create/Open/CreaterRaster/GetRaster*` 挂真实 `GDALDataset`；编码 blob（CxImage `image_code`）存 `/vsimem`，`VSIGetMemFileBuffer` 供 render `GetRasterNoClone`。`memraslayer.cpp` / `SmtMemRasLayer` 已删。**MemTile 死工厂已切除**：`CreateMemTileLayer` / `SmtMemTileLayer` / `sde_mem` DLL / `datasource/mem` 目录已删；抽象 `SmtTileLayer` 仍在 `layer.h`，concrete 待 TileProvider。

要点（历史对照，多数设备已并入 SDBD）：

| 设备 | 图层实现 | 是否已经 OGR/GDAL | 持久化 |
| --- | --- | --- | --- |
| SDBD / `SdbdDataset` | `SdbdLayer` + `OgrRasterLayer` | 是 | GDAL |
| *(已删 MemTile)* | — | — | 见 [tile-layer-provider](../archive/specs/2026-09-13-tile-layer-provider-design.md) |
| *(已删 WS)* | — | — | 见 [tile-layer-provider](../archive/specs/2026-09-13-tile-layer-provider-design.md) |
| ADO `*Fcls` | 按要素类型子类 | 已退出 `src_all` | — |

`SmtLayer` 虚接口在 `src/sdb/layer/layer.h`（raster/tile leftover）。工具层用 `CreateMemVecLayer()` 当查询结果层。

### 目标

```
Map / tools / catalog       持 GDALDataset* / OGRLayer* / OGRFeature*
        ^
        |  GDALOpenEx / CreateLayer / CreateFeature
        v
GDALDriver "SDBD"              前缀 SDBD: ；内部再开 Memory / GPKG / Shapefile / PG
        |
        v
third_party/.install           唯一 GDAL 安装前缀（旧名 gdal_sdk）
        + Memory
        + ESRI Shapefile / GeoJSON / …
        + GPKG / SQLite (SpatiaLite)
        + PostgreSQL
        + 栅格驱动（GeoTIFF / …）

DS_WS 枚举残留                  源码树 datasource/ws 已删；打开拒绝；瓦片见 sibling tile spec
```

`eDSType` 整数值和 `.dsm` 二进制布局可暂时保留（旧工程文件），但打开路径只认 `GDALOpenEx` 目标串。管理器（若还在）对文件 / 库 / 内存都 `GDALOpenEx("SDBD:…")`，不再 new `Smt*DataSource` / `Smt*VecLayer`。

## 推荐架构（GDAL 原生，不是适配器）

**v1 产品代码直接命名 GDAL 类型。不要 `SmtDataSource` / `SmtVectorLayer` / `SmtFeature` 门面。**

| 层 | 类型 | 职责 |
| --- | --- | --- |
| 产品 GIS 模型 | `GDALDataset`、`OGRLayer`、`OGRFeature`、`GDALRasterBand` | 调用方 ABI；地图、工具、sdbd IPC 都用它们 |
| sdbd 驱动 | `GDALDriver` `"SDBD"`（`sdb/datasource/gdal`） | `Identify` / `Open` / `Create`；转发 `CreateLayer` / 要素 CRUD / 空间过滤到内部数据集 |
| 连接串 | `SDBD:`、`SDBD:MEM:name`、`SDBD:GPKG:path`、`SDBD:PostgreSQL:PG:…`、`sdbd://` | mgis 无 GDAL 驱动名；本仓锁定 `SDBD` |
| 存储 | 已注册的 GDAL 驱动 | 本仓库不实现 GPKG/PG 读写器 |

三个被否决的替代：

1. **适配器优先（`OgrDataSource` + `SmtFeature`）** — 已撤销。那是第二套存储语义。
2. **继续按格式扩 C++ 子类**（`SmtGpkgVecLayer`…）— 正是本文要结束的爆炸。
3. **sdbd 作为 GDAL 旁边的 HTTP/IPC 侧车** — sdbd 必须是 `GDALDriverManager` 里的驱动；远程 JSON 经 WSL mogu + `SdbdClient`，不是进程内第二套数据集。本地 HTTP handler 已移除（2026-09-28）。

公共命名空间仍两层：`sdb::datasource`。新辅助放 `sdb::datasource::detail`。

语言：产品 C++ 是 **C++23**（`cc_std = "c++23"`，Windows 上 `/std:c++23preview`）。不要为图层再做一棵 2D/3D 虚继承树。

## 设备管理器：一个 `OgrDataSource`

`SmtDataSourceMgr::CreateDataSource` / `CreateTmpDataSource` 对下列 type 都 `new OgrDataSource()`：

| `eDSType` | `unProvider` | GDAL 打开目标 | 备注 |
| --- | --- | --- | --- |
| `DS_DB_ADO` | `PROVIDER_GPKG` | `db.szService` + `db.szDBName` 文件路径 | 已实现 |
| `DS_DB_ADO` | `PROVIDER_SPATIALITE` | 同上，SQLite + `SPATIALITE=YES` | 已实现 |
| `DS_DB_ADO` | `PROVIDER_POSTGRES` | `PG:host=…` | 已实现；SDK 无驱动则失败 |
| `DS_DB_ADO` | `PROVIDER_ACCESS` / `SQLSERVER` | — | 继续拒绝（已测） |
| `DS_FILE_SMF` | `PROVIDER_SHAPE` | `file.szPath` + `file.szFileName`（`.shp` 或目录） | 从 `SmtSmfDataSource` 并入 |
| `DS_FILE_SMF` | `PROVIDER_OGR_SUPPORT` | 同一路径，`GDALOpenEx` 自动认驱动 | GeoJSON 等；GPKG 作**文件**打开也走这里 |
| `DS_MEM` | `PROVIDER_MEM_VER1` | `MEM:` / Memory 驱动 `Create` | 查询结果、闪烁、未落盘草稿 |
| `DS_WS` | `PROVIDER_SMARTGIS` | — | **源码已删**；`make_sdbd_open_target` 返回空；瓦片见 [tile-layer-provider](../archive/specs/2026-09-13-tile-layer-provider-design.md) |

`DS_DB_ODBC` / `DS_DB_MYSQL` / `DS_DB_ORACLE` 保持枚举占位，`Create*` 返回 null 并打日志。不要再做一套设备。

URL 前缀（`sdb:` / `sfile:` / `smem:` / `sws:`）可继续写进 `.dsm`，便于人读；真正打开只看 `unType` + `unProvider` + 路径/连接串。

`CreateMemVecLayer` / `CreateMemRasLayer` 不再 `new SmtMemDataSource()`。它们构造一个 `OgrDataSource`（`DS_MEM`），`CreateVectorLayer` / `CreateRasterLayer`，然后丢掉外壳数据源指针的方式与今天相同——但图层背后是 Memory 驱动，不是 `vector<SmtFeature*>`。

`CreateTmpDataSource(DS_FILE_SMF)`（xcatalog）同样得到 `OgrDataSource`。目录树列出的是 `GDALDataset::GetLayerCount()`（外加栅格子数据集），不是 `ReadSmf`。

删除 `SmtSmfVecLayer` / `SmtSmfRasLayer` / `SmtMemVecLayer` 作为**产品路径上的类型**。`sde_smf` / `sde_mem` 源码树与 GN 目标已移除。

## 内存图层（`SmtMem*`）

今天的内存层承担三件事，必须拆开：

1. **未落盘草稿 / 查询结果 / 闪烁层** — `CreateMemVecLayer()`。
2. **文件/库图层的编辑缓存** — `SmtSmfVecLayer` 把整层 OGR 要素克隆进 `SmtMemVecLayer`，之后游标、Query、Append 都打在 mem 上；与磁盘的同步不完整。
3. **栅格字节缓冲与瓦片指针** — `SmtMemRasLayer`（已删，改 `OgrRasterLayer`）/ `SmtMemTileLayer`（已删死工厂；见 tile-layer-provider）。

**推荐：矢量走 GDAL Memory 驱动；适配器内保留一层薄 `SmtFeature*` 游标缓存（`OgrVectorLayer::features_` 已是这个形状）。不要把 `SmtMemDataSource` 留成第一类持久化设备。**

| 方案 | 做法 | 取舍 |
| --- | --- | --- |
| A. 全盘 Memory 驱动 | `CreateMem*` → `MEM:` dataset + `OgrVectorLayer` | 一条代码路径；Query 结果也是 GDAL layer。Memory 驱动在标准 GDAL 里几乎总是在。 |
| B. 永远保留 `SmtMem*` | 文件/库用 OGR，草稿仍用 `vector<SmtFeature*>` | 继续两套 Query / 游标 / Append。 |
| C. 混合（采用） | 事实源 = Memory / 文件 / 库的 `OGRLayer`；`Fetch` 后解码进 `features_` 只服务 `MoveFirst` / `GetFeature(i)` ABI | 不引入第三种图层类；大图层可后续改成按需 `GetFeature` 而不全量 `Fetch`。 |

栅格草稿：优先 `MEM` 栅格或内存 `GDALDataset`；在 raster I/O 补齐之前，`OgrRasterLayer` 可以暂存 buffer，但 **Create/Open 不得再假装成功却不挂 GDAL**。瓦片（未来 `TileProvider`；`SmtMemTileLayer` 死工厂已切除）见 [tile-layer-provider](../archive/specs/2026-09-13-tile-layer-provider-design.md)，**不进 `SDBD:MEM`**，不阻塞矢量统一。

`Query(pGQueryDesc, pPQueryDesc, pQueryResult)`：空间过滤走 `OGRLayer::SetSpatialFilter`，简单属性走 `SetAttributeFilter`。结果写入调用方传入的 `SmtVectorLayer*`（选择工具会传入 Memory 适配层）。OGR 表达不了的谓词：扫描 + 现有内存几何判定，结果仍 Append 到那个 Memory 层。不要为 Query 再 new `SmtMemVecLayer`。

## SMF 文件

`SmtSmfDataSource` 不是一种格式。它是：

- 一个私有 `.smf` 目录（`ReadSmf` / `WriteSmf`：`vector<SmtLayerInfo>` 的二进制清单）；
- 每个矢量层一个并列 `.shp`；
- `Fetch` 时 `GDALOpenEx` 只读打开 shp，`CopyOGRFeaToSmtFea`，再丢进 mem。

目标：shapefile、GPKG、GeoJSON 与 PostGIS **同一套** `OgrDataSource` + `OgrVectorLayer`。`PROVIDER_SHAPE` / `PROVIDER_OGR_SUPPORT` 只是 `file_provider_traits` 里的驱动名（`ESRI Shapefile` vs 自动探测）。

一个 `OgrDataSource` 包一个 `GDALDataset*`：

- 单文件（一个 `.shp`、一个 `.gpkg`、一个 `.geojson`）= 一个数据集，层名来自 OGR。
- **目录 + 多个 shapefile**（旧 SMF 工程）：兼容打开已有 `.smf`，对清单里每个 `szArchiveName` 做 `GDALOpenEx`，以**逻辑**数据源呈现多个层。实现上允许内部持有 `vector<GDALDataset*>`（只为这种目录工程）；**新工程不再 WriteSmf**。新产品默认是一个多图层文件（GPKG）或显式打开单个 shapefile。
- `PROVIDER_OGR_SUPPORT` 指向已是多图层的文件（GPKG 当文件、SpatiaLite、部分 CAD）时，不要再包一层 `.smf`。

`smf_ogrsupport.cpp` 的类型表并入 `feature_kind_traits` / codec（大部分已转调）。删掉第二份 `OGRFldTypeToSmtFldType` 映射，避免和 traits 分叉。

Shapefile 限制（10 字符字段名、无原生事务、无 TIN）留在驱动层。产品类型仍是 `SmtFtTin` 等；存盘时用已有 MultiPolygon 回退。需要事务 / 多图层 / 栅格同库时用 GPKG，而不是增强 `.smf`。

## Web / WS 图层（不在本文）

`src/sdb/datasource/ws` **源码树已删除**。`DS_WS` / `PROVIDER_SMARTGIS` 仅为枚举残留；`make_sdbd_open_target` 对 `DS_WS` 返回空串。`CreateMemTileLayer` / `SmtMemTileLayer` / `sde_mem` **亦已删除**；抽象 `SmtTileLayer` 仍在 `layer.h`，**不是** GDAL Memory。

产品 2D 瓦片（HTTP(S) XYZ/WMTS、`MapLayer(kind=tile)`、与 GDAL WMS 的可选关系）一律见 sibling：

**[`2026-09-13-tile-layer-provider-design.md`](../archive/specs/2026-09-13-tile-layer-provider-design.md)**

本文不恢复 WS 设备，不把瓦片塞进 OGR / `SDBD:MEM`。WFS 只读矢量若需要，另走 OGR WFS + 同一矢量适配器（仍非本文实现）。

## 栅格与瓦片 vs OGR 矢量

| 产品类型 | GDAL 对象 | v1 |
| --- | --- | --- |
| `SmtVectorLayer` / 点线面 / Anno / TIN / Grid | `OGRLayer` | 必须。Grid 优先做成同数据集里的 GDAL raster；否则已有 MultiPoint + `grid_row` / `grid_col`。 |
| `SmtRasterLayer` / `SmtFtChildImage` | `GDALDataset` 栅格 / 子数据集 / GPKG tiles | 必须补齐 `OgrRasterLayer`（今天 Create/Open 为 false）。缺栅格创建能力则 `ERR_UNSUPPORTED` + 日志，不发明 blob 表。 |
| `SmtTileLayer` / `SmtLayer_Tile` | 不是 OGR | **不在本文。** 见 [tile-layer-provider](../archive/specs/2026-09-13-tile-layer-provider-design.md)；勿用 Memory 冒充。 |

同一 GPKG 可以既有矢量层又有栅格；`fill_layer_infos()` 已经扫 `GetLayerCount()`，应同时扫栅格子数据集并把 `unFeatureType` 标成 `SmtLayer_Ras`。`Map` 仍按 `GetLayerType()` 区分绘制，不需要知道 GDAL。

## ABI

**推荐：GDAL 原生。**

保持：

- `dll_stem`（`SmtGisCore`、`GdalDevice`、`SmtSDEDeviceMgr`）与 `SDE_GDAL_EXPORT` 直到链接面另开清理。
- `.dsm` 头字节布局可暂留；打开只看路径 / `SDBD:` 连接串。
- `eDSType` / provider 枚举取值不重排（旧文件）。

删除（产品路径）：

- `SmtDataSource` / `SmtVectorLayer` / `SmtFeature`。不要僵尸包装器。
- 新代码不得再 `CreateMemVecLayer()` → `SmtMemVecLayer`。

允许：

- `Style` 与 `src/algorithm` 几何类型独立存在（不是要素门面）。
- `SmtTileLayer` / `DS_WS` 残留 → [tile-layer-provider](../archive/specs/2026-09-13-tile-layer-provider-design.md)。
- 工具 / 地图 `#include` `ogrsf_frmts.h` / `gdal_priv.h`。

`LoadLibrary` 导出宏保持。SMF/Mem 图层子类离开产品路径；删磁盘文件是后续清理。

## Traits（`SmtFeatureType` ↔ WKB）

已有 `feature_kind_traits<Ft>`（`ogr_feature_kind.h`）与 `visit_feature_kind` 继续当**唯一**的产品类型 → WKB / 额外字段表。不要在 SMF 或 Memory 路径再写 switch。

本设计只**扩展连接 traits**，不改几何层次：

```cpp
template <uint Provider>
struct file_provider_traits {
  static constexpr bool supported = false;
  static constexpr const char* driver_name = nullptr;  // null = GDALOpenEx 自动探测
  static std::string open_target(const Smt_GIS::SmtDataSourceInfo& info);
};

template <>
struct file_provider_traits<Smt_GIS::PROVIDER_SHAPE> {
  static constexpr bool supported = true;
  static constexpr const char* driver_name = "ESRI Shapefile";
  static std::string open_target(const Smt_GIS::SmtDataSourceInfo& info);
};

template <>
struct file_provider_traits<Smt_GIS::PROVIDER_OGR_SUPPORT> {
  static constexpr bool supported = true;
  static constexpr const char* driver_name = nullptr;
  static std::string open_target(const Smt_GIS::SmtDataSourceInfo& info);
};

template <>
struct mem_provider_traits<Smt_GIS::PROVIDER_MEM_VER1> {
  static constexpr bool supported = true;
  static constexpr const char* driver_name = "Memory";
  static std::string open_target(const Smt_GIS::SmtDataSourceInfo&) {
    return "MEM:";
  }
};
```

`make_gdal_open_target` 按 `unType` 分发到 db / file / mem traits。`OgrVectorLayer::Create` 继续 `visit_feature_kind` → `CreateLayer(..., Traits::wkb)` + `extra_fields` 元组。几何 encode/decode 仍是 traits 上的静态函数，公共 codec 仍是那两个 `copy_*` 函数。

算法层分析直接调 OGR（GEOS 在 GDAL 内）。图层管理不引入几何 traits。

## 数据流（目标）

1. **Create / Open 数据源** — `register_gdal_driver()`（`GDALAllRegister`）一次；`GDALOpenEx`（更新模式）。缺文件且 traits 给出驱动名：`GetDriverByName->Create`。Memory：`Memory` 驱动 `Create`。失败：`m_bOpen = false`，日志里写驱动名、打码后的目标、`CPLGetLastErrorMsg()`、当前矢量驱动列表。
2. **列举** — `GetLayerCount` + 栅格子数据集 → `m_vLayerInfos`。没有 `DS_TB`，新工程没有 `.smf`。
3. **Create / Open / Delete 图层** — `GDALDataset::CreateLayer` / `GetLayer` / `DeleteLayer`（或删子数据集）。几何类型来自 `feature_kind_traits`。
4. **Fetch** — `ResetReading` + `GetNextFeature`；codec → `SmtFeature`；可选填 `features_` 供游标 ABI。
5. **Append / Update / Delete** — codec → `OGRFeature`；`CreateFeature` / `SetFeature` / `DeleteFeature`。不再先改 mem 再“有空再写盘”。
6. **Query** — OGR 过滤；否则扫描。结果层是 Memory 适配器。
7. **事务** — `GDALDataset::StartTransaction` 等；shapefile / Memory 无事务则每条自动提交（与 DB spec 相同）。
8. **Close** — `GDALClose`；清空层列表与游标缓存。

没有 COM，没有按格式的 `PreReadShp` / `ReadShp` 副本。

## 分阶段落地

| 阶段 | 内容 | 可验证结果 |
| --- | --- | --- |
| 0（已完成） | DB 走 OGR；ADO 离开 `src_all` | `sde_gdal_test`；管理器对 `DS_DB_ADO` new `OgrDataSource` |
| 1 | `file_provider_traits` + `Open` 认 `DS_FILE_SMF`；shapefile / 自动探测走同一 `OgrVectorLayer` | 临时目录里 `.shp` 点层往返；xcatalog 临时 DS 仍编译 |
| 2 | 只读打开旧 `.smf` 清单；**停止 WriteSmf** | 旧工程能列出并打开层；新 Create 不产生 `.smf` |
| 3 | `DS_MEM` + Memory 驱动；`CreateMemVecLayer` 改接线 | select/flash 仍拿到 `SmtVectorLayer*`；Query 写入 Memory 层 |
| 4（已完成核心） | `OgrRasterLayer` + GDAL MEM；`CreateMemRasLayer` 改线；删 `SmtMemRasLayer` | MEM Create/Open/CreaterRaster/GetRaster 绿；文件 Open 可用；GPKG 内嵌栅格列举可后续加强 |
| 5 | 产品路径不再链接 `sde_smf` / `sde_mem`（二者源码树与 GN 已移除）；测试覆盖文件 + SDBD:MEM +（可选）PG | `src_all` 可不依赖 SMF/Mem 图层实现 |
| 6（非 v1） | WFS → 同一矢量适配器；瓦片 → [tile-layer-provider](../archive/specs/2026-09-13-tile-layer-provider-design.md) | — |

每个阶段都要 `build.bat` 与 `build.bat te` 保持绿。阶段 5 之前不要删 `src/sdb/datasource/smf`、`mem` 目录——先改调用方。不要在这些阶段里碰 ADO 删除。

实现计划另文（`docs/superpowers/plans/`）；本文不拆任务复选框。

## 风险

| 风险 | 处理 |
| --- | --- |
| 当前 `gdal_sdk` 没有 GPKG 和/或 PostgreSQL 驱动 | **API 仍是 GDAL Dataset/Layer。** `Open` / `Create` 查 `GetDriverByName`；缺失则日志 + false。测试对 GPKG / PG **跳过**（已有 `PG_DSN` 模式）；shapefile + Memory 是无条件必跑项，因为 ESRI Shapefile 与 Memory 在常见 SDK 里更稳。 |
| GPKG TIN | MultiPolygon 回退；产品类型仍是 `SmtFtTin`（DB spec 已定）。 |
| Shapefile 字段名 / 事务 | 不在产品层补洞；需要完整 GIS 库时用 GPKG 或 PostGIS。 |
| 旧 `.smf` + 目录 shp | 阶段 2 只读兼容；内部多 `GDALDataset*`。新工程不写 `.smf`。 |
| `OgrVectorLayer` 全量 `Fetch` 吃内存 | 与今天 SMF→mem 克隆同类；后续可按需读。不作为 v1 阻塞。 |
| 选择工具假定 mem 层可写 | 阶段 3 用 Memory 适配器满足同一虚接口。 |
| `OgrRasterLayer` 已挂 MEM；render 仍吃编码 buf | GDI/`layer_image_pixels` 继续 `GetRasterNoClone` + CxImage `image_code`；`Open(文件)` 已回填 `/vsimem` blob。未解码格式时 band 可能仍是占位。 |
| 并行 agent 删 ADO | 本文与实现只动 `gdal/`、`mgr/`、以及 SMF/Mem **接线**。不改、不还原 ADO 路径。 |
| SDK 无 Memory 驱动（极少） | 阶段 3 测试失败即停；不回退私有 `vector` 实现（那会重新分裂后端）。 |

## 测试

扩展已有 `test("sde_gdal_test")`（`src/sdb/datasource/gdal/sde_gdal_test.cc`，挂在 `//:test_all`）。不要为 SMF/Mem 再开一套平行测试。

**无条件（不依赖 GPKG / PG / 网络）：**

1. 保持现有：拒绝 `PROVIDER_ACCESS` / `SQLSERVER`；管理器对 `DS_DB_ADO` 构造 `OgrDataSource`。
2. **Shapefile 往返**：临时目录创建点层，Append 一点，Close，再 Open，坐标与 FID 一致。线、面各一次。
3. **Memory 往返**：`CreateMemVecLayer`（或 `DS_MEM` + `CreateVectorLayer`），Append / Update / Delete / Query 矩形，不落盘。
4. **列举**：一个数据集两个图层（Memory 或 shapefile 目录兼容路径），`GetLayerCount()` == 2，名字匹配。
5. **Anno 字段** `anno` / `color` / `angle` 在 Memory（及 shapefile 若字段名允许）往返。

**有驱动才跑：**

6. 现有 GPKG 点/线/面/Anno 往返：`GetDriverByName("GPKG")` 为空则 skip，不要 fail `build.bat te`。
7. `PG_DSN` 已设则跑 PostGIS 点往返（已有）。

**不要：** 要求 SQL Server / Access；要求实时 WMS；在本测试里编第二份 GDAL。

阶段 4：MEM 栅格 Create + `CreaterRaster` / `GetRasterNoClone` blob 往返 + `CreateMemRasLayer`（已加）。无 `MEM` 驱动则 skip。GeoTIFF `Open` 路径可测。

## 文档（实现变更时同步）

- `docs/superpowers/src-layout.md` — `sdb/datasource` 写成「一个 OGR/GDAL 设备覆盖文件、库、内存」；SMF/Mem 标成兼容壳或已移除。
- `src/README.md` — 同上。
- 根 `README.md` — 仅当模块表仍把 SMF 写成独立文件后端时改，并刷新 **最后更新**。
- `docs/README.md` — 链到本文（与本文同一提交）。

## Success

- 产品路径上，文件 / 库 / 内存的打开、创建、列举、编辑、查询、关闭都经过一个 `OgrDataSource` 和一个 `GDALDataset`（Memory 驱动也算）。
- 不再出现新的 per-format / per-feature-type 图层子类。
- `Map` 与工具看见 `GDALDataset` / `OGRLayer` / `OGRFeature`，不是 `SmtFeature`。
- `GetDriverByName("SDBD")` 非空；`SDBD:MEM:` 点层往返。
- `build.bat` 与 `build.bat te` 绿；shapefile + Memory 测试无条件跑；GPKG / PG 随 SDK 跳过或跑。
- 缺 GPKG 驱动时产品仍能打开 shapefile 与 Memory 层；错误信息说明缺的是驱动，不是 API。
- 无第二份 GDAL，无 Qt，无 ADO 回潮。

**最后更新：** 2026-09-28

---

## § DataSession / Provider facade（新树产品入口）

**Status:** accepted（本 §）  
**Updated:** 2026-09-28  
**Approach:** Facade + Adapter（方案 1）  
**Migration:** 仅新树（`app/views`、`content`、新 `tool`、pipeline 调用方）；`legacy/*` 继续用 `gis::DataSourceMgr`，本轮不改其 API、不删。  
**Considered living:** 本文（GDAL 图层后端）+ [`2026-09-13-sdb-feature-maplayer-composition-design.md`](../archive/specs/2026-09-13-sdb-feature-maplayer-composition-design.md) + [`2026-09-19-sdbd-wsl-client-design.md`](../archive/specs/2026-09-19-sdbd-wsl-client-design.md)。本 § 是编排层，不是第二套 I/O 存储；不新开 dated twin。

### Goal

- 新树打开数据源经 **`gis::datasource::DataSession`**，产品只拿 **`gis::MapLayer` / `gis::Feature`**（组合 ABI；代码在 `gis::`，文档历史名 `sdb::`）。
- Provider 注册表取代 `DataSourceMgr::open_dataset` 的二分 if（本地 `open_sdbd_dataset` vs 远程 `open_provider_sdbd_dataset`）。
- **禁止** 新代码依赖 `DataSourceMgr`；`session` / `provider` **禁止** 依赖 `mgr`。
- 遗留 `legacy/gis/datasource` 冻结：仅 bugfix；DSM catalog / `move_*` / 单例留给 legacy。

### Non-goals

- 不删、不改名 `DataSourceMgr` 公共 API（本轮）。
- 不把远程 sdbd 强行并进单一 `GDALOpenEx` 路径。
- 不改 `content/public`；不引入 Qt；不 vendor 第二 GDAL。
- 不在本轮迁移 `legacy/ui/xcatalog` / `legacy/tool` / `smtapp`。

### Layout

| 路径 | 角色 |
| --- | --- |
| `datasource/session/` | L1：`ConnectionSpec`、`DatasetHandle`、`DataSession` |
| `datasource/provider/` | L2：`Provider`、`ProviderRegistry`、Local/Remote SDBD |
| `datasource/provider/impl/sdbd/` | L3：`client/` · `driver/` · `remote/` · `codec/` |
| `datasource/provider/impl/ogr/` | L3：`codec/` · `text/` · `raster/` |
| `datasource/provider/impl/gdal/` | L3：`register_gdal_driver` 聚合入口 |
| `datasource/pipeline/` | L4：feature load Pipeline（header-only） |
| `legacy/gis/datasource/` | 遗留 `DataSourceMgr`（冻结；不在 `gis/datasource` 顶层） |

依赖方向：`session` → `provider` → `sdbd|ogr`。公开命名空间两层：`gis::datasource`；内部 `gis::datasource::detail`。

### Core types

```cpp
namespace gis {
namespace datasource {

enum class ProviderKind { kLocalSdbd, kRemoteSdbd };

// Product connection description. Leftover SmtDataSourceInfo conversion is
// legacy/gis/datasource/connection_spec_info.h (leftover → product).
struct ConnectionSpec {
  ProviderKind kind = ProviderKind::kLocalSdbd;
  std::string name;
  std::string url;  // remote base / sdbd-rpc://…
  // Optional local file/db fields (empty unused).
  std::string path;
  std::string file_name;
  std::string service;
  std::string db_name;
  std::string uid;
  std::string pwd;
  std::uint32_t ds_type = 0;      // eDSType when needed
  std::uint32_t provider_id = 0;  // eSmt*Provider when needed
};

// RAII owned GDALDataset. Product lists layers as MapLayer (!owns ogr).
class DatasetHandle {
 public:
  DatasetHandle() = default;
  explicit DatasetHandle(GDALDataset* owned);
  ~DatasetHandle();
  DatasetHandle(DatasetHandle&&) noexcept;
  DatasetHandle& operator=(DatasetHandle&&) noexcept;
  DatasetHandle(const DatasetHandle&) = delete;
  DatasetHandle& operator=(const DatasetHandle&) = delete;

  explicit operator bool() const;
  GDALDataset* gdal() const;          // non-owning view
  GDALDataset* release();             // give up ownership

  int layer_count() const;
  MapLayer layer_at(int index) const;           // MapLayer::from_ogr
  MapLayer layer_by_name(const char* name) const;
};

class Provider {
 public:
  virtual ~Provider() = default;
  virtual ProviderKind kind() const = 0;
  virtual DatasetHandle open(const ConnectionSpec& spec) = 0;
};

class ProviderRegistry {
 public:
  void register_provider(std::unique_ptr<Provider> provider);
  Provider* find(ProviderKind kind) const;
  DatasetHandle open(const ConnectionSpec& spec) const;
  static ProviderRegistry make_default();  // local + remote registered
};

// New-tree entry. Not a process singleton; callers own the session.
class DataSession {
 public:
  DataSession();  // make_default registry
  explicit DataSession(ProviderRegistry registry);

  DatasetHandle open(const ConnectionSpec& spec);

  // Scratch Memory vector layer (owned MapLayer via adopt_dataset).
  MapLayer create_mem_vector_layer(const char* name = "scratch");
};

}  // namespace datasource
}  // namespace gis
```

### Data flow

```
DataSession::open(spec)
  → ProviderRegistry::open
      → LocalSdbdProvider  → open_sdbd_dataset(info) → DatasetHandle
      → RemoteSdbdProvider → open_provider_sdbd_dataset(info) → DatasetHandle
  → caller: handle.layer_at(i) → MapLayer
  → Feature via OGR cursor / pipeline decode (existing Feature composition)
```

`kind_from_info`：`unProvider == PROVIDER_SDBD` → `kRemoteSdbd`，否则 `kLocalSdbd`（与现 `DataSourceMgr::open_dataset` 一致）。

### Error / ownership

- `open` 失败返回 empty `DatasetHandle`（`operator bool` false）；不抛。
- `DatasetHandle` 析构 `GDALClose`；`MapLayer::from_ogr` 不拥有层；scratch 用 `MapLayer::adopt_dataset`。
- 不暴露 `move_first` / DSM 给新 API。

### Testing

- 单测 `datasource_session_test`：`ConnectionSpec` round-trip；`DataSession` 打开 `DS_MEM` → `layer_count` / `MapLayer` 名；远程可不连网（仅测 registry 选中 `kRemoteSdbd` 或 mock 失败 empty）。
- 挂 `//:test_all`；不替代 `sde_gdal_test` / `sdbd_live_test`。

### Success（本 §）

- 新树可 `#include "gis/datasource/session/data_session.h"` 打开本地 MEM/GPKG 并拿到 `MapLayer`。
- `ProviderRegistry` 可扩展；`mgr` 无新增调用点来自新树。
- `build.bat` + session 单测绿。

---

## §GIS coverage + performance benchmarks（2026-09-28）

**Status:** accepted  
**Plan:** [`../plans/2026-09-28-gis-coverage-benchmark.md`](../plans/2026-09-28-gis-coverage-benchmark.md)  
**As-built matrix:** [`../gis-test-matrix.md`](../gis-test-matrix.md)

### Intent

Ship **functional coverage** (industry-capability matrix vs GDAL / GEOS / PROJ / QGIS-shaped surfaces) **and** **compiler coverage reports** for `src/gis`, plus **performance baselines** with dual-run on critical paths. Entry points: CLI (`build.bat te` / `b`) **and** Diagnostic Tools Console (`:gis test` / `:gis bench`).

### Architecture

- Aggregate `//src/gis:gis_test_all` → root `//:test_all` / `test_shell`.
- Aggregate `//src/gis:gis_benchmark_all` → root `//:benchmark_all` (`build.bat b`).
- Benchmarks use **QPC micro-bench** style matching `views_bench` / `content_console_bench` (not a second harness). `//third_party:gbenchmark` remains available but is not required for v1 GIS benches.
- Dual benches on critical paths: product API vs raw OGR / GEOS / PROJ (same `gdal_sdk`).
- Coverage: `testing/coverage/gis_coverage.ps1` runs `gis_test_all` exes; OpenCppCoverage when present → `out/Debug/coverage/gis/`; else functional summary + matrix.
- Console spawns the same exes (no duplicate test logic).

### Scope (v1 = full tree)

`kernel` (geo/proj/tin/stat) · `datasource` · `model` · `carto`. CPU MapFrame / World 覆盖在 `//src/vista:vista_test_all`（不在 `gis.dll`）。

### Non-goals

- No second GEOS/PROJ vendor; no Qt; no new dated design twin.
- Coverage not a default gate inside plain `build.bat` (separate coverage entry).
- No fail ceilings on benches in v1 (informational print; regression thresholds later).

### Success

- Missing GIS unit tests listed in the matrix are registered under `gis_test_all`.
- `build.bat debug b` builds/runs at least geo / proj / datasource benches.
- `:gis test` / `:gis bench` work when DebugAgent is enabled.
- Coverage script produces a report directory under `out/*/coverage/gis/`.

---

## § gis/model product surface vs leftover + OGR Map/Layer/Feature（2026-10-04）

**Status:** accepted（`gis/model` 已拍平；`layer/`+`crs/` 并入 `map/` + `ConnectionSpec`；datasource 去掉 `provider/impl/`。`gis::Layer` 虚基不在产品头；`from_leftover` 已不在产品 `MapLayer`）  
**Diagram:** [`../diagrams/gis-model-ogr-layers.html`](../diagrams/gis-model-ogr-layers.html)  
**Plan checkboxes:** [`../plans/2026-09-28-datasource-session-provider.md`](../plans/2026-09-28-datasource-session-provider.md) Task 5  
**Considered living:** 本文（GDAL 唯一后端 + Feature/MapLayer composition 已 fold）。不是新子系统；Gate 不满足，不新开 dated spec/plan。

### Intent

两件事同一终局，方向一致（**leftover → product OK；product ↛ leftover**）：

1. **产品面在 `src/gis/{feature,layer,map,edit}`（无 `model/` 父目录）。** 仅被 leftover 使用的 catalog ABI **在 `src/legacy/gis/`**。禁止为迁出发明 product→legacy 依赖。
2. **map · layer · feature{geo + attr} 对齐 OGR。** 产品调用方操作的是 `OGRDataSource`/`GDALDataset` 语义上的 Map、`OGRLayer` 语义上的 Layer、`OGRFeature`（FID + `OGRGeometry*` + `OGRFieldDefn` 字段）语义上的 Feature。几何是 **一棵** OGC/OGR `OGRGeometry`（`coordinateDimension` 2|3 在**实例**上；traits 在 `gis/geo`）。属性是 OGR 字段类型，不是第二套 string map / 已删的 `SmtAttribute`。

### Non-goals

- 本 § **不**一次搬完 `gis.dll`；不改 `content/public` 嵌入 API；不加 Qt；不第二套 GEOS/PROJ；不平行 `Geometry2`/`Geometry3`。
- 不把 `Style` leftover carto POD（已链进 `gis.dll`）假装成 OGR 字段。Style 走 `gis::style::StyleDocument` / layer `style_document()`；POD 收口仍是 leftover carto 债，另轨缩小。
- 不把 XYZ/WMTS 瓦片硬塞进 `OGRLayer`（仍 `MapLayer` + `TileProvider`，见 §Tile）。
- 不把 `geo::Grid` 伪造成 `OGRGeometry`（OGR 无 Grid；sidecar + `indexed_tin`，见 `geometry_traits.h` 注释）。

### Evidence snapshot（CBM `smartgis`，2026-10-04）

Coverage：`src/gis/model` 8 个 parse_partial（`feature.h` / `map.h` / `map_layer.h` / edit sessions）；结论以源码为准。Caller 用 `search_code` + `query_graph` CALLS，**非**全仓库无 path Grep。

| 现状类型 | 路径 | 产品调用（例） | leftover 调用（例） | 处置 |
| --- | --- | --- | --- | --- |
| `gis::Feature` | `model/feature/feature.h` | snake_case OGR field I/O；`feature_test` | leftover `SmtFeature`（`legacy/gis/feature/leftover_feature.h`）+ chart `leftover_append_feature` | **KEEP** `OGRFeature*`。PascalCase **已 MOVE**。sidecar 仍 `Style*` / `Material*` |
| `gis::SmtFeatureType` | 同 header | codec `infer_feature_type`；`sdbd_layer`；`MapLayer::feature_type` | orthogrid/dem/model3d/proj 插件；`map_painter`；chart | **ADAPT** → 产品以 `OGRwkbGeometryType` + 少量 kind（Anno/Grid）为准；枚举名 `SmtFt*` 最终随 leftover |
| `gis::MapLayer` | `model/map/map_layer.h` | `DataSession` / `DatasetHandle`；`content`；tile `make_*_map_layer` | 经 `gis::Map` / `from_leftover` | **KEEP** OGR 缝已补。**债：** `from_leftover` / `leftover()` 仍在产品头（tile 产品层仍 `ProviderTileLayer : SmtTileLayer`） |
| `gis::Map` | `model/map/map.h` | `MapEditSession`；产品测试 | leftover `using Map = gis::Map`（`map.h` 底部） | **ADAPT 已改名**。**债：** `AddLayer(SmtLayer*)` / `GetLeftoverLayer` 仍在产品 `Map` |
| `SmtLayer` / `SmtRasterLayer` / `SmtTileLayer` | `model/layer/layer.h` | `OgrRasterLayer` / `ProviderTileLayer` 仍继承；`ogr_connect` `PROVIDER_*`/`DS_*` | leftover copy_layer；DataSourceMgr ras | **债：** 虚基仍产品头（产品实现继承 leftover 虚接口，不能 `#include leftover`）。下步：产品 raster/tile 去继承 |
| `SmtDataSource` / `Smt*Info` / `leftover_layer_feature_type` / `leftover_feature_wkb` | `legacy/gis/layer/layer.h` | 产品测试经 leftover `connection_spec_info` | catalog / DataSourceMgr / 插件 | **MOVE 已落地** |
| `SmtGQueryDesc` / `geo::SpatialRelation` | 仍 `model/layer/layer.h` | `Map::QueryFeature`；`select_query_test` | leftover select tool | **债：** 查询描述仍绑在产品 `Map` API |
| `append_cloned_feature` / `copy_layer(OGRLayer*)` | `model/feature/feature_api.h` | catalog `copy_layer`（leftover UI）；OGR 拷贝本身可产品化 | `copy_layer(SmtLayer*)` / `SmtRasterLayer*` | **KEEP** OGR 两参拷贝。**MOVE** `Smt*` 重载已在 leftover。产品 Feature 不再 `set_style(const char*)`（named lookup 在 leftover `SmtFeature`） |
| `gis::Envelope` | `model/envelope.h` | `geo/ops/geometry_traits.h` `fill_envelope`；`MapLayer`/`Map` | leftover carto / rhi2d map paint / scene3d `geo_object` | **KEEP**（header-only MBR；可与 `OGREnvelope` 互拷，不是第二套几何） |
| `gis::CrsId` | `model/crs/crs.h`（已删） | 无调用方 | — | **DELETE** 空 stub（权威 CRS 仍是 `OGRLayer::GetSpatialRef()` / `OGRSpatialReference`；PROJ 在 `gis/geo/proj`） |
| `EditSession` / `CommandEditSession` / `MapEditSession` / `OptimisticLayerStore` | `model/edit/` | `content/view`；`tool/workspace`；`app/views` fill/nav/self-test | `legacy/tool/draft/appendfeaturetool`；`legacy/ui/map/viewport/view_2d_edit` | **KEEP**。编辑会话写 `OGRLayer`/`Feature`，不写 `SmtLayer` 虚 CRUD |
| `geo::geometry_traits` / `OGRGeometry` | `gis/geo`（非 model） | 产品几何唯一层次 | leftover 经 `Feature::geometry()` | **KEEP** 在 `gis/geo`；Feature.geo **就是** `OGRGeometry*` |

**DELETE：** 空 `CrsId` stub 已删。`SmtSpIdxInfo` 在 leftover `layer.h`，图上 out=0 时再确认能否删。

### OGR mapping（终局名）

| OGR / GDAL | 产品 C++（终局） | 今日 | 差距 |
| --- | --- | --- | --- |
| `GDALDataset` / 概念上的 `OGRDataSource` | `gis::datasource::DatasetHandle` + 文档 `gis::Map` | `DatasetHandle` 已有；文档类已名 `gis::Map`（`using Map = Map`）；仍混 `SmtLayer*` | 去掉 leftover 层槽；删产品头别名 |
| `OGRLayer` | `gis::MapLayer`（持非拥有 `OGRLayer*`；scratch 用 `adopt_dataset`） | `from_ogr` / `adopt_dataset` 已有；`ogr()` 逃生口 | 产品面补：`GetLayerDefn`/`OGRFeatureDefn`、`GetSpatialRef`、`ResetReading`/`GetNextFeature`、`SetSpatialFilter`/`SetAttributeFilter`、`GetFIDColumn`。实现薄封装，禁止第二套游标 |
| `OGRFeature` | `gis::Feature` | snake_case 全 `OGRFieldType`；PascalCase 在 leftover `SmtFeature` | 无新产品 PascalCase |
| `OGRGeometry` | `Feature::geometry()` + `geo::geometry_traits` | 已是 `OGRGeometry*`；TIN = `OGRTriangulatedSurface` | Grid 保持 sidecar；不要 `SmtFtTin` 第二棵树 |
| `OGRFeatureDefn` / `OGRFieldDefn` | Layer schema；Feature 字段按下标/名 | att-struct leftover UI 已读 `OGRLayer`；codec `CreateField` | 产品 catalog/inspect 只谈 FieldDefn，不谈 `SmtAttribute` |
| `OGRSpatialReference` | Layer CRS | `SmtLayer::SetSRS` 字符串 | MapLayer 暴露 `GetSpatialRef` |
| Raster | `GDALRasterBand` / dataset；**不是** `OGRLayer` | `SmtRasterLayer` 虚接口仍在 `layer.h`；实现 `OgrRasterLayer` | 产品栅格走 dataset handle / 现有 OGR raster 封装；虚基迁 leftover |
| Tile | `TileProvider` → `MapLayer`（kind tile） | `SmtTileLayer` 仍在 `layer.h`；`make_xyz_map_layer` 已产品 | `SmtTileLayer` MOVE |

### Ownership（谁拥有什么）

```
product gis/{feature,map,edit}  Feature, MapLayer, Map, Envelope, edit sessions
product gis/datasource  DataSession, DatasetHandle, ConnectionSpec, OGR codec, SDBD driver
product gis/geo         OGRGeometry traits / ops / Grid sidecar codec
product gis/style       StyleDocument, ResolvedPaint
product gis/tile        TileProvider, protocol/cache/provider/layer
leftover src/legacy/    SmtLayer* 虚树, DataSourceMgr, Smt*Info, PascalCase Feature 门面,
                        leftover_append_feature, leftover Style OGR blob, catalog/MFC 适配
                        leftover 可 #include gis/{feature,map,edit} + gis/datasource
```

### Phased migration（禁止大爆炸）

1. **锁 API、不搬文件。** 产品新代码只走 `Feature`/`MapLayer`/`DataSession` snake_case + `ogr()`。Leftover 新调用禁止再扩 `SmtLayer` 虚方法。
2. **切断产品→leftover。** 修 `feature_api.h` 的 `legacy/core/macros` include；OGR `copy_layer` 留产品，`Smt*` 重载挪 leftover TU。
3. **ConnectionSpec 去 `layer.h`（本波已落地）。** `connection_spec_{to,from}_info` / `provider_kind_from_info` 在 leftover `legacy/gis/datasource/connection_spec_info.*`；产品 `ConnectionSpec` / `DataSession` 头不再导出 `SmtDataSourceInfo`。`ogr_connect` 仍为 `PROVIDER_*`/`DS_*` 包含 `layer.h`（随整文件 MOVE）。
4. **补齐 MapLayer/Feature 的 OGR 缝**（defn、游标、filter、字段读写）。单测挂 `gis_test_all` / 现有 `feature_test`、`select_query_test`、`datasource_session_test`。
5. **`Map` → `gis::Map`（已落地）。** leftover `using Map = gis::Map`（产品 `map.h` 底部，直到 catalog/rhi2d 只写 `gis::Map`）。
6. **整文件 MOVE `layer.h` leftover 类型** 到 `src/legacy/gis/layer/layer.h`。**已迁：** `SmtDataSource` / `Smt*Info` / `leftover_layer_feature_type` / `leftover_feature_wkb`。**未迁：** `SmtLayer`/`SmtRasterLayer`/`SmtTileLayer` 虚基（`OgrRasterLayer`/`ProviderTileLayer` 仍继承）、`SmtGQueryDesc`、`MapLayer::from_leftover`。
7. **剥 Feature PascalCase + `leftover_append_feature`（已落地）。** leftover `class SmtFeature : public Feature`；chart 仍走 leftover 适配。
8. **Style/material sidecar。** 不阻塞 1–7；carto POD 已在 gis.dll 的债单独缩小。

### Success（本 §）

- 活规格 + HTML 原理图描述 KEEP/MOVE/ADAPT 与 OGR 映射；计划 Task 5 可勾选。
- 后续落地后：产品 `gis/model` 头无 `SmtLayer` 虚树、无 `legacy/` include；Feature 属性 = OGR fields；Map/Layer 对得上 Dataset/Layer。
- 方向始终 leftover→product。

### Risks

- **`gis.dll` ABI：** `SmtLayer`/`Map`/`GetID` 仍被 leftover 插件与 rhi2d 链接；必须先 leftover 适配再从产品头删除，否则 LoadLibrary 插件断符号。
- **ConnectionSpec ↔ `SmtDataSourceInfo`：** leftover 适配器桥 catalog/`DataSourceMgr`；产品 session 只谈 `ConnectionSpec`。
- **leftover carto POD 在 gis.dll：** `Feature::style()` 仍是 `base::Style*`；迁 Feature 门面时不要把 POD 再拷进产品模型。
- **`feature_api.h` 已是产品→leftover include** — 现有反向边，迁出时必须删，不能复制第二条。
- **parse_partial** 于 `feature.h`/`map.h`：caller 图会漏；搬迁前对目标符号做 scoped search_code。
- **Grid / Anno / ChildImage：** 不是纯 OGR 要素类；sidecar 必须文档化，避免 leftover 再发明 `SmtMemVecLayer`。

---

## § gis/tile 子目录（protocol / cache / provider / layer）（2026-10-04）

**Status:** accepted  
**Diagram:** [`../diagrams/gis-carto-tile.html`](../diagrams/gis-carto-tile.html)  
**Considered living:** 本文（GDAL / tile / SDB umbrella）。不是新子系统；Gate 不满足，不新开 dated spec。

### Intent

`src/gis/tile` 只保留**瓦片协议、提供者、缓存、以及消费它们的 MapLayer 挂载**。磁盘布局按职责分子目录；公共命名空间仍是 **`gis::tile`**（禁止 `gis::tile::protocol` 第三层）。**不留转发头**：旧扁平 include 全部改为新路径。

| 子目录 | 拥有 | 不拥有 |
| --- | --- | --- |
| `protocol/` | `TileCoord` / `Viewport` / `TileImage`、XYZ 数学、WMTS 模板与 Capabilities | HTTP、LRU、MapLayer |
| `cache/` | `TileCache`、`TileDiskCache` | URL 格式化、fetch |
| `provider/` | `TileProvider`、`SourceRegistry`、Style `sources` 绑定、MVT decode | `MapLayer` 工厂 |
| `layer/` | `ProviderTileLayer`、`make_*_map_layer` | XYZ 公式、WMTS XML |

Style JSON 文档仍在并列的 `gis/style`（`gis::style`；见 **§ gis/style 子目录**）。`make_xyz_map_layer_from_source` 挂在 `layer/`，避免 provider → MapLayer 反向依赖。已删除 `mvt_stub.h`（真 decode 在 `provider/mvt.*`）。

非目标：不把 XYZ/WMTS 塞进 `OGRLayer`；不进 scenic / leftover；泛型 envelope 仍 `gis/envelope.h`。不把 tile 拆进 `datasource/` 或 `map/`。

---

## § gis/style 子目录（document / eval / symbol）（2026-10-04）

**Status:** accepted  
**Diagram:** [`../diagrams/gis-vista-architecture.html`](../diagrams/gis-vista-architecture.html)（既有 GIS 泳道；本 § 不锁新架构，不新开 HTML）  
**Considered living:** 本文（GDAL / tile / SDB / style umbrella）。不是新子系统；Gate 不满足，不新开 dated spec。

### Intent

`src/gis/style` 按职责分子目录，顶层子目录个数对齐并列 `gis/tile/{protocol,cache,provider,layer}`（**3** 个，少于 tile 的 4）。公共命名空间仍是 **`gis::style`**（禁止 `gis::style::document` 第三层）。**不留转发头**。

| 位置 | 拥有 | 不拥有 |
| --- | --- | --- |
| `document/` | `parse_style_document` / `serialize_style_document` | 规则求值、符号库 |
| `eval/` | `eval_expression`、`eval_filter`、`select_layers`、`resolve` | JSON 文档 I/O、符号资产 |
| `symbol/` | `SymbolLibrary` | paint 填充、Style JSON |
| 模块根 `style_types.*` | 共享 POD（`StyleDocument` / `ResolvedPaint` / …） | — |
| 模块根 `paint_resolve.*` | `parse_color` / `fill_resolved_paint` | — |

**Leftover pin：** `src/legacy/gis/present/carto/smt_style_from_paint.h` 包含 `"gis/style/paint_resolve.h"`。`src/legacy/` 冻结，本 § **不改 leftover**，因此 `paint_resolve.h` 留在模块根（不是转发头）。产品调用方已改 `document/` / `eval/` / `symbol/` include。

非目标：不新增 `gis::style::*` 第三命名空间；不把 style 拆进 `map/` 或 `datasource/`；不改 leftover ABI。

---

## § gis/{style,tile} 上移（撤 `gis/carto/` 分组）（2026-10-04）

**Status:** accepted  
**Diagram:** [`../diagrams/gis-carto-tile.html`](../diagrams/gis-carto-tile.html) · [`../diagrams/gis-vista-architecture.html`](../diagrams/gis-vista-architecture.html)  
**Considered living:** 本文（GDAL / tile / SDB / style umbrella）。不是新子系统；Gate 不满足，不新开 dated spec。

### Intent

磁盘路径与两层命名空间对齐：`src/gis/style` ↔ `gis::style`，`src/gis/tile` ↔ `gis::tile`，与 `map/`、`datasource/` 同级。`src-layout` nesting cap 是 `src/<layer>/<module>`；`carto/` 只是无命名空间的分组目录，且当时仅含 style+tile。

**锁：**

- **上移** style 与 tile（成对；tile 是 style 的诚实同级）。
- **禁止**把 tile 拆进 `datasource/` + `map/`。
- **禁止**留下 `gis/carto/` 空壳或转发头。
- leftover 制图 POD 仍在 `legacy/gis/present/carto`（与产品 Style JSON 故意撞名）。

### Non-goals

不改 `gis::style` / `gis::tile` 公共符号；不新增 `gis::carto`；不改 leftover ABI。

---

## § testing/data sample packs（2026-10-05）

`testing/data` is **several packs**, not one dataset. Inventory: [`testing/data/README.md`](../../../testing/data/README.md).

| Tree | Role |
| --- | --- |
| `testing/data/china/` | Aligned product China pack (NE 10m + Mapzen DEM, EPSG:4326). `china_plp.geojson` is schematic fallback only. |
| `testing/data/plugin/` | Tiny/schematic plugin smokes — **not** `china_dem`. |
| `testing/data/fixtures/` | OGR / MVT / city 3D Tiles unit fixtures. |
| `testing/data/rs/terrain/` | Leftover AM heightmap. |

GN `//testing/data:china_map_samples` still **flattens** filenames into `out/data/` so `sample_gis_relative_paths()` / `find_sample_dem_path()` keep `../data/china_city.gpkg` and `../data/china_dem.tif`. Repo fallbacks use `testing/data/china/…`.

Do **not** mix plugin rasters into the china seed path.

---

## Folded topics (2026-09-28 merge B)

Former hot specs are under `archive/specs/` (`superseded`). **Revise this file** (append `§`) for new requirements in this topic. Do not create a new `YYYY-MM-DD-*-design.md`.

| Former hot spec | Section / note |
| --- | --- |
| [`../archive/specs/2026-09-13-ogr-db-datasource-design.md`](../archive/specs/2026-09-13-ogr-db-datasource-design.md) | §OGR DB datasource (folded) |
| [`../archive/specs/2026-09-13-sdb-feature-maplayer-composition-design.md`](../archive/specs/2026-09-13-sdb-feature-maplayer-composition-design.md) | §Feature / MapLayer composition (folded)；续 **§ gis/model product surface vs leftover + OGR Map/Layer/Feature** |
| [`../archive/specs/2026-09-13-tile-layer-provider-design.md`](../archive/specs/2026-09-13-tile-layer-provider-design.md) | §Tile layer provider (folded) |
| [`../archive/specs/2026-09-14-sdb-style-document-design.md`](../archive/specs/2026-09-14-sdb-style-document-design.md) | §SDB style document (folded) |
| [`../archive/specs/2026-09-18-china-city-map-plpt-design.md`](../archive/specs/2026-09-18-china-city-map-plpt-design.md) | §China city map / PLPT sample (folded) |
| [`../archive/specs/2026-09-19-sdb-subdir-rename-design.md`](../archive/specs/2026-09-19-sdb-subdir-rename-design.md) | §SDB subdirectory rename (folded) |
| [`../archive/specs/2026-09-19-sdbd-wsl-client-design.md`](../archive/specs/2026-09-19-sdbd-wsl-client-design.md) | §sdbd WSL client (folded) |

