<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `sdb/style`

MapLibre 风格 Style JSON 子集、外置符号库、属性/比例尺规则引擎。进 **`sdb` DLL**。

| 类型 | 职责 |
| --- | --- |
| `StyleDocument` | 解析 / 序列化 Style JSON |
| `SymbolLibrary` | `icon-image` → 路径 |
| `resolve` / `eval_filter` | 选层 + 产出 `ResolvedPaint` |
| `to_smt_style` | 桥到遗留 `base::SmtStyle` |

**不是** `base/style`（POD 笔刷 + Envelope），也不是 `render`（只消费 paint）。

规格：`docs/superpowers/specs/2026-09-14-sdb-style-document-design.md`。
