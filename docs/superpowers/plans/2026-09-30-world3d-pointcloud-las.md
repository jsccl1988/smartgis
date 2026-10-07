<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# world3d point cloud (LAS/LAZ) — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Shared GIS point-cloud load (LAS/LAZ + legacy txt) and visible 3D scene draw via `World`/`GpuScene`; `world3d` only registers commands.

**Architecture:** Approach 2 — `vista/component/world/pointcloud` owns buffer + I/O; Browser/`World3dSceneWriter` commits; P0 LAS + txt + LAZ (`third_party/.src/LASzip`); P1 chunk/thin; P2 octree/LOD (`third_party/octree`).

**Tech Stack:** ASPRS LAS 1.2/1.4 reader (P0), LASzip (vendored, LAZ), GN/`build.bat`, Views shell.

**Spec:** [`../specs/2026-09-13-plugin-host-design.md`](../specs/2026-09-13-plugin-host-design.md) §world3d pointcloud LAS · [`../specs/2026-09-13-render-rhi-scene-design.md`](../specs/2026-09-13-render-rhi-scene-design.md) §Point cloud GPU draw

## Global Constraints

- Stay on `master`; compile via `build.bat` + `out/.build.lock`.
- Two-level namespaces; `snake_case` functions; English comments.
- No Qt; no new dated design specs.
- Processing factories must not touch Views.
- PDAL is P3 only.

---

### Task 1 — `gis` PointCloud + LAS/txt I/O

**Files:**
- Create: `src/vista/component/world/pointcloud/{point_cloud,las_io,text_io,load}.{h,cc}`
- Modify: `src/vista/component/world/BUILD.gn`
- Create: `src/vista/component/world/pointcloud/pointcloud_test.cc` + sample `.las` fixture

- [x] Step 1: `PointCloud` buffer (xyz float, optional rgba, AABB)
- [x] Step 2: ASPRS LAS reader (formats 0/2/3); LAZ via `load_las_via_laszip`
- [x] Step 3: Legacy `x,z,y,r,g,b` txt loader (leftover compatible)
- [x] Step 4: `load_point_cloud(path)` dispatch; unit test green

**Done when:** `pointcloud_test` loads fixture LAS + txt.

---

### Task 2 — World + GpuScene draw

**Files:**
- Modify: `world.h` / `world.cc` (point payload on `kPointCloud`)
- Modify: `tessellate.{h,cc}` (`tessellate_point_cloud`)
- Modify: `vista/pass/world/pass.{h,cc}` (sync + rebuild + `record_kind` for pointcloud)

- [x] Step 1: `set_pointcloud_points` on World node
- [x] Step 2: Tessellate points → tiny triangles
- [x] Step 3: GpuScene sync/rebuild/record for `kPointCloud`

**Done when:** scene unit path draws non-empty mesh for attached cloud.

---

### Task 3 — GisScene + Browser + world3d UI

**Files:**
- Modify: `feature_edit`, `gis_scene` (`add_point_cloud_layer`)
- Modify: `browser.cc` (use `gis::load_point_cloud`)
- Modify: `scene_commands.cc` (file filter `*.las;*.laz;*.txt`)
- Modify: `manifest/plugin.json` titles as needed

- [x] Step 1: Map point features from cloud XY
- [x] Step 2: Browser writer loads via shared API
- [x] Step 3: File picker accepts LAS/LAZ/txt

**Done when:** `world3d.add_pointcloud` / interact can open LAS sample and paint.

---

### Task 4 — P1 / P2 / LAZ (claimed)

- [x] LASzip codec GN (`LASunzipper` / `LASzipper`) + `laz_io` for `.laz` (wired through `load_las_file`)
- [x] P1: `build_point_cloud_chunks` + World stores chunks; GpuScene one mesh/chunk AABB cull
- [x] P2: unibn `select_point_cloud_lod` + auto-thin at >500k in `set_pointcloud_points`
- [x] Sample LAS fixture `testing/data/plugin/world3d_pointcloud_sample.las` (uncolored fallback); showcase prefers `pointcloud_public_sample.txt` (china_dem RGB)
- [x] `world3d.add_pointcloud` command/processing alias
- [x] `pointcloud_test` LAS↔LAZ round-trip green
- [x] P3: PDAL processing — Task 5 stub path (live install optional)

**Done when:** `pointcloud_test` green (LAS+LAZ round-trip); Views links gis pointcloud path. **Met 2026-09-30** (P3 → Task 5).

---

### Task 5 — P3 PDAL processing (1+2: hard when installed, stub otherwise)

**Files:**
- Modify: `third_party/manifest.json` (+ `install.py` markers), `third_party/gn/BUILD.gn`, `third_party/BUILD.gn`, `build/smartgis.gni`
- Create: `src/vista/component/world/pointcloud/pdal_io.{h,cc,stub.cc}` + test
- Modify: `world/BUILD.gn`, `world3d` scene_commands + manifest, Browser optional buffer attach

- [x] Step 1: Manifest + CMake pin for PDAL; `has_pdal` via `file_exists.py` on `.install/include/pdal/PipelineManager.hpp`
- [x] Step 2: `run_pdal_to_point_cloud` / `pdal_is_available`; stub returns `pdal_not_built`
- [x] Step 3: Processing `world3d.pdal_read` + `world3d.pdal_pipeline`; scene attach via map layer
- [x] Step 4: `pdal_io_test` green on stub (`pdal_not_built`); live PDAL optional via `build.bat t pdal` when network allows

**Done when:** tree compiles without PDAL; with `build.bat t pdal`, processing reads sample LAS via PDAL. **Met without install (stub path) 2026-09-30**; live install deferred (slow GitHub fetch).

### Follow-up — Map3D gap pin

- [ ] 大云生产路径稳定（≥1M + 文档化 in-tree vs PDAL）— **map3d-gap-pin P1-A**；见 [`../industry-gap-matrix.md`](../industry-gap-matrix.md) §3.2.1  
- [ ] 活 PDAL install 可选绿 — **map3d-gap-pin P2-B**；计划 [`2026-09-30-map3d-gap-pin.md`](2026-09-30-map3d-gap-pin.md)