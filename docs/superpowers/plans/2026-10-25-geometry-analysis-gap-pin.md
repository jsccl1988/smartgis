<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Geometry analysis industry gap pin — Implementation Plan

> **For agentic workers:** Tick boxes as gaps close. Ownership + family matrix live in
> [`../specs/2026-09-13-algorithm-layer-oss-design.md`](../specs/2026-09-13-algorithm-layer-oss-design.md)
> **§9 Geometry industry gap**. Overview row in
> [`../industry-gap-matrix.md`](../industry-gap-matrix.md) **§9**.
> Do **not** open a new dated design; revise those living docs.
>
> **Diagram:** [`../diagrams/geometry-analysis-gap.html`](../diagrams/geometry-analysis-gap.html)

**Goal:** Close executable **vector geometry analysis** gaps vs PostGIS `ST_*` /
QGIS Processing vector tools / ArcGIS Pro Geoprocessing geometry + overlay +
topology — without a second GEOS/CGAL and without moving kernels into plugins.

**Prioritization (locked):**

| Phase | Rule |
| --- | --- |
| **P0** | GEOS / OGR already can do it → implement in `geo::{set,shape,predicate,measure,…}` (§3.5) → thin IR + `native.*` catalog + WKT/GeoJSON surface |
| **P1** | Product main path: layer dissolve / spatial join / overlay; `analysis/geometry` Eigen fit/transform deepen; Processing discoverability |
| **P2** | Topology rules / validation on EditSession (cross-link Map2D P1-4 / P2-4); not a parallel rule engine in `analysis/` |

**Non-goals:** Second GEOS · CGAL · Triangle · TetGen · parallel Shapely · Qt Processing ABI ·
ArcGIS rule-engine clone · 3D tetrahedralization · putting kernels under `src/plugin/`.

**Living specs:**

| Domain | Spec |
| --- | --- |
| Kernel ownership / GEOS / Eigen | [`../specs/2026-09-13-algorithm-layer-oss-design.md`](../specs/2026-09-13-algorithm-layer-oss-design.md) §3 / §9 |
| Layer / Feature / OGR | [`../specs/2026-09-13-gdal-layer-management-design.md`](../specs/2026-09-13-gdal-layer-management-design.md) |
| Processing product surface | [`../specs/2026-09-13-plugin-host-design.md`](../specs/2026-09-13-plugin-host-design.md) §Processing |
| Snap / topology product behavior | [`../specs/2026-09-13-tool-event-dispatch-design.md`](../specs/2026-09-13-tool-event-dispatch-design.md) §Topology validation · Map2D gap-pin P1-4 / P2-4 |

**Code anchors today:**

| Area | Path | Notes |
| --- | --- | --- |
| GEOS 功能体 | `src/gis/geo/ops/{set,shape,predicate,measure,transform,buffer}` | §3.5；`ops_geos` 已删 |
| 薄 IR / catalog | `analysis/ops/ir` + `ops/runner` | `eval_geometry_ir` → `geo::*`；`native.*` |
| WKT surface | `ops/wkt/wkt_ops.*` | 脚本边界；不内嵌 GEOS |
| Eigen fit | `src/gis/analysis/geometry/fit.*` | fit_line / fit_plane / affine_align only |
| Delaunay | `src/gis/geo/tin/delaunay.*` | not yet a Processing `native.*` for Voronoi |
| Snap | `feature_edit` snap | Map2D P0-4 **done**; topology validation still open |

---

## P0 — GEOS / OGR zero-cost expose

Each item: add/extend `geo::*`（GEOS/OGR）→ thin IR id → `ops_catalog` + runner → unit（`ops_test` / geo ops tests / WKT）→ optional `wkt_ops` when the script surface already mirrors that family.

### P0-A Validity / repair

- [x] **P0-A1** `native.is_valid` / `native.is_simple` (bool predicates) — 2026-10-25
- [x] **P0-A2** `native.make_valid` (geometry out; GEOS MakeValid) — 2026-10-25
- [x] **P0-A3** `native.relate` (DE-9IM string via `geo::relate_de9im`) — 2026-10-25; `relate_pattern` Deferred

### P0-B Constructive shapes (GEOS)

- [x] **P0-B1** `native.point_on_surface` — 2026-10-25
- [x] **P0-B2** `native.densify` / segmentize by distance — 2026-10-25
- [x] **P0-B3** `native.offset_curve` (GEOSOffsetCurve round join) — 2026-10-25
- [x] **P0-B4** `native.concave_hull` (`env.number` = ratio) — 2026-10-25
- [x] **P0-B5** `native.minimum_rotated_rectangle` / `native.minimum_bounding_circle` — 2026-10-25
- [x] **P0-B6** `native.unary_union` (multi → dissolved) — 2026-10-25

### P0-C Line / polygon assembly

- [x] **P0-C1** `native.line_merge` — 2026-10-25
- [x] **P0-C2** `native.node` (noded multilinestring) — 2026-10-25
- [x] **P0-C3** `native.polygonize` (`GEOSPolygonize_full`) — 2026-10-25
- [x] **P0-C4** `native.remove_repeated_points` — 2026-10-25
- [x] **P0-C5** `native.snap` (geometry-to-geometry; not edit vertex snap) — 2026-10-25

### P0-D Measures / closest

- [x] **P0-D1** `native.hausdorff_distance` — 2026-10-25; densifyFrac param Deferred
- [x] **P0-D2** `native.frechet_distance` — 2026-10-25
- [x] **P0-D3** `native.shortest_line` / nearest points — 2026-10-25
- [x] **P0-D4** Predicates gap: `native.covers` / `native.covered_by` — 2026-10-25

### P0-E Tessellation surface

- [x] **P0-E1** `native.delaunay` → `geo::make_delaunay` / `delaunay_via_geos` — 2026-10-25
- [x] **P0-E2** `native.voronoi` → GEOS Voronoi polygons (flags=0) — 2026-10-25

### P0-F Catalog honesty

- [x] **P0-F1** Predicates already in IR listed in `geometry_file_op_table` — 2026-10-25
- [x] **P0-F2** Measures already in IR listed in catalog — 2026-10-25
- [x] **P0-F3** README under `src/gis/analysis/geometry` points to this pin — 2026-10-25

**P0 done when:** A1–A2, B1–B2, B6, C1 or C3, D1, E1, F1–F2 green; remaining P0 rows either done or marked **Deferred (GEOS API missing)** with SDK version note.

**P0 done-bar (2026-10-25):** met. Evidence: `ops_test` + `analysis_wkt_ops_test` exit 0.

---

## P1 — Product main path

### P1-A Layer / feature-set ops (kernel may stay in `gis/analysis`, product via Processing)

- [x] **P1-A1** Dissolve / unary aggregate by attribute key (layer in → layer/geom out) — 2026-10-25
- [x] **P1-A2** Clip layer by polygon layer (batch clip; builds on `native.clip`) — 2026-10-25
- [x] **P1-A3** Spatial select / join (predicate + optional attribute copy; not a DB join) — 2026-10-25
- [x] **P1-A4** Overlay subset: **identity** or **erase** (ArcGIS-shaped; implement via GEOS set ops + attribute policy doc) — 2026-10-25（`native.erase` + `native.identity`；identity 字段可选 `copy_fields` → `id_*`）
- [x] **P1-A5** Split line by point / cut polygon by line (minimal) — 2026-10-25（`native.split_line` / `native.cut_polygon`）

### P1-B `analysis/geometry` Eigen deepen

- [x] **P1-B1** Rigid / similarity align (Umeyama or equivalent) beside `affine_align_2d` — 2026-10-25
- [x] **P1-B2** PCA axes / oriented bbox from point set (may call P0-B5) — 2026-10-25
- [x] **P1-B3** Constructive affine: translate / rotate / scale as `native.*` (OGR/GEOS Affine, not only registration) — 2026-10-25
- [x] **P1-B4** Unit + `analysis_eigen_test` coverage for new fits — 2026-10-25

### P1-C Processing discoverability

- [x] **P1-C1** Views Processing panel lists all new `native.*` from catalog (no hand-maintained twin list) — 2026-10-25; `processing_composer` ← `plugin::builtin_op_catalog()` ← `gis::detail::builtin_op_catalog()`（含 `geometry_layer_op_table` / fit / P0 file ops）
- [x] **P1-C2** Buffer style params — **defaults documented** on `geo::buffer` / `buffer.h`（quadsegs=8；round endcap/join）；JSON endcap/join Deferred
- [x] **P1-C3** Plugin host Processing contract stays forward-only (`run_builtin_op`); no Shapely twin — as-built `plugin/runtime/processing`

**P1 done when:** A1–A3 + B1 or B3 + C1 green; overlay/split may remain partial with documented subset.

**P1 done-bar (2026-10-25):** met（A1–A5 + B1–B4 + C1–C3；buffer JSON endcap/join Deferred）。Evidence: `analysis_layer_ops_test` + `analysis_eigen_test` exit 0；Processing 单一 catalog 源。

---

## P2 — Topology (product owned by tool / EditSession)

> Kernels for predicates/set ops stay in `gis/analysis` / GEOS. **Rule evaluation + UX**
> live under tool / `feature_edit` / EditSession. Do not invent `analysis/topology/`
> god-object. Cross-track Map2D gap-pin **P1-4** / **P2-4**.

- [x] **P2-1** Must-not-overlap on EditSession commit — `validate_topology` + `CommitStatus::kTopology`（≡ Map2D P1-4；2026-10-25）
- [x] **P2-2** Must-not-have-gaps v1 — same `validate_topology` + `gap_tolerance`（2026-10-25）
- [x] **P2-3** Report surface — `last_topology_result().issues()`（feature / other ids + message；2026-10-25）
- [ ] **P2-4** Advanced rule set (ArcGIS-like catalog) — **same as Map2D P2-4**; post-M4

**P2 done when:** P2-1 + P2-3 green and industry matrix / Map2D §8 P1-4 closed together.

**P2 done-bar (2026-10-25):** P2-1…P2-3 met（`topology_check_test`）；P2-4 remains open with Map2D P2-4.

---

## Benchmark reference (families → vendors)

Use as review checklist when claiming “parity”; not every vendor name must become a `native.*`.

| Family | PostGIS | QGIS Processing | ArcGIS Pro | SmartGIS owner |
| --- | --- | --- | --- | --- |
| Set / overlay | `ST_Intersection`… | Vector overlay | Intersect / Union / Erase / Identity | `ops` GEOS → P0/P1-A |
| Buffer / offset | `ST_Buffer` / `ST_OffsetCurve` | Buffer / Offset | Buffer / Graphic Buffer | `geo::buffer` + P0-B3 |
| Simplify / densify | `ST_Simplify` / `ST_Segmentize` | Simplify / Densify | Simplify / Densify | P0-B2 + existing simplify |
| Hulls | `ST_ConvexHull` / `ST_ConcaveHull` | Convex/Concave hull | Minimum Bounding Geometry | convex done; P0-B4/B5 |
| Validity | `ST_IsValid` / `ST_MakeValid` | Check/Fix geometries | Check Geometry / Repair | P0-A |
| Tessellation | `ST_DelaunayTriangles` / `ST_VoronoiPolygons` | Delaunay / Voronoi | Create TIN / Thiessen | `geo::delaunay` + P0-E |
| Measures | `ST_Distance` / `ST_HausdorffDistance` | Distance matrix / Hausdorff | Near / Generate Near Table | IR distance + P0-D |
| Fit / transform | — / `ST_Affine` | Affine / warp tools | Spatial Adjustment / Project | `analysis/geometry` P1-B |
| Layer dissolve/join | aggregate + join | Dissolve / Join attributes by location | Dissolve / Spatial Join | P1-A |
| Topology | constraint triggers / extension | Topology Checker | Topology rules | P2 + tool |

---

## Done when

1. Algorithm OSS **§9** status table matches this plan’s checkboxes.  
2. Industry matrix **§9** overview row is **B** or better for single-geom GEOS surface; layer/topology rows honest.  
3. No new GEOS/CGAL vendor; no plugin-local geometry kernel copies.  
4. P0 done-bar (above) met; P1-A1–A3 and P1-C1 met or explicitly Deferred with owner.

**Audit date:** 2026-10-25  
**Auditor notes:** M2 toolbox skeleton (`m2-*-ok`) already green — this pin is **depth**, not a second Processing panel.
