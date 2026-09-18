<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# 制图样式：Style JSON + 符号库 + 规则引擎

**Date:** 2026-09-14  
**Status:** active（v1+ 子集：paint 键扩展 + LayerType 占位 + expression 常量求值）  
**Scope:** MapLibre 风格 Style JSON 子集、外置符号库、按属性/比例尺选层规则；与遗留 `base::SmtStyle` 的桥接。不管 Skia/RHI 画笔实现；不管 Views chrome。

**Sibling:**

- 分层锁定 — [`docs/build/src-layout.md`](../../build/src-layout.md)
- `Feature` / `MapLayer` — [`2026-09-13-sdb-feature-maplayer-composition-design.md`](2026-09-13-sdb-feature-maplayer-composition-design.md)
- 瓦片 — [`2026-09-13-tile-layer-provider-design.md`](2026-09-13-tile-layer-provider-design.md)
- 渲染 — [`2026-09-13-render-rhi-scene-design.md`](2026-09-13-render-rhi-scene-design.md)（只消费解析后的 paint，不拥有 Style 文档）

## Goal

为矢量图层提供可序列化的**表现模型**（不是渲染后端，也不是 GIS 属性表）：

1. **Style JSON 契约** — MapLibre Style Spec 子集：文档 + `layers[]` + `paint`/`layout` + `filter` + zoom 范围。
2. **符号库外置** — 按 `icon-image` / 符号 id 解析到文件路径或字节；与 `SmtSymbolDesc.lSymbolID` 解耦。
3. **图层样式规则** — 按属性字典 + 当前比例尺（zoom）匹配 layer / filter，产出 `ResolvedPaint`，并可桥到遗留 `base::SmtStyle`。

## Non-goals

- **不**把 Style 放进 `src/render/`（render 只消费已解析 paint）。
- **不**把 JSON / 规则塞进 `src/sdb/carto`（cartographic POD：`SmtStyle` / `Envelope` / `StyleManager`）。
- **不**新建顶层 `src/style/`（破坏五层锁定）。
- **不**追求完整 MapLibre 表达式 / 数据驱动样式 / glyphs PBF / sprite 合图引擎（v1+ 仅嵌套数组 **常量求值** 子集，见下）。
- **不**在本周期改 OGR 二进制 `SmtStyle` blob 编解码语义。
- **不**引入 Qt；不引入第三套 JSON 库（手写最小解析 `json_mini`，对齐 `sdb/model/tileset`）。

## 落位（推荐锁定）

| 能力 | 树 | 命名空间 | DLL |
| --- | --- | --- | --- |
| 遗留笔刷/符号 POD + Envelope | `src/sdb/carto/` | `base` | `base` |
| StyleDocument / SymbolLibrary / RuleEngine / 桥接 | `src/sdb/style/` | `sdb::style` | `sdb` |

```
MapLayer / Feature attrs + zoom
        |
        v
 sdb::style::StyleDocument     parse Style JSON
 sdb::style::SymbolLibrary     id → asset
 sdb::style::evaluate_*        filter + zoom match
        |
        v
 ResolvedPaint  ----bridge---->  base::SmtStyle (可选，喂遗留 Feature/render)
        |
        v
 render / legacy_render        只读 paint / SmtStyle
```

### 方案对比

| 方案 | 做法 | 结论 |
| --- | --- | --- |
| A. 扩 `sdb/carto` | JSON+规则进 cartographic POD 树 | 否：拉高 carto 职责，违反「carto=POD」 |
| B. 顶层 `src/style/` | 新层 | 否：破坏五层 |
| **C. `sdb/style`（推荐）** | 表现模型跟 GIS 图层同层；base 保留 POD | **采用** |

## JSON 子集（v1+）

根对象：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `version` | number | 必填；接受 `8`（MapLibre 兼容号） |
| `name` | string | 可选 |
| `sprite` | string | 可选；符号库根路径或 URL 占位 |
| `glyphs` | string | 可选；v1 仅存字符串，不加载 PBF |
| `layers` | array | 必填（可空） |

`layers[]` 元素：

| 字段 | 说明 |
| --- | --- |
| `id` | 层 id |
| `type` | 见 LayerType |
| `source` / `source-layer` | 可选字符串 |
| `minzoom` / `maxzoom` | 可选 number |
| `filter` | 见下 |
| `paint` / `layout` | 扁平表；值可为标量，或 **JSON 数组 expression**（求值后变常量字符串再写入 `ResolvedPaint`） |

### LayerType

已实现解析（未知 → `kUnknown`）：

| 字符串 | 枚举 | 说明 |
| --- | --- | --- |
| `fill` / `line` / `symbol` / `circle` | 对应 | 主路径 |
| `background` / `raster` | 对应 | paint 键见下 |
| `fill-extrusion` / `heatmap` / `hillshade` | `kFillExtrusion` / `kHeatmap` / `kHillshade` | **占位**：可 parse，无渲染实现 |

Filter v1：`==` `!=` `<` `>` `<=` `>=` `has` `!has` `in` `!in` `all` `any` `none`；属性侧用 `std::map<std::string,std::string>`（数值比较时 `strtod`）。

### Expression 子集（v1+）

API：`eval_expression(json, attrs, zoom, &ExprValue)`；`resolve` / `fill_resolved_paint` 对 paint/layout 中「看起来像 expression 的数组字符串」自动求值。

| 算子 | 形式 | 说明 |
| --- | --- | --- |
| `get` | `["get","key"]` | 读 `AttrMap`；缺失 → null |
| `literal` | `["literal", <scalar\|json>]` | 常量；非标量序列化为 JSON 字符串 |
| `zoom` | `["zoom"]` | 当前 zoom（number） |
| 二元比较 | `["=="\|"!="\|"<"\|... , a, b]` | 操作数可嵌套；比较规则与 filter 一致（优先数值） |

**不在本子集**：`case` / `match` / `interpolate` / `step` / 数据驱动渐变等。

Paint / layout 键（识别并写入 `ResolvedPaint`）：

- fill: `fill-color`, `fill-opacity`, `fill-pattern`（图案 id 字符串）
- line: `line-color`, `line-width`, `line-opacity`, `line-dasharray`（number 数组）, `line-cap`, `line-join`（layout 或 paint）
- symbol: `icon-image`, `text-field`, `icon-size`, `text-size`, `text-anchor`, `icon-offset`（`[x,y]`）
- circle: `circle-color`, `circle-radius`, `circle-opacity`
- background: `background-color`, `background-opacity`
- raster: `raster-opacity`

颜色：`#RRGGBB` / `#AARRGGBB` / `rgb(r,g,b)`；失败则默认。

## 符号库（v1）

- `SymbolLibrary::load_manifest(json)`：`{"symbols":[{"id":"marker","path":"marker.png"}]}`
- `SymbolLibrary::set_root(dir)` + 相对 path 拼接
- `find(id)` → `SymbolEntry{id, path, optional bytes}`
- Sprite 合图 / 远程下载：非目标；`sprite` 字段只作根提示

## 规则引擎（v1+）

- `layer_matches_zoom(layer, zoom)`
- `eval_filter(filter, attrs)`
- `eval_expression(json, attrs, zoom)` → `ExprValue` 常量
- `select_layers(doc, zoom, source_layer_opt)` → 匹配层列表（文档顺序）。若 `source_layer_opt` 非空，仅精确匹配该 `source-layer`（层字段为空则不匹配）。
- `resolve(doc, library, attrs, zoom, source_layer)` → 第一个匹配层的 `ResolvedPaint`（含 expression 求值与 `icon` 解析）
- `fill_resolved_paint(layer, library, attrs, zoom, out)` → 填充 paint 字段
- `to_smt_style(ResolvedPaint, name)` → `base::SmtStyle`（fill→brush，line→pen，symbol→SmtSymbolDesc 仅宽高；`lSymbolID` 仍为 0，id 走库）

## MapLayer 挂接（同变更可做薄挂）

- `MapLayer` 增加可选 `style_document` 共享指针或仅 `style_uri`；**不删除** `style_name_`（仍指向 `SmtStyleManager`）。
- v1 优先：模块可独立单测；`MapLayer::set_style_document` 可选落地。

## 测试

- `//src/sdb/style:style_test`：parse 样例 JSON、新 paint 键、占位 LayerType、filter、zoom、expression（`get`/`literal`/比较）、symbol manifest、`to_smt_style`。

## 文档同步

- `docs/build/src-layout.md`：`sdb` 行加入 `style`
- `src/README.md` / `src/base/README.md`：澄清 dual-track
- `docs/README.md`：索引本 spec
