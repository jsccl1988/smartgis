<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# 业界差距矩阵：QGIS / Cesium / ArcGIS Pro

**Status:** active  
**Date:** 2026-09-20  
**Scope:** 以 SmartGIS 产品愿景（桌面原生 C++、「走进去干活」的图）为锚，对照三家标杆的能力下限；把差距映射到现有 `docs/superpowers` 规格/计划与建议里程碑。  
**非目标：** 不宣称要对齐三家全部产品线；不重新打开已锁定的拒绝项（Qt、Cesium Native 全家桶、第二套 GEOS、产品 Web GIS/mapd）。

相关真源：

| 主题 | 文档 |
| --- | --- |
| 分层与 OSS 对照 | [`src-layout.md`](src-layout.md)、[`../src/README.md`](../../src/README.md) |
| UI 终局 | [`ui-views-skia.md`](ui-views-skia.md) |
| 模型 / 渲染 / 计算 | [`../superpowers/specs/2026-09-13-model-render-compute-design.md`](../superpowers/specs/2026-09-13-model-render-compute-design.md) |

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
| **M0** | Views 主壳能独立完成：开图 → 漫游 → 查属性 → 简单编辑；日常停编 MFC。计划：[`../superpowers/plans/2026-09-20-m0-views-main-path.md`](../superpowers/plans/2026-09-20-m0-views-main-path.md) | 桌面 GIS 下限 |
| **M1** | Style + 瓦片 + 标注/中国底图闭环；出图可打印一页 | ≈ QGIS 基础制图 |
| **M2** | Processing 可发现工具箱（≥ 核心空间算子 + 批跑） | ≈ QGIS Processing 入口 |
| **M3** | 城市场景：地形 LOD + 3D Tiles 流式 + 稳定帧；大气旁路可演示 | ≈ Cesium / Pro 3D 观感下限 |
| **M4** | 企业数据缝：PostGIS/GPKG 多用户编辑下限 + 拓扑/捕捉；SDK/文档可嵌入 | ≈ Pro / Enterprise 入门 |

M0→M1 必须串行（壳不合拢，制图落在旧窗上）。M2 可与 M1 后半并行。M3 依赖 RHI/World 车道，可与 M1 部分并行。M4 最晚。

---

## 2. 总览矩阵

| 能力域 | QGIS | Cesium | ArcGIS Pro | SmartGIS 现状 | 目标级 | 主规格 / 计划 | 里程碑 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 桌面壳 / 控件 | A | — | A | C（Views）+ leftover | A | [ui-views-mfc-migration](../superpowers/specs/2026-09-13-ui-views-mfc-migration-design.md) **active**；[ui-views-skia](ui-views-skia.md)；[shell-compile-gate](../superpowers/specs/2026-09-19-shell-compile-gate-design.md) **active**；[M0 plan](../superpowers/plans/2026-09-20-m0-views-main-path.md) | **M0** |
| 数据模型 / OGR | A | B | A | B（GDAL/SDBD 已定） | A | [ogr-db](../superpowers/specs/2026-09-13-ogr-db-datasource-design.md)、[gdal-layer](../superpowers/specs/2026-09-13-gdal-layer-management-design.md)、[feature-maplayer](../superpowers/specs/2026-09-13-sdb-feature-maplayer-composition-design.md) **accepted** | M0–M1 |
| 2D 瓦片底图 | A | A | A | B–C（XYZ/WMTS 最小） | B→A | [tile-layer-provider](../superpowers/specs/2026-09-13-tile-layer-provider-design.md) **active**（Phase 0–1 landed） | **M1** |
| 制图样式 / 符号 | A | B | A | C（MapLibre 子集） | B→A | [sdb-style-document](../superpowers/specs/2026-09-14-sdb-style-document-design.md) **active** | **M1** |
| 注记 / 离线底图 | A | B | A | C（china_city 在推） | B | [china-city-map-plpt](../superpowers/specs/2026-09-18-china-city-map-plpt-design.md) **active** | **M1** |
| 布局打印 | A | — | A | D（新栈）/ leftover print | B | [plugin-full-upgrade](../superpowers/specs/2026-09-14-plugin-full-upgrade-design.md)；print 插件 | M1 末 |
| 编辑 / 捕捉 / 拓扑 | A | — | A | C（EditSession）+ leftover 工具 | B→A | [tool-event-dispatch](../superpowers/specs/2026-09-13-tool-event-dispatch-design.md) **accepted**；[tool-behavior-migration](../superpowers/specs/2026-09-19-tool-behavior-migration-design.md) **active**；[legacy-tool-workspace](../superpowers/specs/2026-09-19-legacy-tool-workspace-strangler-design.md) **active** | **M0** / M4 |
| Processing / 分析 | A | — | A | C–D（algorithm 核有，无工具箱） | B | [algorithm-layer-oss](../superpowers/specs/2026-09-13-algorithm-layer-oss-design.md) **accepted**；[plugin-host](../superpowers/specs/2026-09-13-plugin-host-design.md) Processing 契约 | **M2** |
| RHI / 双场景 | — | A | A | B–C（FlyCube + World/GpuScene） | A | [render-rhi-scene](../superpowers/specs/2026-09-13-render-rhi-scene-design.md)、[model-render-compute](../superpowers/specs/2026-09-13-model-render-compute-design.md) **accepted**；[scene3d-world-gpuscene](../superpowers/specs/2026-09-19-scene3d-world-gpuscene-design.md) **active** | **M3** |
| 3D 模型 / Tiles | B | A | A | C（Assimp + tileset 最小） | B→A | model-render-compute §模型；`gis::model` | **M3** |
| 地形 DEM | B | A | A | B–C（DEM 插件 + World 对齐中） | B | [leftover-scene3d-dem-unify](../superpowers/specs/2026-09-19-leftover-scene3d-dem-unify-design.md) **accepted** | M3 |
| 点云 | B | B | A | D–C（引擎有；PDAL I/O future） | C→B | `src/README` PDAL future | M3 后 |
| 大气 / 海 / 云 | — | B | B（多维） | C（v1 规格 + 升级计划） | B（差异化） | [atmosphere-ocean-cloud](../superpowers/specs/2026-09-19-atmosphere-ocean-cloud-design.md) **accepted**；[upgrade plan](../superpowers/plans/2026-09-20-atmosphere-ocean-cloud-upgrade.md) **active** | M3 旁路 |
| OGC / Web 服务栈 | A | A | A | —（产品 Web GIS 已删） | — / 客户端子集 | net + TileProvider；**不**复活 mapd | 可选客户端 |
| 远程 sdbd | — | — | B | B–C（WSL 客户端规格） | B | [sdbd-wsl-client](../superpowers/specs/2026-09-19-sdbd-wsl-client-design.md) **accepted** | M4 |
| 插件 / Python | A | B | A | C（Host + embed） | B | [plugin-host](../superpowers/specs/2026-09-13-plugin-host-design.md)、[plugin-full-upgrade](../superpowers/specs/2026-09-14-plugin-full-upgrade-design.md) **accepted** | M2–M4 |
| 嵌入 SDK | B | A | A | C（`content::`） | B | content/public；[ui-shell-multiprocess](ui-shell-multiprocess.md) | M4 |
| 跨平台 | A | A | Windows 主 | D（Windows-first） | C（后置） | — | 不进 M0–M3 |
| 企业版本库 / 身份 | B | — | A | D | C→B | 无独立规格 | **M4** |

---

## 3. 分域说明（差距 → 动作）

### 3.1 对标 QGIS（桌面干活）

| 缺口 | 现状证据 | 建议动作 | 里程碑 |
| --- | --- | --- | --- |
| 单一主 UI | Views 与 MFC 双轨；`legacy_app` opt-in | 完成 MFC→Views  parity 门禁；日常只编 `build.bat app` | M0 |
| 工具迁到 Workspace | dispatch 已有；行为仍在 leftover | 执行 tool-behavior-migration + strangler，禁新功能进 `legacy/tool` | M0 |
| Style 端到端 | Style JSON 子集 active；渲染消费未闭环 | 扩 paint + MapLayer 挂接 + GPU/2D 消费 ResolvedPaint | M1 |
| 瓦片 / MVT | Phase 0–1；MVT stub | 完成缓存与矢量瓦片最小可读 | M1 |
| Processing 入口 | algorithm 有核；无「工具箱 UI + 批跑」 | 新规格：`ProcessingRegistry` + Views 面板 + 包装 GEOS/GDAL 常用算子；挂 PluginHost ProcessingContribution | M2 |
| 打印布局 | leftover print 插件 | Views 侧最小 PrintComposer（一页 + 比例尺 + 图例） | M1 末 |

### 3.2 对标 Cesium（走进场景）

| 缺口 | 现状证据 | 建议动作 | 里程碑 |
| --- | --- | --- | --- |
| 双场景合拢 | World/GpuScene **active**；leftover scene3d 仍厚 | 按 SP4 增量切 DEM/种子；Present Facade 不回潮 | M3 |
| 流式 3D Tiles | `tileset.json` + select_tiles 最小 | LOD 调度、屏幕误差、内容缓存、失败降级；**不**引入 Cesium Native | M3 |
| 全球/城市地形 | DEM 插件 + land_mask | 瓦片化高度场 + GpuScene 同步；与 china/city 尺度验收 | M3 |
| 大气差异化 | 规格 accepted；upgrade plan active | 场驱动海/云达到「业务可看 + 中上外观」判据（见 upgrade plan） | M3 旁路 |
| 点云生产路径 | PDAL future | 单独立项：`gis/datasource` + 现有 pointcloud 引擎 | M3 后 |

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
| World↔scene3d | **active** |
| 大气 | accepted + upgrade plan **active** |
| 插件 Host | accepted |

### 4.2 建议新开规格（当前空白）

仅在现有文档盖不住时新开；默认先尝试扩写已有 active 规格。

| 建议题目 | 为何需要 | 优先挂在 |
| --- | --- | --- |
| Processing 工具箱（Views + Registry） | algorithm 与 PluginHost 契约之间缺产品面 | M2；可扩 [plugin-host](../superpowers/specs/2026-09-13-plugin-host-design.md) §Processing |
| 2D 布局打印（Views） | leftover print 不能算新栈能力 | M1 末；可挂 plugin print 升级 |
| 3D Tiles 流式调度 | model 最小路径 ≠ 规模 | M3；扩 model-render-compute 或 scene3d-world 子节 |
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
| **M0** | 仅 `SmartGisViews.exe`：打开 GPKG → 平移缩放 → FeatureInfo → 追加一条线并保存；`build.bat e2e` 绿。执行计划：[`../superpowers/plans/2026-09-20-m0-views-main-path.md`](../superpowers/plans/2026-09-20-m0-views-main-path.md) |
| **M1** | Style JSON 驱动矢量着色 + XYZ 底图 + china_city 注记可读；导出/打印一页 PDF 或位图 |
| **M2** | Views「处理」面板列出 ≥10 个算子；buffer/clip 批跑写回图层 |
| **M3** | 城市 DEM + 至少一个 3D Tiles 集流式加载不炸内存；大气海/云按 upgrade 判据开/关 |
| **M4** | 两客户端先后编辑同一 PostGIS 层有冲突提示或检出；`content::` 样例嵌入方能开图 |

---

## 7. 维护

- 规格 **Status** 变更或里程碑收口时，**同变更集** 更新本表对应行。
- 里程碑完成后：把可复述事实写入本文件或 `src/` 模块 README；相关 plan 按 [superpowers-docs](../../.cursor/rules/repo/superpowers-docs.mdc) 归档。
- 不在此文件展开实现 checklist（那是 `docs/superpowers/plans/` 的职责）。

**最后更新：** 2026-09-20
