<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# 中国地级底图（区/线/点/注记）Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Views 默认加载高精度离线 `china_city.gpkg`（`area`/`line`/`point`/`text`），MapScene 能画注记，观感对标正常地级矢量地图。

**Architecture:** OGR 摄入多图层 GPKG → `MapScene` 四层；`kText` 用 GDI `TextOutW`；`seed_default` 优先 GPKG。数据可 fetch 落盘，&gt;25MB 不强制进 Git。

**Tech Stack:** GDAL/OGR、GeoPackage、Views `MapScene` overlay、GN copy/fetch。

**Spec:** [`docs/superpowers/specs/2026-09-18-china-city-map-plpt-design.md`](../specs/2026-09-18-china-city-map-plpt-design.md)

## Global Constraints

- 工作在 `master`，不新开分支；用户未要求则不 commit。
- 源码注释英文；用户文档中文。
- 函数 `snake_case`；两层命名空间。
- 构建：`build.bat views`；产出 `out/`。
- 无 Qt；不默认开 MapLibre。

---

### Task 1: MapScene `kText` + OGR 注记 + 分色绘制

**Files:** `src/app/views/map_scene.h`, `src/app/views/map_scene.cc`

- [x] `GeomKind::kText`；`feature_from_ogr`：Point + 层名 `text` 或字段 `anno` → `kText`
- [x] `paint`：注记 `TextOutW`；面/线/点分色；GDI 句柄复用防泄漏
- [x] 多层层名保留 OGR 名（`area`/`line`/`point`/`text`）

### Task 2: 高精度 `china_city.gpkg` + LICENSE + GN

**Files:** `testing/data/`（脚本或产物）、`testing/data/BUILD.gn`、`testing/data/china_city.LICENSE.txt`

- [x] 生成/获取地级面（高精度）+ 线 + 点 + 注记四层 GPKG
- [x] LICENSE 写明来源；&gt;25MB 则 fetch→`out/` + PIN
- [x] GN data_deps 复制到 `out/`

### Task 3: seed_default + 自测 + README

**Files:** `src/app/views/map_scene.cc`, `src/app/views/main.cc`, `src/app/views/README.md`, `src/app/views/BUILD.gn`

- [x] `try_bootstrap` 优先 `china_city.gpkg`
- [x] `--self-test`：≥4 层、要素量级、有 text、`has_china_extent`
- [x] README 更新默认数据说明

### Task 4: 构建验证

- [x] `build.bat views` exit 0；`SmartGisViews.exe --self-test` exit 0
