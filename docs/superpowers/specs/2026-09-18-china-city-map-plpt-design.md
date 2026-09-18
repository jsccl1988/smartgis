<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# 中国地级底图：区 / 线 / 点 / 注记（Views MapScene）

**Date:** 2026-09-18  
**Status:** active  
**Scope:** `SmartGisViews`（及共用 `MapScene` 的壳）默认种子地图内容：对标正常地图的 **区（面）、线、点、text（注记）**；数据为 **地级（约地市）离线包、更高精度几何**。  
**非范围：** 县级/乡镇；完整全国路网；影像 XYZ；默认打开 MapLibre；Qt。

**Sibling:**

- Views 壳 / Catalog / overlay — `src/app/views/README.md`、`docs/build/ui-views-skia.md`
- OGR / Anno 字段语义 — [`2026-09-13-ogr-db-datasource-design.md`](2026-09-13-ogr-db-datasource-design.md)（`SmtFtAnno`：`anno` / `color` / `angle`）
- 在线瓦片底图 — [`2026-09-13-tile-layer-provider-design.md`](2026-09-13-tile-layer-provider-design.md)（**互补**，不替代本离线矢量包）

## 背景与问题

当前默认种子 `testing/data/china_plp.geojson` 仅含极简轮廓 + 少量 PLP，Catalog 虽显示「china…」，视口观感不像中国地图。  
`MapScene::GeomKind` 只有 `kPoint` / `kLine` / `kPolygon`，**无注记**。  
用户确认：**方案 A（离线四图层）+ 地级更高精度**。

## Goal

1. 启动后 Catalog 默认至少四层：`area`（地市面）、`line`、`point`、`text`。
2. 视口内可见：**填色地市面 + 主要线划 + 地市点 + 中文注记**；右键 **View** / `view.full` 缩放到全国范围。
3. OGR 打开同一 GPKG（或等价多图层源）可复现；`--self-test` 断言层数、要素量级、中国经纬度包络、至少一条注记。
4. 几何精度：**地级行政区轮廓可辨认**（非示意多边形）；允许轻度简化，但顶点密度远高于现行 `china_plp`。

## Non-goals

- 不追求测绘级境界线或官方审图号发布；本包为 **工程演示 / 开发默认数据**。
- 不做县级、不做全量 OSM 路网、不做在线实时境界更新。
- 不把完整商业底图引擎（MapLibre 全栈）设为 Views 默认路径。
- 不把大体积原始数据无压缩地塞进 Git 历史而不走获取约定（见「交付」）。

## 方案选择（已决）

| 选项 | 结论 |
| --- | --- |
| A 离线四图层 GPKG | **采用** |
| B 单 GeoJSON + kind | 否（Catalog 单层，不像正常地图） |
| C MapLibre 默认 | 否（本轮） |
| 精度 | **更高精度地级面**（可轻度简化；禁止示意级折线） |

## 数据模型

### 容器

- 主文件：`testing/data/china_city.gpkg`（或构建后落到 `out/china_city.gpkg`）。
- CRS：`EPSG:4326` / OGC CRS84（与现有 OGR 摄入 Y-flip 约定一致）。
- 图层名（稳定，供 `seed_default` / Catalog）：

| 图层 | OGR 几何 | 角色 | 必填字段 |
| --- | --- | --- | --- |
| `area` | Polygon / MultiPolygon | 地市级行政区（区） | `name`；建议 `adcode` |
| `line` | LineString / MultiLineString | 主要河流 / 干线交通等 | `name`（可空）；`kind`（如 `river` / `road`） |
| `point` | Point | 地市驻地或代表点 | `name`；建议 `kind=city` |
| `text` | Point | 注记锚点 | `anno`（优先）或 `name`；可选 `angle`、`color` |

`text` 字段对齐 leftover / OGR Anno 约定：`anno` + 可选 `angle` / `color`（见 ogr-db-datasource 设计）。无 `anno` 时回退 `name`。

### 精度与体量（更高精度档）

- **面：** 地级单元应覆盖中国陆地主要地市（量级约数百个面要素）；边界以可辨认行政区形状为准。允许 `ogr2ogr -simplify` 轻度抽稀，但目标观感不是「十几段折线画全国」。
- **线：** 主要大江大河 + 若干干线即可（不必全国道路网）。
- **点 / 注记：** 与地市一一对应为主；注记与点可同坐标或注记略偏移。
- **体量指引：** 压缩 GPKG 目标 **约 15–60 MB**。若超过 Git 舒适区（建议单文件 &gt; 25 MB 不直接进主历史），改用 **fetch 落盘**（`third_party/tools/fetch.py` 或 `testing/data` 旁脚本）+ `PIN`/校验和；`BUILD.gn` `copy` 或 data dep 指向 `out/` / `.src` 落点。
- **许可：** 数据旁必须有 `testing/data/china_city.LICENSE.txt`（或 PIN 旁 README）写明来源、许可、禁止用途；产品 About 可链到该文件。禁止未标明来源的二进制进仓。

### 与旧样例关系

- `china_plp.geojson`：保留为 **缺 GPKG 时的自测 / 兜底**（`--self-test` 可仍优先试 GPKG，失败再写/开小 PLP）。
- 默认 `MapScene::seed_default` / `try_bootstrap_*`：**优先** `china_city.gpkg`，其次旧 geojson。

## 运行时（MapScene / Views）

### GeomKind

```text
kPoint | kLine | kPolygon | kText
```

- OGR：`wkbPoint` + 层名 `text` 或字段表明注记 → `kText`；普通点层 → `kPoint`。
- `paint`：`kText` 用 `TextOutW`（UTF-16）在锚点绘制 `anno`/`name`；面半透明填充+描边；线分色；点符号。

### 多层摄入

- `ingest_ogr_path`：GPKG 多图层时 **每个 OGR 层一个 `MapScene::Layer`**（已具备雏形则加固），Catalog 显示真实层名 `area`/`line`/`point`/`text`。
- `fit_extent` / Catalog **View**：对全部可见层包络适配（已有 `fit_map_extent` 路径须对四层有效）。

### 样式（v1 最小）

- 固定分色即可（区/线/点/注记），不强制接 `sdb::style` 全文。
- 后续可把层风格迁到 StyleDocument；**本规格不阻塞**。

## 构建与测试

- GN：`testing/data` 增加 `china_city` 产物（copy 或 fetch→out）。
- `//src/app/views` data_deps 依赖该产物。
- `--self-test` 新/加强断言（示例退出语义，实现时可并入现有码表）：
  - 打开 `china_city.gpkg` 成功且 `last_open_was_ogr`
  - `layer_count() >= 4`
  - `feature_count()` 显著大于示意 PLP（建议阈值：面 ≥ 100 或总要素 ≥ 200，按实装数据微调）
  - 至少 1 个 `kText`（或 text 层非空）
  - `has_china_extent()` 仍真
- `build.bat views` 绿；无数据文件时明确失败或明确降级到 PLP 并在状态栏提示（自测不得静默冒充「高精度中国地图」）。

## 实现阶段

| 阶段 | 内容 |
| --- | --- |
| 1 | `MapScene`：`kText` + paint + OGR 注记字段；多层 Catalog 名 |
| 2 | 数据：生成/获取高精度 `china_city.gpkg` + LICENSE + GN/fetch |
| 3 | `seed_default` 优先 GPKG；自测与 README 更新 |
| 4 | （可选）按比例尺显隐注记、线 kind 分色细化 |

## 验收

- 打开 `SmartGisViews.exe`：Catalog 四层；地图上可辨认中国地市面斑 + 线 + 点 + 中文注记。
- 右键图层 **View** 缩放到全国。
- 与旧「棕色网格 + 示意折线」观感明显不同。
- 文档与 LICENSE 可追溯数据来源。

## Open questions

1. **数据源最终选型**（实现时锁定一种并写入 LICENSE）：公开可再分发的地级界（需人工核对许可）vs 本机已有数据导入脚本。  
2. **超大文件策略**：Git 直存 vs fetch+校验和（默认倾向：&gt;25MB 走 fetch）。  
3. **CEF / WinUI / C# 壳**：是否共用同一 `MapScene` 种子（倾向是；本规格以 Views 为验收主路径）。

## Success

- 用户主观：这是「中国地级矢量地图」，不是 demo 折线。
- 技术：区/线/点/text 四类均可绘制与 Catalog 管理；高精度离线包可构建复现。

---

**最后更新：** 2026-09-18
