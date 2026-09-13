<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# 2D 地图瓦片：独立 Tile Provider

**Date:** 2026-09-13  
**Status:** active — Phase 0–2 (LRU + disk + WMTS parse + HTTPS via net/OpenSSL + Views AddBasemap) landed 2026-09-14  
**Scope:** 产品地图上的 **2D 瓦片图层**（XYZ / WMTS 一类 HTTP(S) 栅格瓦片）的数据面与挂接。不管桌面 chrome；不管 GDAL 文件/库/内存矢量；不管 GDAL 栅格文件路径（另一 agent）。

**Sibling:**

- 图层打开 / `SDBD:` / Memory 矢量 — [`2026-09-13-gdal-layer-management-design.md`](2026-09-13-gdal-layer-management-design.md)（**瓦片不进该文，也不进 `SDBD:MEM`**）
- `Feature` / `MapLayer` 组合 — [`2026-09-13-sdb-feature-maplayer-composition-design.md`](2026-09-13-sdb-feature-maplayer-composition-design.md)
- HTTP 客户端 — [`2026-09-13-net-asio-httplib-design.md`](2026-09-13-net-asio-httplib-design.md)
- 3D Tiles（`tileset.json`）— [`2026-09-13-model-render-compute-design.md`](2026-09-13-model-render-compute-design.md)（**不是本文**）

## Goal

为 `MapLayer` / `SmtMap` 提供一条 **独立于 OGR / SDBD Memory** 的 2D 瓦片供给与消费路径：

1. 打开 URL 模板或 WMTS 能力描述 → 按视口请求瓦片字节 → 交给现有 tessellate / GpuScene 画 textured quad。
2. 明确产品类型：`kind=tile` / leftover `LYR_TITLE`，**不是** `OGRLayer`，**不是** GDAL Memory 矢量/栅格草稿。
3. 切除已死的 `datasource/ws` 工厂语义；**已删除** `SmtMemTileLayer` 指针表与 `CreateMemTileLayer` 死工厂（`sde_mem` DLL / `datasource/mem` 已离树）。

## Non-goals

- **禁止**用 OGR Memory / `SDBD:MEM:` / `CreateMemVecLayer` 路径 **冒充**瓦片层。
- **禁止**把 `SmtTileLayer` 硬塞进 OGR（与 gdal-layer-management v1 一致）。
- **禁止**在本文周期恢复已删的 `src/sdb/datasource/ws` 树、无窗口瓦片发布栈、或 `src/web` / 自制 WMS-over-UDP。
- **不做** GDAL 文件栅格（GeoTIFF 等）实现；不改 `SmtMemRasLayer` / `OgrRasterLayer` 补齐工作。
- **不做** 3D Tiles / Assimp（`sdb::model::Tileset`）。
- **不做** WFS 矢量远程层（若需要，走 OGR WFS + 同一 `MapLayer` 矢量路径，另开或扩 gdal-layer，不进本文）。
- **不**默认假装「已评估并采用 GDAL WMS 驱动」；见下方选项与 Open questions。
- 不改 `content/public`；不引入 Qt。

## 现状（2026-09-14 树）

### 抽象与内存实现

| 符号 | 位置 | 语义 |
| --- | --- | --- |
| `SmtTileLayer` | `src/sdb/layer/layer.h` | 抽象层；`GetLayerType() == LYR_TITLE`；游标 + `AppendTile` / `GetTile*`（**保留**；render/scene/tessellate 仍依赖） |
| `sdb::tile::TileProvider` | `src/sdb/tile/` | HTTP(S) XYZ / WMTS 模板供给；进程内 LRU + 可选磁盘缓存；`ProviderTileLayer`；`make_xyz_map_layer` / `make_wmts_*` |
| `sdb::tile::TileCache` | `src/sdb/tile/tile_cache.h` | 进程内 LRU（默认 256） |
| `sdb::tile::TileDiskCache` | `src/sdb/tile/tile_disk_cache.*` | 可选磁盘缓存（目录可配、有界 entry） |
| `sdb::tile::parse_wmts_capabilities` | `src/sdb/tile/wmts.*` | 最小 Capabilities → URL 模板 |
| `base::SmtTile` | `src/base/core/bas_struct.h` | `pTileBuf` + `lTileBufSize` + `rtTileRect` + `lImageCode`；`typedef SmtWSTile` |
| `SmtMemTileLayer` | *(removed)* | 曾为 `vector<SmtTile*>` 指针表；死工厂已切除 |
| `CreateMemTileLayer` | *(removed from `SmtDataSourceMgr`)* | 无外部调用方；已安全删除 |

### 已删 WS 死树 vs 枚举残留

- **源码树 `src/sdb/datasource/ws` 已删除**（`docs/build/src-layout.md`：`SmtSDEWSDevice` / `sde_ws` → removed；瓦片 URL leftover 未入建）。
- CBM 索引里可能仍有旧 `ws*.cpp` 节点；**以磁盘为准**。
- **残留：**
  - `eDSType::DS_WS`、`eSmtWSProvider::PROVIDER_SMARTGIS`（`layer.h`）— 枚举取值不重排（旧 `.dsm`）。
  - `make_sdbd_open_target`：对 `DS_WS`（及若干占位 DB type）**直接返回空串**，不映射到 `SDBD:`。
  - `base::SmtWSTile` 仍是 `SmtTile` 别名。

gdal-layer-management 里「v1 继续 `new SmtWSDataSource`」已过时：工厂源码不在树里；打开路径应失败/拒绝，而不是假装还有 WS 设备。

### Render / Scene 消费点

| 调用 | 行为 |
| --- | --- |
| `sdb::scene::tessellate_tile_layer` | 遍历 `GetTile(i)`，按 `rtTileRect` 拼 quad；有 `pTileBuf` 则 `has_image` |
| `World::attach_tile_layer` | 合并瓦片包络，以 `NodeKind::kRasterLayer` 挂节点（`node->layer = layer`） |
| GpuScene（RHI 计划） | 有像素则 upload 纹理，否则实色包络 quad |

消费方今天认的是 **`SmtTileLayer*` + `SmtTile` 字节**，不是 `OGRLayer*`。

### 与邻近子系统的混淆点

| 概念 | 本文？ | 说明 |
| --- | --- | --- |
| `SmtMemTileLayer` | **已删** | 死工厂切除；勿复活指针表 |
| GDAL Memory / `SDBD:MEM` | 否 | 矢量（及未来栅格草稿）专用 |
| GDAL 文件/库栅格 | 否 | `OgrRasterLayer` / GPKG tiles 等 |
| `sdb::model::Tileset` | 否 | 3D Tiles `tileset.json` |
| 已删 map service / 瓦片发布 | 否 | 不恢复 |

## 目标架构

### 选项

| 方案 | 做法 | 优点 | 缺点 |
| --- | --- | --- | --- |
| **A. 独立 `TileProvider`（推荐）** | 新模块（建议 `sdb::tile` 或 `sdb/datasource/tile`）持 URL 模板 / WMTS 端点；用 `net::HttpClient` 拉 PNG/JPEG；产出 `TileImage`（字节 + 世界矩形 + z/x/y）；`MapLayer(kind=tile)` 持 provider；tessellate 读 provider 可见集 | 边界清晰；不污染 OGR/SDBD；与现有 `SmtTile` 消费面可薄适配 | 需自管缓存、重试、坐标系约定 |
| B. 仅 GDAL WMS/WMTS 驱动 | `GDALOpenEx` + WMS XML；把 dataset 当栅格源 | 少写 HTTP | 产品模型仍非 OGR；配置 XML 重；SDK 是否编进 WMS 未评估；易与「图层走 GDAL」口号混谈成硬塞 OGR |
| C. 永远扩展 `SmtMemTileLayer` | ~~继续指针表~~ **已否决并删除** | — | 曾混淆 Memory 矢量 DLL；无 HTTP |

### 推荐：方案 A

```
SmtMap / MapLayer(kind=tile)
        |
        v
sdb::tile::TileProvider          打开 URL 模板或 WMTS Capabilities / 模板
        |
        +-- net::HttpClient      GET tile（HTTP + HTTPS）
        +-- TileCache            进程内 LRU
        +-- TileDiskCache        可选磁盘（目录可配）
        |
        v
TileImage / 适配为 base::SmtTile   供 tessellate_tile_layer / GpuScene
```

**与 GDAL WMS 的关系（默认立场）：**

- v1 **不**把 TileProvider 实现成「内部必开 GDAL WMS」。
- 若后续评估（驱动已进 `gdal_sdk`、配置成本可接受、与现有 `TileImage` 映射清晰），允许 **可选后端** `GdalWmsTileBackend`，仍挂在同一 `TileProvider` 门面下，**不**把瓦片层登记成 `OGRLayer`，**不**进 `SDBD:MEM`。
- 在评估完成前，文档与代码注释不得写「已用 GDAL WMS」。

命名空间：公共最多两层（如 `sdb::tile`）；内部放 `sdb::tile::detail`。新 API 函数 `snake_case`。

### `MapLayer` / `SmtMap` 挂接

沿用 composition design：

- `MapLayer` 可 `kind=tile`（或 `layer_type() == LYR_TITLE`）。
- 迁移期：`from_leftover(SmtTileLayer*)`；产品挂接：`sdb::tile::make_map_layer(shared_ptr<TileProvider>)` / `make_xyz_map_layer(url)` / `make_wmts_map_layer*`（等价 `from_tile_provider`；放在 `tile/` 以免 gis↔tile GN 环），**`ogr()` 为 null**。
- `SmtMap` 加层/绘制按 `layer_type` 分支；矢量路径继续 OGR；瓦片路径不调用 `CreateFeature`。

### HTTP(S) 协议面（v1）

| 能力 | v1 | 备注 |
| --- | --- | --- |
| XYZ / TMS 风格 URL 模板（`{z}/{x}/{y}`） | 必须 | 与 `net::HttpClient` 对齐 |
| HTTPS | **已接线** | 依赖 net + OpenSSL |
| WMTS | **最小** | Capabilities ResourceURL / GetTile 模板；夹具单测 |
| 本地目录瓦片（`file://` 或路径模板） | 可选 | 不经 HTTP |
| 认证头 / token | Open question | 勿在 v1 发明第二套 net API |

## 与 sdbd / Feature / MapLayer 的边界

| 边界 | 规则 |
| --- | --- |
| `SDBD:` / sdbd 驱动 | **不**增加 `SDBD:TILE:` / `SDBD:WS:`。瓦片不是 GDAL Dataset 产品路径。 |
| `Feature` / `OGRFeature` | 瓦片层 **无** 要素游标；不要为每块瓦片造假 `OGRFeature`。 |
| `MapLayer` | `kind=tile`；持 provider 或 leftover `SmtTileLayer*`；与 `kind=vector` / `kind=raster` 并列。 |
| `DS_WS` 枚举 | 可保留取值；`Create*` / `make_sdbd_open_target` **拒绝**；新工程用 TileProvider 连接描述（URL 字符串或小 JSON），不写回 WS 设备。 |
| `SmtMemTileLayer` | **已删**；新代码不得再作为「远程瓦片」实现。 |
| 栅格 `SmtMemRasLayer` / GDAL raster | 另一轨；整幅影像 ≠ XYZ 瓦片集。 |

## 迁移阶段

### Phase 0 — 死工厂切除（已落地）

1. 确认无 GN/`#include` 指向已删 `datasource/ws`。
2. 任何仍 `CreateTmpDataSource(DS_WS)` / `PROVIDER_SMARTGIS` 的调用方改为 **显式失败 + 日志**（或 UI 禁用），不要 new 幽灵类型。
3. 更新 gdal-layer / src-layout 表述：WS **已删**，不是「v1 维持 WS」。
4. **已完成**：删除 `CreateMemTileLayer` / `DestoryMemTileLayer` / `SmtMemTileLayer` / `sde_mem` / `datasource/mem`；抽象 `SmtTileLayer` 保留。

### Phase 1 — 新实现（已落地）

1. **已完成** `sdb::tile::TileProvider` + XYZ GET + `tiles_for_viewport`（Web Mercator / EPSG:3857）。
2. **已完成** 适配层 `ProviderTileLayer`（`SmtTileLayer` 具体类，非 mem/DLL）→ `tessellate_tile_layer` / `attach_tile_layer`。
3. **已完成** `sdb::tile::make_map_layer(shared_ptr<TileProvider>)` → `MapLayer(kind=tile)`（`ogr()` 为空；避免 gis↔tile GN 环，等价 composition 文的 `from_tile_provider`）。
4. **已完成** 单测 `//src/sdb/tile:tile_test`：URL 模板拼装、viewport→tile 索引、mock HTTP、`has_image`；直播 CDN **SKIP**。
5. **HTTPS：** **已启用**（net + OpenSSL）。直播 CDN 单测仍 SKIP 外网。

### Phase 2 — 收口抽象

1. ~~删除 `SmtMemTileLayer`~~ **已完成（并入 Phase 0）**。
2. `SmtTileLayer` 抽象：要么缩成 leftover 兼容头并逐步删除，要么改为非虚的 provider 适配器；**禁止**再增加 mem 子类。（`ProviderTileLayer` 已是唯一 concrete。）
3. 清理 `SmtWSTile` 别名（若无引用）。
4. [x] **进程内 LRU**（`TileCache`，默认 256；同 key 二次 fetch 不二次 HTTP；单测覆盖）。
5. [x] **磁盘缓存** — 可选目录 + 有界 entry；命中磁盘不二次 HTTP（单测）。
6. [x] **WMTS** — 最小 Capabilities ResourceURL / GetTile 模板归一化；夹具 XML 单测。
7. [x] **Views「添加在线底图」** — `AddBasemapDialog` + Catalog 菜单 `catalog.layer.add_basemap`；`app/views` 调 `make_xyz_map_layer` / `make_wmts_map_layer` 校验后 `CatalogCall(add_basemap)`。完整 scene 挂接仍待 content 认 `kind=tile`。

并行约束：GDAL 栅格 agent **不**改 tile provider；本文作者 **不**改 mem ras（已迁 `OgrRasterLayer`）。

## Open questions（更新）

1. **HTTPS：** **已决（2026-09-14）** — net 启用 `CPPHTTPLIB_OPENSSL_SUPPORT` + `//third_party:openssl`（Shining Light → `.install`）。TileProvider 可开 `https://` 模板；直播 CDN 测仍 SKIP 外网。
2. **坐标系：** **已决（v1）：** XYZ 写死 Web Mercator（EPSG:3857）；与地图 SRS 不一致时仅警告（后续 PROJ）。
3. **WMTS：** **已决最小落地**；完整 TileMatrixSet / 多图层选择器延后。
4. **缓存：** **进程内 LRU + 可选磁盘** 已落地。
5. **GDAL WMS 后端：** 评估前禁止当默认实现宣传。
6. **`NodeKind`：** `attach_tile_layer` 今日用 `kRasterLayer`；是否需要独立 `kTileLayer`？（未决，不阻塞）
7. **旧 `.dsm` 含 `DS_WS`：** 打开时迁移提示 vs 静默忽略？（未决）
8. **Views UI：** 对话框 + Catalog 菜单已挂；content `add_basemap` 完整画布挂接仍待。

## 验收（设计级）

- 存在本文；gdal-layer 明示瓦片见 sibling，且 **不进 `SDBD:MEM`**。
- 代码落地后：无 OGR Memory 瓦片冒充；无恢复 `datasource/ws` 产品路径；`MapLayer(kind=tile)` 可挂 HTTP(S) XYZ；同 key LRU / 磁盘命中不二次 HTTP；WMTS 夹具可解析。

## 本文不产出

- plan 勾选清单（需要落地时另开 `docs/superpowers/plans/2026-09-1x-tile-layer-provider.md`）。
- 仍延后：完整 WMTS 矩阵 UI、content 画布 `kind=tile` 挂接、独立 `kTileLayer`。

**最后更新：** 2026-09-14
