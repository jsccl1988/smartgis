<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# M1 — Style + 瓦片 + 注记 + 一页导出 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 Views 主路径上闭环 **Style JSON 驱动矢量着色 + XYZ 底图可见 + china_city 注记可读 + 导出一页位图**；`--self-test` 含 `m1-*` marks；`build.bat e2e` 绿。

**Architecture:** 不重开制图引擎。`MapScene` 挂可选 `gis::style::StyleDocument` 与 `gis::tile::TileProvider`；`paint` 先画瓦片包络（或解码失败时的色块占位），再按 `resolve` 的 `ResolvedPaint` 画矢量，缺 style 时回退现有 Baidu 分色。导出走内存 DC → BMP 文件。打印插件 `PrintPreviewDialog` 仍为壳；本里程碑以 **Export BMP** 满足「一页位图」验收。

**Tech Stack:** C++23、`gis::style`、`gis::tile`、GDI、`SmartGisViews.exe --self-test`、GN/`build.bat`.

## Global Constraints

- Git：在 **master** 改；**不**新建分支；**不** `git commit`（除非用户明确要求）。
- 禁止 Qt；终局禁止 `#include "legacy/…"`。
- 新函数 `snake_case`；公开命名空间 ≤ 两层；源码注释英文；用户文档中文。
- 版权：`Copyright (c) 2026 The Mogu Authors.`
- 不吞并 M2 Processing / M3 流式 3D；不做完整 PrintComposer / PDF。
- MVT 可读不进本里程碑（规格 Phase 仍 stub）。

**Living gap matrix:** [`../../build/industry-gap-matrix.md`](../../build/industry-gap-matrix.md) §6 M1  
**Related:** [sdb-style-document](../specs/2026-09-14-sdb-style-document-design.md)、[tile-layer-provider](../specs/2026-09-13-tile-layer-provider-design.md)、[china-city-map-plpt](../specs/2026-09-18-china-city-map-plpt-design.md)、[M0 plan](2026-09-20-m0-views-main-path.md)

---

## File map

| File | Role |
| --- | --- |
| `src/app/views/map_scene.h` `.cc` | style / basemap 挂接；paint 消费；`export_bmp` |
| `src/app/views/BUILD.gn` | `map_scene` deps → `//src/gis:gis` style+tile |
| `testing/data/china_city.style.json` | 最小 Style JSON（area/line/point/text） |
| `src/app/views/map_scene_test.cc` | style 着色 + basemap fetch + export 往返 |
| `src/app/views/browser_view.*` | 菜单 Export；Open 后可选 load style；basemap 挂 provider |
| `src/app/views/main.cc` | `--self-test` marks `m1-style-ok` / `m1-basemap-ok` / `m1-labels-ok` / `m1-export-ok` |
| `docs/build/ui-testing.md` | 退出码 70–73 |
| `docs/build/industry-gap-matrix.md` | M1 行回写 |

---

### Task 1: StyleDocument → MapScene paint

**Produces:** `set_style_document` / `clear_style_document`；有 style 时 fill/line/circle 用 `ResolvedPaint` 颜色。

- [x] **Step 1:** `map_scene_test`：parse 最小 JSON → `set_style_document` → 断言 paint 路径使用非默认色（可通过导出像素抽样，或暴露 `resolve_layer_paint_for_test`）。
- [x] **Step 2:** 实现挂接 + `paint` 内对 `area`/`line`/`point` 调 `gis::style::resolve`。
- [x] **Step 3:** 落地 `testing/data/china_city.style.json`；Views 启动/`seed_default` 旁若存在则 `load_style_path`。
- [x] **Step 4:** `build.bat map_scene_test` PASS。不 commit。

### Task 2: XYZ basemap underlay

**Produces:** `set_basemap_provider(std::shared_ptr<TileProvider>)`；`paint` 在矢量前 `fetch_visible` 并绘制 tile 屏幕矩形（优先 WIC 解码 PNG；失败则半透明色块占位）。

- [x] **Step 1:** 测试注入 `set_fetch_fn` 返回 PNG stub；`paint` 后 `basemap_tiles_drawn_for_test() > 0`。
- [x] **Step 2:** `catalog.layer.add_basemap` 在创建 Catalog 层同时 `document_.set_basemap_provider(...)`。
- [x] **Step 3:** 测绿。不 commit。

### Task 3: Export one page BMP

**Produces:** `bool export_bmp(const std::string& path, int w, int h) const`；菜单 **Export**。

- [x] **Step 1:** 测：`export_bmp(tmp)` 文件存在且头为 `BM`。
- [x] **Step 2:** 菜单 + `on_export_document`。
- [x] **Step 3:** 测绿。不 commit。

### Task 4: Self-test + docs + e2e

- [x] **Step 1:** `main.cc` 在 china 打开后断言 text 层/注记；加载 style；mock basemap；export → marks `m1-labels-ok` / `m1-style-ok` / `m1-basemap-ok` / `m1-export-ok`；失败码 70–73。
- [x] **Step 2:** 更新 `ui-testing.md`、差距矩阵 M1 行。
- [x] **Step 3:** `build.bat views` + `--self-test` + `build.bat e2e` 绿。不 commit。

---

## Done when

1. Style JSON 改变矢量 fill/line 色（相对默认 Baidu 色可测）。
2. XYZ provider（可 mock）在 paint 下有可见 underlay 计数。
3. china_city 注记：`--self-test` 断言 text/anno。
4. Export BMP 一页成功。
5. e2e 绿；差距矩阵回写。
