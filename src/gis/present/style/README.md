<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `gis/present/style`

`src/gis/present` 产出 `StyleDocument`、符号和瓦片（`present/tile`）。`gis::vista::Layout` 消费它们。`present` 不是 Frame Graph，也不是领域。`gis::World` 与 `gis::DomainSession` 不解析 Style JSON。

MapLibre 风格 Style JSON **v1+ 子集**、外置符号库、属性/比例尺规则引擎、嵌套数组 expression 常量求值。进 **`gis` DLL**。

| 类型 | 职责 |
| --- | --- |
| `StyleDocument` | 解析 / 序列化 Style JSON（层类型含 `background` / `raster`，以及占位 `fill-extrusion` / `heatmap` / `hillshade`） |
| `SymbolLibrary` | `icon-image` → 路径 |
| `eval_expression` | paint 侧嵌套数组 expression → 常量（`get` / `literal` / `zoom` / 二元比较） |
| `resolve` / `eval_filter` | 选层 + 产出 `ResolvedPaint` |
| `to_smt_style` | 桥到遗留 `base::SmtStyle` |

**不是** `legacy/carto`（POD 笔刷；`gis::Envelope` 在 `gis/model`），也不是 `render`（只消费 paint）。

规格：`docs/superpowers/specs/2026-09-14-sdb-style-document-design.md`。

## v1+ paint / layout 识别键

- fill: `fill-color`, `fill-opacity`, `fill-pattern`
- line: `line-color`, `line-width`, `line-opacity`, `line-dasharray`, `line-cap`, `line-join`
- symbol: `icon-image`, `text-field`, `icon-size`, `text-size`, `text-anchor`, `icon-offset`
- circle: `circle-color`, `circle-radius`, `circle-opacity`
- background: `background-color`, `background-opacity`
- raster: `raster-opacity`

## Expression 子集（v1+）

paint/layout 值为 JSON 数组且首元为算子时，在 `fill_resolved_paint` / `resolve` 中求值为常量（`AttrMap` + zoom）。JSON 解析/写出走 RapidJSON（`//third_party:rapidjson`）。
