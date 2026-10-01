<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Map2D industry gap pin — Implementation Plan

> **For agentic workers:** Tick boxes as gaps close. Evidence + acceptance live in
> [`../../build/industry-gap-matrix.md`](../../build/industry-gap-matrix.md) **§8 Map2D 钉死清单**.
> Do **not** open a new dated design; revise living umbrellas linked below.

**Goal:** Close the executable Map **2D** gaps vs QGIS / MapLibre-class carto
(not Cesium 3D). Each checkbox maps to one §8 row.

**Non-goals:** Qt · Cesium Native · product Web GIS/mapd · second GEOS · 3D tiles /
atmosphere (those stay on other plans).

**Living specs:**

| Domain | Spec |
| --- | --- |
| map2d / Style richness / labels | [`../specs/2026-09-13-render-rhi-scene-design.md`](../specs/2026-09-13-render-rhi-scene-design.md) §Map2d richness |
| tile / Style document / china | [`../specs/2026-09-13-gdal-layer-management-design.md`](../specs/2026-09-13-gdal-layer-management-design.md) |
| shell / panels / print UI | [`../specs/2026-09-27-views-desktop-shell-design.md`](../specs/2026-09-27-views-desktop-shell-design.md) |
| edit tools | [`../specs/2026-09-13-tool-event-dispatch-design.md`](../specs/2026-09-13-tool-event-dispatch-design.md) + tool-behavior-migration |

**Related landed / open plans:** [`2026-09-20-m1-carto-style-tile-export.md`](2026-09-20-m1-carto-style-tile-export.md) (M1 marks green)、[`2026-09-30-map2d-hillshade-line-casing.md`](2026-09-30-map2d-hillshade-line-casing.md) (P0 richness mostly landed; china loop still open).

---

## P0 — unblock “能干活的 2D 主路径”

- [x] **P0-1 MVT 最小可读**：`mvt_stub` → protobuf+gzip decode → features into MapFrame; reject stays only for unsupported encodings. Gate: unit + `m1-basemap`-class mark with local `.mvt` fixture.
- [x] **P0-2 China 产品环绿**：`map2d_china_loop` / `maplibre_align` full still with casing + soft hillshade; fixture path deterministic under `out/data`.
- [x] **P0-3 一页布局出图**：Views PrintComposer 最小（地图 + 比例尺 + 图例）→ BMP/PNG；`PrintPreviewDialog` 不再仅靠 viewport `export_bmp`。Gate: self-test mark `m1-layout-ok`（或继任口令）+ 人工一页可读。
- [x] **P0-4 顶点/边捕捉**：EditSession / feature_edit 路径上 vertex+edge snap（容差可配）；`--self-test` 或 tool harness 断言 snap 命中。

## P1 — 制图深度与编辑下限

- [ ] **P1-1 栅格瓦片生产缓存**：`TileDiskCache` + XYZ/WMTS 在 Views 主路径默认开启；离线二次打开无重复全量拉取（或可证明 hit rate）。
- [ ] **P1-2 Style 表达式算术族**：mini eval 增加 `+`/`*`/`-`/`/`（及必要 `to-number`）；`style_test` 覆盖 data-driven `line-width`/`fill-color`。
- [ ] **P1-3 注记产品化**：china_city 沿线槽位在 showcase/self-test 可读；CJK 字体路径稳定；不做 SDF atlas（仍 P2）。
- [ ] **P1-4 基础拓扑校验**：叠盖 / 缝隙（或等价）挂 EditSession；失败可查 FeatureInfo / mark。
- [ ] **P1-5 GPU heatmap / extrusion 观感**：CPU splat/prism 之上可选 GPU 密度或 lit extrusion（仍禁 mln pin）。

## P2 — 对标 MapLibre / Pro 的后置项

- [ ] **P2-1 SDF 线/字形 atlas**：距离场线与 GPU text（禁 mln 源码移植）。
- [ ] **P2-2 完整 expression VM**：`let`/`var`、feature-state、cubic-bezier、格式化族。
- [ ] **P2-3 地图册 / 多页布局**：一页之后的 atlas / multi-page。
- [ ] **P2-4 高级拓扑规则集**：QGIS/Pro 级规则引擎（非本阶段必需）。

---

## Done when

§8 全部 **P0** 勾选且对应口令绿；P1 至少完成瓦片缓存 + 捕捉后的拓扑或表达式其一；矩阵 §2 Map2D 相关行已回写。

**Audit date:** 2026-09-30  
**Auditor notes:** Style casing/hillshade/heatmap/fill-extrusion/mini-expr/collision v1 already landed under render-rhi §Map2d richness — those are **not** re-listed as open P0.
