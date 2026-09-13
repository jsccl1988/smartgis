<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# GDAL 作为唯一图层管理后端

**Date:** 2026-09-13  
**Status:** accepted  
**Scope:** 图层的打开 / 创建 / 列举 / 编辑 / 查询 / 关闭一律走 GDAL Dataset / Layer（矢量）或 GDAL raster（栅格）。本文件管 `sdb` 数据源与图层，不管桌面 chrome。

**Sibling:**

- 数据库路径（PostGIS / GeoPackage / SpatiaLite，ADO 退出 `src_all`）见 [`2026-09-13-ogr-db-datasource-design.md`](2026-09-13-ogr-db-datasource-design.md)。
- **2D 地图瓦片（XYZ/WMTS）** 见 [`2026-09-13-tile-layer-provider-design.md`](2026-09-13-tile-layer-provider-design.md) — **另开 spec，不进本文，也不进 `SDBD:MEM` / OGR Memory。**

ADO 源码删除由另一条工作流负责。本文不恢复、不重写、不阻挡那条删除。

## Goal

停止按驱动复制一套 C++ 图层子类，也**不再**用 `SmtDataSource` / `SmtVectorLayer` / `SmtFeature` 当产品 ABI。

- **事实源与调用方类型** 都是 `GDALDataset` + `OGRLayer` + `OGRFeature`（矢量）或 `GDALRasterBand` / 子数据集（栅格）。v1 **不是**适配器层。
- **sdbd 是 GDAL 驱动**（`GDALDriverManager` 名 `"SDBD"`，连接前缀 `SDBD:`）。`GDALAllRegister()` + `register_sdbd_driver()`。打开：`GDALOpenEx("SDBD:MEM:name")` 或 `SDBD:GPKG:path` / `SDBD:PostgreSQL:PG:…`。mgis 没有同名 GDAL 驱动（只有 HTTP `:8021` / `sdbd://`）；本仓 `sdbd://` 在进程内落到 Memory，不另开 HTTP 守护。
- 文件 / 库 / 内存共用这一套 GDAL 对象。Memory = GDAL Memory 驱动（经 `SDBD:MEM:` 或直接 `Memory`）。
- 驱动是否编进当前 `gdal_sdk` 是运行时问题。缺 GPKG / PG 时 `Open` 失败并打日志，不另写 C++ 读写器。

产品调用方 ABI 是 `sdb::Feature` / `sdb::MapLayer`（组合持有 `OGRFeature*` / `OGRLayer*`）；事实源与 I/O 仍是 GDAL/OGR。细节见 [`2026-09-13-sdb-feature-maplayer-composition-design.md`](2026-09-13-sdb-feature-maplayer-composition-design.md)。不要再维护 `SmtAttribute` 第二套字段存储。

## Non-goals

- 不要再 vendor 一份 GDAL / GEOS / PROJ。只链 `//third_party:gdal`（现有 `gdal_sdk`）。
- 不要 Qt。桌面终局是 Views + Skia；本文不改 `src/ui` / `src/app` chrome。
- 不要保留 `SmtFeature` / `SmtVectorLayer` / `SmtDataSource` 作为图层 ABI（几何算法 / `SmtStyle` 可独立留下）。
- 不要在本文周期实现 WMS / WFS / XYZ 瓦片，也不要把 `SmtTileLayer` 硬塞进 OGR。瓦片见 sibling [`2026-09-13-tile-layer-provider-design.md`](2026-09-13-tile-layer-provider-design.md)；**禁止**用 OGR Memory / `SDBD:MEM` 冒充瓦片。
- 不要恢复 ADO / `msado15` / `DS_TB` / 按类型 `*Fcls` SQL。另一 agent 删 leftover ADO 文件时不要冲突。
- 不要为缺 GPKG / PG 的 SDK 手写读写器。
- 不要改 `content/public` 的稳定嵌入 API。
- 不要把 `SmtMap` 再设计成只持 `SmtLayer*`；地图文档改持 `OGRLayer*` / `GDALDataset*`（或同等 GDAL 句柄）。

## 现状 vs 目标

### 现状（2026-09-14 树）

```
SmtMap  (src/sdb/map)          矢量走 OGRLayer* / MapLayer；raster/tile leftover 仍 SmtLayer*
        ^
SmtDataSourceMgr
        |
        +-- 文件/库/内存  --> open_sdbd_dataset → SdbdDataset（内层 Memory / GPKG / PG / 文件）
        |                     + SdbdLayer（矢量）
        |                     + OgrRasterLayer   GDAL MEM / 文件栅格；CreaterRaster 经 /vsimem
        +-- CreateMemVecLayer --> SDBD:MEM + OGRLayer scratch
        +-- CreateMemRasLayer --> OgrRasterLayer + GDAL MEM（已离开 SmtMemRasLayer）
        +-- CreateMemTileLayer --> SmtMemTileLayer（瓦片另开 spec；不进 SDBD:MEM）

Select / Flash：CreateMemVecLayer()
```

阶段 4（栅格 MEM 等价）已落地：`OgrRasterLayer::Create/Open/CreaterRaster/GetRaster*` 挂真实 `GDALDataset`；编码 blob（CxImage `image_code`）存 `/vsimem`，`VSIGetMemFileBuffer` 供 render `GetRasterNoClone`。`memraslayer.cpp` / `SmtMemRasLayer` 已删。`mem/` 仅保留 tile。

要点（历史对照，多数设备已并入 SDBD）：

| 设备 | 图层实现 | 是否已经 OGR/GDAL | 持久化 |
| --- | --- | --- | --- |
| SDBD / `SdbdDataset` | `SdbdLayer` + `OgrRasterLayer` | 是 | GDAL |
| `SmtMemTileLayer` | 瓦片指针表 | 否（故意） | 进程内；见 tile-layer-provider |
| *(已删 WS)* | — | — | 见 [tile-layer-provider](2026-09-13-tile-layer-provider-design.md) |
| ADO `*Fcls` | 按要素类型子类 | 已退出 `src_all` | — |

`SmtLayer` 虚接口在 `src/sdb/layer/layer.h`（raster/tile leftover）。工具层用 `CreateMemVecLayer()` 当查询结果层。

### 目标

```
SmtMap / tools / catalog       持 GDALDataset* / OGRLayer* / OGRFeature*
        ^
        |  GDALOpenEx / CreateLayer / CreateFeature
        v
GDALDriver "SDBD"              前缀 SDBD: ；内部再开 Memory / GPKG / Shapefile / PG
        |
        v
third_party/gdal_sdk           唯一 GDAL
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
3. **sdbd 作为 GDAL 旁边的 HTTP/IPC 侧车** — sdbd 必须是 `GDALDriverManager` 里的驱动；JSON 路由只是对该驱动的调用，不是另一套数据集。

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
| `DS_WS` | `PROVIDER_SMARTGIS` | — | **源码已删**；`make_sdbd_open_target` 返回空；瓦片见 [tile-layer-provider](2026-09-13-tile-layer-provider-design.md) |

`DS_DB_ODBC` / `DS_DB_MYSQL` / `DS_DB_ORACLE` 保持枚举占位，`Create*` 返回 null 并打日志。不要再做一套设备。

URL 前缀（`sdb:` / `sfile:` / `smem:` / `sws:`）可继续写进 `.dsm`，便于人读；真正打开只看 `unType` + `unProvider` + 路径/连接串。

`CreateMemVecLayer` / `CreateMemRasLayer` 不再 `new SmtMemDataSource()`。它们构造一个 `OgrDataSource`（`DS_MEM`），`CreateVectorLayer` / `CreateRasterLayer`，然后丢掉外壳数据源指针的方式与今天相同——但图层背后是 Memory 驱动，不是 `vector<SmtFeature*>`。

`CreateTmpDataSource(DS_FILE_SMF)`（xcatalog）同样得到 `OgrDataSource`。目录树列出的是 `GDALDataset::GetLayerCount()`（外加栅格子数据集），不是 `ReadSmf`。

删除 `SmtSmfVecLayer` / `SmtSmfRasLayer` / `SmtMemVecLayer` 作为**产品路径上的类型**。`sde_smf` / `sde_mem` DLL 在调用方切完之前可以留在 GN 里当兼容壳，但新代码不得再 new 它们。

## 内存图层（`SmtMem*`）

今天的内存层承担三件事，必须拆开：

1. **未落盘草稿 / 查询结果 / 闪烁层** — `CreateMemVecLayer()`。
2. **文件/库图层的编辑缓存** — `SmtSmfVecLayer` 把整层 OGR 要素克隆进 `SmtMemVecLayer`，之后游标、Query、Append 都打在 mem 上；与磁盘的同步不完整。
3. **栅格字节缓冲与瓦片指针** — `SmtMemRasLayer` / `SmtMemTileLayer`。

**推荐：矢量走 GDAL Memory 驱动；适配器内保留一层薄 `SmtFeature*` 游标缓存（`OgrVectorLayer::features_` 已是这个形状）。不要把 `SmtMemDataSource` 留成第一类持久化设备。**

| 方案 | 做法 | 取舍 |
| --- | --- | --- |
| A. 全盘 Memory 驱动 | `CreateMem*` → `MEM:` dataset + `OgrVectorLayer` | 一条代码路径；Query 结果也是 GDAL layer。Memory 驱动在标准 GDAL 里几乎总是在。 |
| B. 永远保留 `SmtMem*` | 文件/库用 OGR，草稿仍用 `vector<SmtFeature*>` | 继续两套 Query / 游标 / Append。 |
| C. 混合（采用） | 事实源 = Memory / 文件 / 库的 `OGRLayer`；`Fetch` 后解码进 `features_` 只服务 `MoveFirst` / `GetFeature(i)` ABI | 不引入第三种图层类；大图层可后续改成按需 `GetFeature` 而不全量 `Fetch`。 |

栅格草稿：优先 `MEM` 栅格或内存 `GDALDataset`；在 raster I/O 补齐之前，`OgrRasterLayer` 可以暂存 buffer，但 **Create/Open 不得再假装成功却不挂 GDAL**。瓦片（`SmtMemTileLayer` 指针表 / 未来 `TileProvider`）见 [tile-layer-provider](2026-09-13-tile-layer-provider-design.md)，**不进 `SDBD:MEM`**，不阻塞矢量统一。

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

`src/sdb/datasource/ws` **源码树已删除**。`DS_WS` / `PROVIDER_SMARTGIS` 仅为枚举残留；`make_sdbd_open_target` 对 `DS_WS` 返回空串。leftover `SmtMemTileLayer` 仍是进程内 `SmtTile*` 指针表，**不是** GDAL Memory。

产品 2D 瓦片（HTTP(S) XYZ/WMTS、`MapLayer(kind=tile)`、与 GDAL WMS 的可选关系）一律见 sibling：

**[`2026-09-13-tile-layer-provider-design.md`](2026-09-13-tile-layer-provider-design.md)**

本文不恢复 WS 设备，不把瓦片塞进 OGR / `SDBD:MEM`。WFS 只读矢量若需要，另走 OGR WFS + 同一矢量适配器（仍非本文实现）。

## 栅格与瓦片 vs OGR 矢量

| 产品类型 | GDAL 对象 | v1 |
| --- | --- | --- |
| `SmtVectorLayer` / 点线面 / Anno / TIN / Grid | `OGRLayer` | 必须。Grid 优先做成同数据集里的 GDAL raster；否则已有 MultiPoint + `grid_row` / `grid_col`。 |
| `SmtRasterLayer` / `SmtFtChildImage` | `GDALDataset` 栅格 / 子数据集 / GPKG tiles | 必须补齐 `OgrRasterLayer`（今天 Create/Open 为 false）。缺栅格创建能力则 `SMT_ERR_UNSUPPORTED` + 日志，不发明 blob 表。 |
| `SmtTileLayer` / `SmtLayer_Tile` | 不是 OGR | **不在本文。** 见 [tile-layer-provider](2026-09-13-tile-layer-provider-design.md)；勿用 Memory 冒充。 |

同一 GPKG 可以既有矢量层又有栅格；`fill_layer_infos()` 已经扫 `GetLayerCount()`，应同时扫栅格子数据集并把 `unFeatureType` 标成 `SmtLayer_Ras`。`SmtMap` 仍按 `GetLayerType()` 区分绘制，不需要知道 GDAL。

## ABI

**推荐：GDAL 原生。**

保持：

- `dll_stem`（`SmtGisCore`、`SmtSDEGdalDevice`、`SmtSDEDeviceMgr`）与 `SDE_GDAL_EXPORT` 直到链接面另开清理。
- `.dsm` 头字节布局可暂留；打开只看路径 / `SDBD:` 连接串。
- `eDSType` / provider 枚举取值不重排（旧文件）。

删除（产品路径）：

- `SmtDataSource` / `SmtVectorLayer` / `SmtFeature`。不要僵尸包装器。
- 新代码不得再 `CreateMemVecLayer()` → `SmtMemVecLayer`。

允许：

- `SmtStyle` 与 `src/algorithm` 几何类型独立存在（不是要素门面）。
- `SmtTileLayer` / `DS_WS` 残留 → [tile-layer-provider](2026-09-13-tile-layer-provider-design.md)。
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
| 5 | 产品路径不再链接 `sde_smf` / `sde_mem` 的图层子类；测试覆盖文件 + mem +（可选）PG | `src_all` 可不依赖 SMF/Mem 图层实现 |
| 6（非 v1） | WFS → 同一矢量适配器；瓦片 → [tile-layer-provider](2026-09-13-tile-layer-provider-design.md) | — |

每个阶段都要 `build.bat` 与 `build.bat te` 保持绿。阶段 5 之前不要删 `src/sdb/datasource/smf`、`mem` 目录——先改调用方。不要在这些阶段里碰 ADO 删除。

实现计划另文（`docs/superpowers/plans/`）；本文不拆任务复选框。

## 风险

| 风险 | 处理 |
| --- | --- |
| 当前 `gdal_sdk` 没有 GPKG 和/或 PostgreSQL 驱动 | **API 仍是 GDAL Dataset/Layer。** `Open` / `Create` 查 `GetDriverByName`；缺失则日志 + false。测试对 GPKG / PG **跳过**（已有 `SMT_PG_DSN` 模式）；shapefile + Memory 是无条件必跑项，因为 ESRI Shapefile 与 Memory 在常见 SDK 里更稳。 |
| GPKG TIN | MultiPolygon 回退；产品类型仍是 `SmtFtTin`（DB spec 已定）。 |
| Shapefile 字段名 / 事务 | 不在产品层补洞；需要完整 GIS 库时用 GPKG 或 PostGIS。 |
| 旧 `.smf` + 目录 shp | 阶段 2 只读兼容；内部多 `GDALDataset*`。新工程不写 `.smf`。 |
| `OgrVectorLayer` 全量 `Fetch` 吃内存 | 与今天 SMF→mem 克隆同类；后续可按需读。不作为 v1 阻塞。 |
| 选择工具假定 mem 层可写 | 阶段 3 用 Memory 适配器满足同一虚接口。 |
| `OgrRasterLayer` 已挂 MEM；render 仍吃编码 buf | GDI/`layer_image_pixels` 继续 `GetRasterNoClone` + CxImage `image_code`；未解码格式时 band 可能仍是 1×1 占位。 |
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
7. `SMT_PG_DSN` 已设则跑 PostGIS 点往返（已有）。

**不要：** 要求 SQL Server / Access；要求实时 WMS；在本测试里编第二份 GDAL。

阶段 4：MEM 栅格 Create + `CreaterRaster` / `GetRasterNoClone` blob 往返 + `CreateMemRasLayer`（已加）。无 `MEM` 驱动则 skip。GeoTIFF `Open` 路径可测。

## 文档（实现变更时同步）

- `docs/build/src-layout.md` — `sdb/datasource` 写成「一个 OGR/GDAL 设备覆盖文件、库、内存」；SMF/Mem 标成兼容壳或已移除。
- `src/README.md` — 同上。
- 根 `README.md` — 仅当模块表仍把 SMF 写成独立文件后端时改，并刷新 **最后更新**。
- `docs/README.md` — 链到本文（与本文同一提交）。

## Success

- 产品路径上，文件 / 库 / 内存的打开、创建、列举、编辑、查询、关闭都经过一个 `OgrDataSource` 和一个 `GDALDataset`（Memory 驱动也算）。
- 不再出现新的 per-format / per-feature-type 图层子类。
- `SmtMap` 与工具看见 `GDALDataset` / `OGRLayer` / `OGRFeature`，不是 `SmtFeature`。
- `GetDriverByName("SDBD")` 非空；`SDBD:MEM:` 点层往返。
- `build.bat` 与 `build.bat te` 绿；shapefile + Memory 测试无条件跑；GPKG / PG 随 SDK 跳过或跑。
- 缺 GPKG 驱动时产品仍能打开 shapefile 与 Memory 层；错误信息说明缺的是驱动，不是 API。
- 无第二份 GDAL，无 Qt，无 ADO 回潮。

**最后更新：** 2026-09-14
