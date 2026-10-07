<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# 业界差距矩阵：QGIS / Cesium / ArcGIS Pro

**Status:** active  
**Date:** 2026-09-30  
**Scope:** 以 SmartGIS 产品愿景（桌面原生 C++、「走进去干活」的图）为锚，对照三家标杆的能力下限；把差距映射到现有 `docs/superpowers` 规格/计划与建议里程碑。  
**非目标：** 不宣称要对齐三家全部产品线；不重新打开已锁定的拒绝项（Qt、Cesium Native 全家桶、第二套 GEOS、产品 Web GIS/mapd）。

相关真源：

| 主题 | 文档 |
| --- | --- |
| 分层与 OSS 对照 | [`src-layout.md`](src-layout.md)、[`../../src/README.md`](../../src/README.md) |
| UI 终局 | [`ui-views-skia.md`](ui-views-skia.md) |
| 模型 / 渲染 / 计算 | [`specs/2026-09-13-render-rhi-scene-design.md`](specs/2026-09-13-render-rhi-scene-design.md) |
| Map2D 差距执行清单 | **§8**；计划 [`plans/2026-09-30-map2d-gap-pin.md`](plans/2026-09-30-map2d-gap-pin.md) |
| Map3D 差距执行清单 | §3.2.1；计划 [`plans/2026-09-30-map3d-gap-pin.md`](plans/2026-09-30-map3d-gap-pin.md) |

---

## 1. 怎么读

### 1.1 标杆角色（不是「全面抄」）

| 标杆 | SmartGIS 向它学什么 | 明确不追 |
| --- | --- | --- |
| **QGIS** | 桌面制图、Processing 可发现性、GDAL 数据面、插件/Python 日常扩展 | Qt GUI；与 QGIS 插件 ABI 兼容 |
| **Cesium**（及地球级 3D） | 流式 LOD、全球地形/影像、3D Tiles 规模感、稳定相机 | Cesium Native 一锅端；Ion 云生态 |
| **ArcGIS Pro** | 专业制图与布局、多用户/版本编辑下限、三维场景与多维时间轴产品完整度 | Portal/SaaS 全家桶；许可与封闭格式深度 |

### 1.2 成熟度字母

| 级 | 含义 |
| --- | --- |
| **A** | 产品主路径可用，日常可依赖 |
| **B** | 主能力有，缺深度或规模 |
| **C** | 骨架 / 子集 / demo；规格已有 |
| **D** | 几乎没有；或仅 leftover 旧路径 |
| **—** | 刻意不做（见 Non-goals） |

「SmartGIS 现状」指 **Views 新栈主路径** 的可用性；仅 leftover MFC 能做的记为 **D（新栈）/ leftover**。

### 1.3 里程碑（建议顺序，可并行车道）

| 里程碑 | 一句话成功判据 | 大致对标 |
| --- | --- | --- |
| **M0** | Views 主壳能独立完成：开图 → 漫游 → 查属性 → 简单编辑；日常停编 MFC。计划：[`plans/2026-09-20-m0-views-main-path.md`](plans/2026-09-20-m0-views-main-path.md) | 桌面 GIS 下限 |
| **M1** | Style + 瓦片 + 标注/中国底图闭环；出图可打印一页（已绿） | ≈ QGIS 基础制图 |
| **M2** | Processing 工具箱 ≥10 算子 + buffer/clip（`m2-*-ok`；已绿） | ≈ QGIS Processing 入口 |
| **M3** | DEM + 3D Tiles 流式 + 大气（`m3-*-ok`；已绿） | ≈ Cesium / Pro 3D 观感下限 |
| **M4** | 乐观冲突 + `content::` 嵌入（`m4-*-ok`；已绿） | ≈ Pro / Enterprise 入门 |

M0–M4 验收口令均挂在 `SmartGIS.exe --self-test`；计划：`docs/superpowers/plans/2026-09-20-m0-*.md`、`2026-09-20-m1-*.md`、`2026-09-27-m{2,3,4}-*.md`。

---

## 2. 总览矩阵

| 能力域 | QGIS | Cesium | ArcGIS Pro | SmartGIS 现状 | 目标级 | 主规格 / 计划 | 里程碑 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 桌面壳 / 控件 | A | — | A | B（M0 自测口令绿；深度编辑仍弱）+ leftover | A | [ui-views-mfc-migration](specs/2026-09-27-views-desktop-shell-design.md) **active**；[ui-views-skia](ui-views-skia.md)；[shell-compile-gate](archive/specs/2026-09-19-shell-compile-gate-design.md) **active**；[M0 plan](plans/2026-09-20-m0-views-main-path.md) | **M0** |
| 数据模型 / OGR | A | B | A | B（GDAL/SDBD 已定） | A | [ogr-db](specs/2026-09-13-gdal-layer-management-design.md)、[gdal-layer](specs/2026-09-13-gdal-layer-management-design.md)、[feature-maplayer](specs/2026-09-13-gdal-layer-management-design.md) **accepted** | M0–M1 |
| 2D 瓦片底图 | A | A | A | **B**（XYZ/WMTS + cache；**MVT 本地 decode**；URL vector bind 仍开） | B→A | [tile-layer-provider](specs/2026-09-13-gdal-layer-management-design.md) **active**；gap pin [`plans/2026-09-30-map2d-gap-pin.md`](plans/2026-09-30-map2d-gap-pin.md) P0-1 done / P1-1 | **M1**+ |
| 制图样式 / 符号 | A | B | A | **B**（casing+hillshade+heatmap/extrusion v1+mini expr landed；缺 SDF/完整 VM/GPU 密度） | B→A | [sdb-style](specs/2026-09-13-gdal-layer-management-design.md)；[render-rhi §Map2d richness](specs/2026-09-13-render-rhi-scene-design.md)；[`map2d-hillshade-line-casing`](plans/2026-09-30-map2d-hillshade-line-casing.md) 近收口；gap pin P1-2/P1-5/P2 | **M1**+ |
| 注记 / 离线底图 | A | B | A | **C→B**（`m1-labels-ok` + along-line collision v1；**china 产品环 P0-2 已绿**；CJK 字体仍 P1-3） | B | [china-city-map-plpt](specs/2026-09-13-gdal-layer-management-design.md)；gap pin P1-3 | **M1**+ |
| 布局打印 | A | — | A | **C**（Views `PrintPreviewDialog` 壳 + viewport BMP；**无**页布局/比例尺/图例引擎） | B | [plugin-host](specs/2026-09-13-plugin-host-design.md) print；gap pin P0-3 | M1 末 |
| 编辑 / 捕捉 / 拓扑 | A | — | A | C（`EditSession`/`feature_edit` 有；**新栈无 snap/拓扑**）+ leftover | B→A | [tool-event-dispatch](specs/2026-09-13-tool-event-dispatch-design.md)；gap pin P0-4/P1-4 | **M0** / M4 |
| Processing / 分析 | A | — | A | C–D（algorithm 核有，无工具箱） | B | [algorithm-layer-oss](specs/2026-09-13-algorithm-layer-oss-design.md) **accepted**；[plugin-host](specs/2026-09-13-plugin-host-design.md) Processing 契约 | **M2** |
| RHI / 双场景 | — | A | A | **B**（`gis::World` + `vista::GpuScene` + `Scene3dGpuPresent`；SP4 主刀已绿；leftover `DemHeightField` / `Scene` octree 仍 Deferred） | A | [render-rhi-scene](specs/2026-09-13-render-rhi-scene-design.md) **accepted**；[legacy umbrella SP4](specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md)；[scene3d-world-gpuscene plan](plans/2026-09-19-scene3d-world-gpuscene.md)；[**map3d-gap-pin**](plans/2026-09-30-map3d-gap-pin.md) | **M3**+ |
| 3D 模型 / Tiles | B | A | A | **B**（`TilesetStreamSession` + present pump/`GpuScene` cache；b3dm/glb 解码；M3 fixture + `scene3d_presenter_test` 绿；城市场规模 / i3dm·pnts → P1-C） | B→A | render-rhi-scene §model；`vista/assets/tileset`；[m3 plan](archive/plans/2026-09-27-m3-city-3d-stream.md)；[map3d-gap-pin](plans/2026-09-30-map3d-gap-pin.md) P0-B **done** | **M3**+ |
| 地形 DEM | B | A | A | **B–C**（`DemRaster` + `seed_china_dem_into_world` + mesh；**无**高度场瓦片/clipmap） | B | [legacy umbrella DEM](specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md)；[map3d-gap-pin](plans/2026-09-30-map3d-gap-pin.md) P0-C | M3+ |
| 点云 | B | B | A | **C→B**（LAS/LAZ/txt + World/`kPointCloud` + chunk/LOD；PDAL stub；活 PDAL install Deferred） | B | [plugin-host §world3d](specs/2026-09-13-plugin-host-design.md)；[world3d-pointcloud-las](plans/2026-09-30-world3d-pointcloud-las.md)；[map3d-gap-pin](plans/2026-09-30-map3d-gap-pin.md) P1-A | M3 后 |
| 大气 / 海 / 云 | — | B | B（多维） | **C→B**（upgrade Phase 0–3.3 多已勾；**Phase 3.4 总验收**仍开） | B（差异化） | [atmosphere](specs/2026-09-13-render-rhi-scene-design.md) **accepted**；[upgrade plan](plans/2026-09-20-atmosphere-ocean-cloud-upgrade.md) **active**；[map3d-gap-pin](plans/2026-09-30-map3d-gap-pin.md) P1-B | M3 旁路 |
| OGC / Web 服务栈 | A | A | A | —（产品 Web GIS 已删） | — / 客户端子集 | net + TileProvider；**不**复活 mapd | 可选客户端 |
| 远程 sdbd | — | — | B | B–C（WSL 客户端规格） | B | [sdbd-wsl-client](specs/2026-09-13-gdal-layer-management-design.md) **accepted** | M4 |
| 插件 / Python | A | B | A | C（Host + embed） | B | [plugin-host](specs/2026-09-13-plugin-host-design.md)、[plugin-full-upgrade](specs/2026-09-13-plugin-host-design.md) **accepted** | M2–M4 |
| 嵌入 SDK | B | A | A | C（`content::`） | B | content/public；[ui-shell-multiprocess](ui-shell-multiprocess.md) | M4 |
| 跨平台 | A | A | Windows 主 | D（Windows-first） | C（后置） | — | 不进 M0–M3 |
| 企业版本库 / 身份 | B | — | A | D | C→B | 无独立规格 | **M4** |

---

## 3. 分域说明（差距 → 动作）

### 3.1 对标 QGIS（桌面干活）

Map2D 可勾选细项见 **§8**。本表保留壳/工具/Processing 等非纯 Map2D 行。

| 缺口 | 现状证据 | 建议动作 | 里程碑 |
| --- | --- | --- | --- |
| 单一主 UI | Views 与 MFC 双轨；`legacy_app` opt-in | 完成 MFC→Views parity 门禁；日常只编 `build.bat app` | M0 |
| 工具迁到 Workspace | dispatch 已有；行为仍在 leftover | 执行 tool-behavior-migration + strangler，禁新功能进 `legacy/tool` | M0 |
| Style 端到端 | **已抬升：** `ResolvedPaint` + casing/hillshade/heatmap/extrusion v1 + mini expr（`src/gis/style`）；仍缺算术族/SDF/GPU 密度 | 按 §8 P1-2 / P1-5 / P2 推进；勿重复做已落地的 P0 richness | M1+ |
| 瓦片 / MVT | XYZ/WMTS + cache；**MVT 本地 decode 已落地**（`mvt.cc`）；Style vector URL bind 仍拒 | §8 P0-1 已关；P1-1 生产路径默认磁盘缓存 | M1+ |
| 注记 / china | `m1-labels-ok`；`collision.cc` along-line slots；**china loop P0-2 已绿** | §8 P1-3（字体/沿线产品化） | M1+ |
| Processing 入口 | algorithm 有核；工具箱口令见 M2（已绿骨架） | 深度算子与可发现性继续挂 plugin-host Processing | M2 |
| 打印布局 | **PrintComposer** 一页（地图+比例尺+图例）；`m1-layout-ok` | §8 P0-3 已关；多页见 P2-3 | M1 末 |
| 捕捉 / 拓扑 | **vertex/edge snap** 已落地（`feature_edit`）；拓扑仍缺 | §8 P0-4 已关；P1-4 拓扑 | M0 / M4 |

### 3.2 对标 Cesium（走进场景）

> 2026-09-30 核对：M3 骨架（`m3-dem-ok` / `m3-tiles-ok` / `m3-atmosphere-ok`）与 world3d LAS 主路径已落地；下表是相对 Cesium「走进场景」仍开的产品差距。可执行钉死表见 **§3.2.1**。

| 缺口 | 现状证据 | 建议动作 | 里程碑 |
| --- | --- | --- | --- |
| 双场景合拢收口 | `DemHeightField` 薄壳委托 `gis::DemRaster`；`seed_dem_height_field_into_world` → `seed_dem_raster_into_world`；present 只吃 World mesh | 保持薄壳；禁 Present 回潮厚 DEM | M3+ / P0-A **done** |
| 流式 3D Tiles 产品化 | `TilesetStreamSession` + present pump/`GpuScene` cache；`tileset_test` 绿；安静锁下 `scene3d_presenter_test` exit 0（2026-09-30） | 真实 glb / i3dm·pnts → P1-C | M3+ / P0-B **done** |
| 全球/城市地形瓦片 | `DemRaster::build_mesh_window` + `seed_dem_view_tiles_into_world`（远 1 / 近 2×2·4×4，≤65536 verts）；`rebuild_terrain_mesh` 同步 GpuScene | clipmap/morph 观感挂 P2-C | M3+ / P0-C **done** |
| 大气差异化总验收 | upgrade Phase 0–3.3 多已勾；Phase 3.4 开 | 对照 upgrade §1.2 五条关闸 | M3 旁路 / P1-B |
| 点云生产路径 | LAS/LAZ/txt + chunk/LOD 已有；PDAL 活装 Deferred | 大云稳定打开 + 文档化 PDAL 可选 | M3 后 / P1-A |

### 3.2.1 Map3D 钉死清单（2026-09-30）

核对依据：CBM `smartgis` + `src/vista/component/world/**`、`src/vista/assets/tileset/**`、`src/vista/scene/**`、`src/content/browser/present/scene3d/**`、相关 plans。刻意不追：Cesium Native、完整 GCM/影视级海洋、产品 Web GIS。

执行 checkbox：[`plans/2026-09-30-map3d-gap-pin.md`](plans/2026-09-30-map3d-gap-pin.md)。

| ID | 缺口 | 现状证据（路径 / 口令） | 建议动作 | 验收判据 | 挂靠规格或计划 | 优先级 |
| --- | --- | --- | --- | --- | --- | --- |
| **P0-A** | leftover DEM / 双场景未完全合拢 | **done (2026-09-30):** `DemHeightField` 持有 `gis::DemRaster` 并转发 load/sample/mask/mesh；`dem_to_world` → `seed_dem_raster_into_world`；无并行 heights_ 网格 | 保持薄壳；禁 Present 回潮 | `dem_stereo_test` + `dem_raster_test` 绿 | [legacy umbrella SP4](specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md)；[scene3d-world-gpuscene plan](plans/2026-09-19-scene3d-world-gpuscene.md)；[map3d-gap-pin](plans/2026-09-30-map3d-gap-pin.md) | **P0** |
| **P0-B** | 3D Tiles 未进产品 present 流 | **done (2026-09-30):** `TilesetStreamSession`；`Scene3dGpuPresent` attach + present pump；`GpuScene::set_tileset_content_cache`；`m3-tiles-ok` 相机改 URI；`tileset_test` 绿；`build.bat debug scene3d_presenter_test` + exe exit 0（安静锁；曾 AV 未复现） | 真实 glb / 内容面 → P1-C | 相机移动改 `visible_uris`；缓存 ≤ budget；失败 URI 不崩；`tileset_test` + present smoke 绿 | [m3-city-3d-stream](archive/plans/2026-09-27-m3-city-3d-stream.md)；[map3d-gap-pin](plans/2026-09-30-map3d-gap-pin.md) | **P0** |
| **P0-C** | DEM 无瓦片化 / 无 clipmap | **done (2026-09-30):** `build_mesh_window` + `seed_dem_view_tiles_into_world`（预算 65536）；`rebuild_terrain_mesh` 按视距 1/2×2/4×4；`dem_raster_test` 断言瓦数/顶点数 | clipmap/morph → P2-C | china/city DEM orbit 顶点有预算；放大换细瓦；测绿 | [legacy umbrella DEM](specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md)；[map3d-gap-pin](plans/2026-09-30-map3d-gap-pin.md) | **P0** |
| **P1-A** | 点云「生产可用」未钉死 | `load_point_cloud` / LAS·LAZ·txt；`set_pointcloud_points` chunk+LOD；`world3d` 命令；`pointcloud_test`；PDAL stub | 大云（≥1M）稳定打开 + 文档化 in-tree vs PDAL | showcase/interact 出画；>500k 自动 thin；`pointcloud_test` 绿 | [plugin-host](specs/2026-09-13-plugin-host-design.md) §world3d；[world3d-pointcloud-las](plans/2026-09-30-world3d-pointcloud-las.md) | **P1** |
| **P1-B** | 大气产品总验收未关 | upgrade Phase 0–3.3 多 `[x]`；Phase 3.4 开；`m3-atmosphere-ok`；`--atmosphere-showcase` | 按 upgrade §1.2 五条逐项关闸并记证据 | 五条均有可复述证据；大气自测不回归 | [atmosphere upgrade](plans/2026-09-20-atmosphere-ocean-cloud-upgrade.md)；[render-rhi-scene](specs/2026-09-13-render-rhi-scene-design.md) Atmosphere | **P1** |
| **P1-C** | Tiles 内容面偏窄 | b3dm/glb 主路径；i3dm/pnts 多为路径识别；`select_tiles_limited` 截断偏粗 | 内容矩阵 + 诚实 skip；SSE/预算策略产品默认 | README 内容矩阵；unsupported → `put_failed` 无重试风暴 | [render-rhi-scene](specs/2026-09-13-render-rhi-scene-design.md)；[map3d-gap-pin](plans/2026-09-30-map3d-gap-pin.md) | **P1** |
| **P2-A** | `Scene` octree 仅 AABB 镜像 | `seed_smt_scene_aabbs_into_world`；SP4 Deferred | 查询路径进一步委托 World | 相关 leftover 查询测绿；无双真相 | [scene3d-world-gpuscene plan](plans/2026-09-19-scene3d-world-gpuscene.md) Deferred | **P2** |
| **P2-B** | 活 PDAL 安装未产品化 | `pdal_io` stub → `pdal_not_built`；plan Task 5 注明 live Deferred | 可选 `build.bat t pdal` 文档/机时允许时绿 | stub 必绿；有安装时 processing 读样例 | [world3d-pointcloud-las](plans/2026-09-30-world3d-pointcloud-las.md) Task 5 | **P2** |
| **P2-C** | 地形 LOD 观感低于 Cesium 中档 | 单 coarse mesh / 无 morph | clipmap 或等价中档观感（非 Cesium terrain 全家桶） | 城市场演示可述「近细远粗」 | [render-rhi-scene](specs/2026-09-13-render-rhi-scene-design.md)；[map3d-gap-pin](plans/2026-09-30-map3d-gap-pin.md) | **P2** |

### 3.3 对标 ArcGIS Pro（专业完整度）

| 缺口 | 现状证据 | 建议动作 | 里程碑 |
| --- | --- | --- | --- |
| 专业制图深度 | 子集 Style；无布局引擎 | M1 先到「能出图」；深度符号/地图册可后置 | M1 / 后置 |
| 多维时间轴 | 大气 FieldStore 时间维方向 | 与 atmosphere upgrade 共用时间轴控件，不另起 GCM | M3 |
| 多用户编辑 | GPKG/PostGIS 打开；无版本/冲突 | M4：基于 PostGIS 的乐观锁或简易 checkout；先不做完整 branch versioning | M4 |
| 拓扑 / 规则 | 编辑会话有；拓扑弱 | 捕捉 + 基础拓扑校验（叠盖/缝隙）挂 EditSession | M4 |
| Enterprise 身份 | 无 | 后置；嵌入场景用宿主身份即可 | 后置 |

---

## 4. 已有规格覆盖 vs 空白

### 4.1 已有 living 规格（可直接执行）

| 域 | 规格状态（2026-09-20 快照） |
| --- | --- |
| 数据 / GDAL | accepted |
| RHI / 模型伞状 | accepted |
| 工具 dispatch | accepted（v1 landed）；行为迁移 **active** |
| Views 迁移 / shell gate | **active** |
| Style / Tile / china_city | **active** |
| World↔scene3d（SP4） | plan 主刀 landed；Deferred + [map3d-gap-pin](plans/2026-09-30-map3d-gap-pin.md) 收口（design 已归档，决策在 umbrella / render-rhi-scene） |
| 大气 | accepted + upgrade plan **active**（Phase 3.4 开） |
| 插件 Host | accepted（§world3d pointcloud LAS 主路径 landed） |

### 4.2 建议新开规格（当前空白）

仅在现有文档盖不住时新开；默认先尝试扩写已有 active 规格。

| 建议题目 | 为何需要 | 优先挂在 |
| --- | --- | --- |
| Processing 工具箱（Views + Registry） | algorithm 与 PluginHost 契约之间缺产品面 | M2；可扩 [plugin-host](specs/2026-09-13-plugin-host-design.md) §Processing |
| 2D 布局打印（Views） | 有 `PrintPreviewDialog` 壳，缺页布局/比例尺/图例 | M1 末；§8 P0-3；可挂 plugin-host print |
| 3D Tiles **产品**流 + DEM 瓦片 | P0-B/P0-C **done**（present stream + DEM view tiles）；城市场规模 / i3dm·pnts / clipmap 观感挂 P1-C·P2-C（**勿新开 design**） | [map3d-gap-pin](plans/2026-09-30-map3d-gap-pin.md)；扩 [render-rhi-scene](specs/2026-09-13-render-rhi-scene-design.md) / umbrella DEM |
| 企业编辑下限（PostGIS） | 无独立决策 | M4 |

---

## 5. 刻意不追（避免范围膨胀）

| 项 | 理由（仓内已锁） |
| --- | --- |
| Qt | [`no-qt`](../../.cursor/rules/repo/no-qt.mdc)；Views + Skia 终局 |
| Cesium Native 一锅端 | model-render-compute 已拒 |
| 第二份 GEOS / PROJ | 只走 `//third_party:gdal` |
| 产品 Web GIS / mapd / 服务端 WMS | 已删；Tile/HTTP 客户端可留 |
| 完整 GCM / 影视级海洋 | atmosphere Non-goals |
| Linux/mac 一等公民 | 不进 M0–M3 关键路径 |

---

## 6. 验收口令（每个里程碑一条）

| 里程碑 | 可演示 / 可测口令 |
| --- | --- |
| **M0** | 仅 `SmartGIS.exe`：打开 GPKG → 平移缩放 → FeatureInfo → 追加一条线并保存；`build.bat e2e` 绿。执行计划：[`plans/2026-09-20-m0-views-main-path.md`](plans/2026-09-20-m0-views-main-path.md)（2026-09-20：`m0-line-ok` / `m0-featureinfo-ok` / `m0-save-ok` + e2e 绿） |
| **M1** | Style JSON 驱动矢量着色 + XYZ 底图 + china_city 注记可读；导出一页 BMP（`m1-labels-ok` / `m1-style-ok` / `m1-basemap-ok` / `m1-export-ok`；exit 70–73）。执行计划：[`plans/2026-09-20-m1-carto-style-tile-export.md`](plans/2026-09-20-m1-carto-style-tile-export.md)（2026-09-20：`--self-test` + `build.bat e2e` 绿） |
| **M2** | Views「处理」面板 ≥10 算子；buffer/clip 写回。计划：[`plans/2026-09-27-m2-processing-toolbox.md`](archive/plans/2026-09-27-m2-processing-toolbox.md)（`m2-panel-ok` / `m2-buffer-ok` / `m2-clip-ok`；exit 80–82；2026-09-27：`--self-test` + `build.bat e2e` / `te` 绿） |
| **M3** | DEM + 3D Tiles 流式 + 大气开关。计划：[`plans/2026-09-27-m3-city-3d-stream.md`](archive/plans/2026-09-27-m3-city-3d-stream.md)（`m3-dem-ok` / `m3-tiles-ok` / `m3-atmosphere-ok`；exit 90–92；2026-09-27：`--self-test` + `build.bat e2e` / `te` 绿） |
| **M4** | 双会话乐观冲突。计划：[`plans/2026-09-27-m4-enterprise-edit-embed.md`](archive/plans/2026-09-27-m4-enterprise-edit-embed.md)（`m4-conflict-ok`；exit 100；HWND-free embed sample 已移除） |

---

## 7. 维护

- 规格 **Status** 变更或里程碑收口时，**同变更集** 更新本表对应行。
- 里程碑完成后：把可复述事实写入本文件或 `src/` 模块 README；相关 plan 按 [superpowers-docs](../../.cursor/rules/repo/superpowers-docs.mdc) 归档。
- **差距钉死表：**
  - **Map2D：** **§8** + [`2026-09-30-map2d-gap-pin.md`](plans/2026-09-30-map2d-gap-pin.md) — Map3D 代理 **勿覆盖**。
  - **Map3D：** §3.2.1 + [`2026-09-30-map3d-gap-pin.md`](plans/2026-09-30-map3d-gap-pin.md) — Map2D 代理 **勿覆盖**。

---

## 8. Map2D 钉死清单（2026-09-30）

核对依据：CBM `smartgis` + `src/gis/style/**`、`src/gis/tile/**`、`src/vista/component/map/**`、`src/content/browser/present/map2d/**`、`src/plugin/product/map2d/print/**`、`src/content/browser/document/**`、render-rhi §Map2d richness、[`2026-09-30-map2d-hillshade-line-casing.md`](plans/2026-09-30-map2d-hillshade-line-casing.md)。

刻意不追：Qt、Cesium Native、产品 Web GIS/mapd、第二套 GEOS、完整 MapLibre Native 链接。

执行 checkbox：[`plans/2026-09-30-map2d-gap-pin.md`](plans/2026-09-30-map2d-gap-pin.md)。

**已抬升（勿再当开缺口重复立项）：** dual-stroke casing、DEM hillshade underlay、heatmap/fill-extrusion v1、mini expression（`interpolate`/`match`/…）、along-line collision slots、M1 口令 `m1-*-ok`。

| ID | 优先级 | 缺口 | 现状证据（路径 / 口令） | 建议动作 | 验收判据 | 挂靠 |
| --- | --- | --- | --- | --- | --- | --- |
| P0-1 | **P0** | MVT / 矢量瓦片不可读 | **已关闭：** `src/gis/tile/provider/mvt.{h,cc}` protobuf 线解码 + gzip；`decode_mvt_to_map_frame`；fixture `testing/data/fixtures/mvt/roads_fixture.mvt(.gz)`；`tile_test` PASS。Style vector URL bind 仍 `kVectorUnsupported` | — | 本地 `.mvt` → MapFrame ≥1 矢量 draw item；unit 绿 | gap-pin P0-1 |
| P0-2 | **P0** | China 产品环未钉死 | **已关闭：** `testing/data` → GN `//testing/data:china_map_samples` 同步 `out/data/china_city.*` + `china_dem.tif`（PIN 对齐）；`py -3 testing/tools/loop_runner.py --suite map2d.china --no-build` exit 0；BMP `road_casing_frac`/`road_gold+casing` + `hillshade_soft_ok` | — | `map2d.china` loop exit 0；BMP casing / soft hillshade | gap-pin P0-2 |
| P0-3 | **P0** | 无页布局出图 | **已关闭：** `PrintComposer`（地图+比例尺+图例）→ BMP；`PrintPreviewDialog` Save 走页布局；`print_composer_test` PASS；self-test `m1-layout-ok` | — | mark + 一页可读 | gap-pin P0-3 |
| P0-4 | **P0** | 新栈无顶点/边捕捉 | **已关闭：** `feature_edit` `snap_to_features` / `snap_point`（vertex 优先 + edge 投影）；`move_selected_vertex` 吸附他要素；`feature_edit_test` | — | 命中误差 ≤ 容差 | gap-pin P0-4 |
| P1-1 | **P1** | 栅格瓦片缓存未成产品默认 | `TileCache` / `TileDiskCache` 有实现；主路径未必默认开 | Views 打开 XYZ/WMTS 默认挂磁盘缓存 | 二次打开同 extent 无全量重拉（日志/计数可证 hit） | gdal-layer tile；gap-pin P1-1 |
| P1-2 | **P1** | Style 表达式缺算术族 | `src/gis/style/README.md` / `eval/expression.*`：有 interpolate/match；无 `+`/`*` 等 | 扩 mini eval；`style_test` 覆盖 data-driven width/color | `style_test` PASS；china Style 可用 zoom×attr 驱动线宽 | render-rhi §Mini expression；gap-pin P1-2 |
| P1-3 | **P1** | 注记产品化（字体/沿线） | `collision.cc` slots landed；`m1-labels-ok`；CJK 路径仍脆 | 稳定 CJK 字体；showcase/self-test 断言沿线注记可见 | china showcase BMP 或 self-test 注记计数 ≥ N | china-city + render-rhi §Symbol collision；gap-pin P1-3 |
| P1-4 | **P1** | 基础拓扑校验缺失 | EditSession 有；无叠盖/缝隙规则 | 叠盖/缝隙（或等价）挂会话；失败可查 | 自造叠盖 fixture → 校验失败 mark / FeatureInfo | tool + M4 编辑下限；gap-pin P1-4 |
| P1-5 | **P1** | Heatmap/extrusion 仍 CPU 观感 | layout CPU splat / prism（render-rhi §） | 可选 GPU 密度或 lit extrusion（禁 mln） | 对比 BMP：密度/立体可读性高于 CPU splat；unit 绿 | render-rhi §Map2d richness；gap-pin P1-5 |
| P2-1 | **P2** | 无 SDF 线/字形 | richness Non-goals | 自研 SDF atlas（不移植 mln） | 细线抗锯齿 + GPU text 抽样门禁 | render-rhi；gap-pin P2-1 |
| P2-2 | **P2** | 非完整 expression VM | README「仍不做」列表 | `let`/`var`/feature-state/cubic-bezier 等 | 规格用例集绿；仍不链 Native | render-rhi §Mini expression；gap-pin P2-2 |
| P2-3 | **P2** | 无地图册/多页 | 仅一页目标（P0-3） | multi-page / atlas | ≥2 页导出一致性 | plugin print；gap-pin P2-3 |
| P2-4 | **P2** | 高级拓扑规则集 | 无 | Pro/QGIS 级规则引擎后置 | 规则集文档 + 抽样用例 | M4+；gap-pin P2-4 |

### 8.1 相对 2026-09-27 矩阵的评级修正

| 能力域 | 旧评级 | 新评级 | 原因（一句话） |
| --- | --- | --- | --- |
| 制图样式 / 符号 | C→B | **B** | casing/hillshade/heatmap/extrusion/mini expr 已落地 |
| 注记 / 离线底图 | C | **C→B** | collision + `m1-labels-ok`；china 产品环 P0-2 已绿（字体/注记产品化仍 P1-3） |
| 布局打印 | D（新栈） | **B** | PrintComposer 一页（地图+比例尺+图例）+ `m1-layout-ok` |
| 2D 瓦片底图 | B–C | **B** | 缓存有；**MVT 本地 decode 已落地**（URL vector bind 仍拒） |
| 编辑 / 捕捉 / 拓扑 | C | **C→B** | vertex/edge snap 已落地；拓扑仍 P1-4 |

**最后更新：** 2026-09-30（§8 P0-1–P0-4 均关闭；china fixture + `map2d.china` loop 绿；§3.2.1 由 Map3D 代理维护）
