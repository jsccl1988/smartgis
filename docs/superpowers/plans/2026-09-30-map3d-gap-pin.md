<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Map3D gap pin — Implementation Plan

> **As-built gap source:** [`../industry-gap-matrix.md`](../industry-gap-matrix.md) §3.2.1 Map3D 钉死清单（2026-09-30）。  
> **Living specs (do not open a new dated design):**  
> [`../specs/2026-09-13-render-rhi-scene-design.md`](../specs/2026-09-13-render-rhi-scene-design.md) ·  
> [`../specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md`](../specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md) (SP4 DEM) ·  
> [`../specs/2026-09-13-plugin-host-design.md`](../specs/2026-09-13-plugin-host-design.md) (§world3d pointcloud)

> **For agentic workers:** Stay on **master**. Checkboxes only. Do **not** `git commit` unless asked. **不**引入 Cesium Native；**不**做完整 GCM / 影视级海洋；**不**复活产品 Web GIS.

**Goal:** Turn industry Map3D gaps into executable, verifiable work items after M3 skeleton (`m3-*-ok`) and world3d LAS I/O landed.

**Out of scope here:** Map2D / carto / Processing toolbox (owned by other gap pins).

---

## P0 — must close for “走进城市场景”

- [x] **P0-A 双场景合拢收口：** leftover `DemHeightField` → thin wrap over `gis::DemRaster`; Views/`content` present path only seeds World → `GpuScene` (no parallel thick DEM). Spec: legacy umbrella SP4 Deferred + render-rhi-scene.  
  **Accept:** `dem_stereo_test` + `dem_raster_test` green; no new `DemHeightField` logic beyond adapter; `map_to_scene` / `Scene3dGpuPresent` still draw terrain from World mesh.
- [x] **P0-B 3D Tiles 产品流：** wire `stream_tileset` + `ensure_tileset_content` into scene3d present/orbit loop (not only `run_m3_self_test_hooks`); city/local tileset fixture with ≥1 real `b3dm`/`glb` content decode under LRU cap. Spec: render-rhi-scene §model/tiles; plan sibling [`2026-09-27-m3-city-3d-stream.md`](../archive/plans/2026-09-27-m3-city-3d-stream.md).  
  **Accept:** showcase or `--self-test` path loads fixture tileset, camera move changes `visible_uris`, resident cache ≤ budget, failed URI degrades without crash; `tileset_test` + present smoke green.  
  **Status:** done (2026-09-30 quiet lock): `TilesetStreamSession` + present pump + `GpuScene` cache + m3 hook camera URI check; `tileset_test` green; `SMARTGIS_BUILD_OWNER=map3d-p0b-verify build.bat debug scene3d_presenter_test` + `out\Debug\scene3d_presenter_test.exe` exit 0 (prior contested-lock AV not reproduced).
- [x] **P0-C DEM 瓦片化高度场：** tile/clip `DemRaster` (or World terrain chunks) by view AABB; sync selected tiles into `GpuScene` instead of one full-china coarse mesh. Spec: legacy umbrella DEM unify + render-rhi-scene terrain.  
  **Accept:** china or city DEM at interactive orbit stays under documented mesh-vert budget; zoom-in replaces coarse tiles with finer; unit or harness asserts tile count / vert cap.

## P1 — product depth

- [ ] **P1-A 点云生产路径：** shell/plugin open LAS/LAZ ≥1M with chunk cull + LOD thin stable; document PDAL optional (`smt_has_pdal`) vs in-tree LAS. Plan: [`2026-09-30-world3d-pointcloud-las.md`](2026-09-30-world3d-pointcloud-las.md). Spec: plugin-host §world3d.  
  **Accept:** `pointcloud_test` + plugin showcase / interact loads sample; >500k auto-thin; GpuScene draws non-empty `kPointCloud`.
- [ ] **P1-B 大气总验收：** close atmosphere upgrade Phase 3.4 against §1.2 five criteria (field scrub, ocean mid-tier, timeline, wind overlay, RHI boundary). Plan: [`2026-09-20-atmosphere-ocean-cloud-upgrade.md`](2026-09-20-atmosphere-ocean-cloud-upgrade.md). Spec: render-rhi-scene Atmosphere.  
  **Accept:** `--atmosphere-showcase` (or self-test) documents pass/fail per criterion; `m3-atmosphere-ok` remains green.
- [x] **P1-D Views Scene3D legacy look（2026-09-30）：** opt-in `Scene3dLookPreset::kLegacyStereo` — default face stays atmosphere; `--atmosphere-showcase=legacy` applies black clear + ocean sea + hypsometric China DEM + place-name overlays; content marks `look-legacy` / `labels-ok` / `coast-doc-*`; visual gate suite `atmosphere.legacy` reuses `legacy_scene3d_china` score. Spec: render-rhi-scene §Scene3d legacy look.  
  **Accept:** `views_launch_options_test` parses `legacy`; GPU showcase writes `atmosphere-showcase-legacy.bmp` with landish/black-clear gate; marks include `look-legacy` + `labels-ok`.
- [ ] **P1-C Tiles 内容面：** i3dm / pnts (or honest skip) + clearer geometricError/SSE product defaults beyond `select_tiles_limited` truncate. Spec: render-rhi-scene §model.  
  **Accept:** decode matrix table in module README; unsupported formats `put_failed` without retry storm.

## P2 — later

- [ ] **P2-A `SmtScene` octree → World query：** further delegate leftover octree queries (today AABB mirror only). Plan: [`2026-09-19-scene3d-world-gpuscene.md`](2026-09-19-scene3d-world-gpuscene.md) Deferred.
- [ ] **P2-B 活 PDAL install：** optional `build.bat t pdal` CI/doc path when network allows (stub already green).
- [ ] **P2-C 地形 LOD 观感：** clipmap / morph or equivalent mid-tier look without Cesium terrain pipeline.

---

## Verify (repo root)

```bat
build.bat debug tileset_test
build.bat debug dem_raster_test
build.bat debug pointcloud_test
build.bat debug scene_test
REM optional: SmartGisViews.exe --self-test  (m3-dem-ok / m3-tiles-ok / m3-atmosphere-ok)
```

## Non-goals (locked)

- Cesium Native 一锅端  
- 完整 GCM / 影视级海洋  
- 产品 Web GIS / mapd  
