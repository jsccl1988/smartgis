<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# sdb Feature / MapLayer 组合 — Implementation Plan

> **For agentic workers:** Use subagent-driven or parallel agents on non-overlapping paths. Steps use checkbox syntax.

**Goal:** 产品要素与地图层改为组合持有 OGR；`src_all` 主路径去掉 `SmtAttribute` / 裸双轨 `Entry`。

**Architecture:** `sdb::Feature` owns/borrows `OGRFeature*`；`sdb::MapLayer` wraps `OGRLayer*` + meta；`SmtMap` stores `vector<MapLayer>`；codec 只填侧车。

**Tech Stack:** C++23、GDAL/OGR（`//third_party:gdal`）、GN `//src/sdb/map:gis`

## Global Constraints

- 工作在 `master`；不新开分支。
- 注释英文；用户文档中文。
- 函数 `snake_case`；公开命名空间两层 `sdb`。
- 不继承 OGR；不引入 Qt。
- 用户未要求则不 git commit。

---

### Task 1: `sdb::Feature`

**Files:** `src/sdb/feature/feature.h`, `feature.cpp`；`BUILD.gn` 可暂留 attribute 源直至 Task 5。

- [ ] 实现 `Feature`（own/borrow/release、字段转发 OGR、style/Grid/Tin/material 侧车）
- [ ] `leftover_append_feature` 改吃 `Feature*`
- [ ] 提供 `using SmtFeature = Feature;` **仅**在过渡编译需要时，Task 5 删除

### Task 2: `sdb::MapLayer` + `SmtMap`

**Files:** `src/sdb/map/map_layer.h`（新）、`map.h`、`map.cpp`

- [ ] 新增 `MapLayer`
- [ ] `SmtMap::layers_` → `vector<MapLayer>`
- [ ] CRUD/Query 对 `Feature` / `OGRFeature` 对齐

### Task 3: codec

**Files:** `src/sdb/datasource/gdal/ogr_feature_codec.{h,cc}`

- [ ] `copy_ogr_feature_to_feature`
- [ ] 去掉对 `SmtAttribute` 的写入

### Task 4: `src_all` 调用方（可并行）

**Paths:** `src/sdb/edit/`、`src/tool/`（在 `src_all` 内的）、`src/sdb/datasource/gdal/`、其它直接 `#include feature.h` 且进 `src_all` 的 TU

- [ ] 替换 `SmtFeature` / `GetAttributeRef` / `SmtField` 用法
- [ ] `AppendFeature` 等 API 对齐

### Task 5: 删除主路径旧类型

- [x] 从 `map/BUILD.gn` 去掉 `attribute.cpp` / `field.cpp`（已迁 `//src/sdb/map:leftover_attr`；`feature_3d` 早已不在 gis）
- [ ] 删除或挪出 `attribute.*` / `feature_3d.*`（UI 若仍依赖：门控或薄 stub，不进主 ABI）
- [ ] `layer.h` 去掉产品向 `SmtVectorLayer` 别名（datasource 内部可保留局部 using）

### Task 6: 文档

- [ ] `docs/README.md` 索引本 spec/plan
- [ ] `gdal-layer-management-design.md` 加一句 ABI 修正交叉引用
- [ ] `src-layout.md` feature/map 一句

---

## Parallel notes

Task 1 → Task 2 顺序；Task 3 可在 Task 1 头文件稳定后与 Task 2 并行；Task 4 在 1–3 后按目录并行；Task 5–6 收尾。
