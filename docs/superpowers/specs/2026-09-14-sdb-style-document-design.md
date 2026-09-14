<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# 制图样式：Style JSON + 符号库 + 规则引擎

**Date:** 2026-09-14  
**Status:** active  
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
- **不**把 JSON / 规则塞进 `src/base/style`（Foundation 继续只持 POD：`SmtStyle` / `Envelope` / `StyleManager`）。
- **不**新建顶层 `src/style/`（破坏五层锁定）。
- **不**追求完整 MapLibre 表达式 / 数据驱动样式 / glyphs PBF / sprite 合图引擎（可后续增量）。
- **不**在本周期改 OGR 二进制 `SmtStyle` blob 编解码语义。
- **不**引入 Qt；不引入第三套 JSON 库（手写最小解析，对齐 `sdb/model/tileset`）。

## 落位（推荐锁定）

| 能力 | 树 | 命名空间 | DLL |
| --- | --- | --- | --- |
| 遗留笔刷/符号 POD + Envelope | `src/base/style/` | `base` | `base` |
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
| A. 扩 `base/style` | JSON+规则进 Foundation | 否：拉高 base 职责，违反「style=POD」 |
| B. 顶层 `src/style/` | 新层 | 否：破坏五层 |
| **C. `sdb/style`（推荐）** | 表现模型跟 GIS 图层同层；base 保留 POD | **采用** |

## JSON 子集（v1）

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
| `type` | `fill` \| `line` \| `symbol` \| `circle` |
| `source` / `source-layer` | 可选字符串 |
| `minzoom` / `maxzoom` | 可选 number |
| `filter` | 见下 |
| `paint` / `layout` | string→string\|number\|bool 扁平表（v1 不解析嵌套 expression） |

Filter v1：`==` `!=` `<` `>` `<=` `>=` `has` `!has` `in` `!in` `all` `any` `none`；属性侧用 `std::map<std::string,std::string>`（数值比较时 `strtod`）。

Paint 键（v1 识别并写入 `ResolvedPaint`）：

- fill: `fill-color`, `fill-opacity`
- line: `line-color`, `line-width`, `line-opacity`
- symbol: `icon-image`, `text-field`, `icon-size`
- circle: `circle-color`, `circle-radius`, `circle-opacity`

颜色：`#RRGGBB` / `#AARRGGBB` / `rgb(r,g,b)`；失败则默认。

## 符号库（v1）

- `SymbolLibrary::load_manifest(json)`：`{"symbols":[{"id":"marker","path":"marker.png"}]}`
- `SymbolLibrary::set_root(dir)` + 相对 path 拼接
- `find(id)` → `SymbolEntry{id, path, optional bytes}`
- Sprite 合图 / 远程下载：非目标；`sprite` 字段只作根提示

## 规则引擎（v1）

- `layer_matches_zoom(layer, zoom)`
- `eval_filter(filter, attrs)`
- `select_layers(doc, zoom, source_layer_opt)` → 匹配层列表（文档顺序）。若 `source_layer_opt` 非空，仅精确匹配该 `source-layer`（层字段为空则不匹配）。
- `resolve(doc, library, attrs, zoom)` → 第一个匹配层的 `ResolvedPaint`（含 `icon` 解析结果）
- `to_smt_style(ResolvedPaint, name)` → `base::SmtStyle`（fill→brush，line→pen，symbol→SmtSymbolDesc 仅宽高；`lSymbolID` 仍为 0，id 走库）

## MapLayer 挂接（同变更可做薄挂）

- `MapLayer` 增加可选 `style_document` 共享指针或仅 `style_uri`；**不删除** `style_name_`（仍指向 `SmtStyleManager`）。
- v1 优先：模块可独立单测；`MapLayer::set_style_document` 可选落地。

## 测试

- `//src/sdb/style:style_test`：parse 样例 JSON、filter、zoom、symbol manifest、`to_smt_style`。

## 文档同步

- `docs/build/src-layout.md`：`sdb` 行加入 `style`
- `src/README.md` / `src/base/README.md`：澄清 dual-track
- `docs/README.md`：索引本 spec
