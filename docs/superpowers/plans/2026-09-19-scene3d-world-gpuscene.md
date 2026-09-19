<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# SP4 Scene3D → World / GpuScene — Implementation Plan

> **For agentic workers:** follow checkboxes; stay on `master`; no commit unless asked.

**Goal:** Align leftover DEM / scene3d seed path with RHI dual scene (`gis::World` + `GpuScene`); thin adapters only.  
**Spec:** [`../specs/2026-09-19-scene3d-world-gpuscene-design.md`](../specs/2026-09-19-scene3d-world-gpuscene-design.md)

## Constraints

- Path partition: `src/legacy/render/scene3d/**`, `src/gis/world/**`, `src/render/scene/**`（仅必要时）, docs。  
  Views 薄接线允许：`src/app/views/scene3d_controller.*` + `BUILD.gn`（不碰 bridge / tool / app MFC）。  
- **Do not** edit `src/legacy/render/bridge/**` present 热点。  
- No Qt; English source comments; copyright 2026; `snake_case` new APIs.  
- SP2 Present 缝保持：`leftover_record_map_frame` + `bind_rhi_present` Null。

---

### Task 1: Spec + 伞状 Child 表

**Files:**
- Create: `docs/superpowers/specs/2026-09-19-scene3d-world-gpuscene-design.md`
- Create: `docs/superpowers/plans/2026-09-19-scene3d-world-gpuscene.md`
- Modify: `docs/superpowers/specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md` Child 表

**Steps:**
1. [x] Living design Status `active`；含成功标准模板各节。  
2. [x] Plan 勾选清单。  
3. [x] 伞状 Child：SP4 链到本 spec / plan。

---

### Task 2: `DemHeightField` 掩膜委托 `gis::land_mask`

**Files:**
- Modify: `src/legacy/render/scene3d/dem_height_field.cc`
- Modify: `src/legacy/render/scene3d/BUILD.gn`（`scene3d_sources` deps `//src/gis/world:land_mask`）

**Steps:**
1. [x] `#include "gis/world/land_mask.h"`；删除本文件内重复 even-odd 实现。  
2. [x] `point_in_lonlat_ring` / `any_ring_contains` 转发 `gis::`（保留 `render::` 导出；bbox  cull 留在 leftover 壳）。  
3. [x] `mask_outside_rings` 经 `any_ring_contains` → `gis::point_in_lonlat_ring`。  
4. [x] `dem_stereo_test` 既有掩膜用例仍绿。

---

### Task 3: DEM envelope → World / GpuScene 缝（第一刀）

**Files:**
- Create: `src/legacy/render/scene3d/dem_to_world.h`
- Create: `src/legacy/render/scene3d/dem_to_world.cc`
- Modify: `src/legacy/render/scene3d/BUILD.gn` + `dem_stereo_test` deps  
- Modify: `src/legacy/render/scene3d/dem_stereo_test.cc`
- Modify: `src/render/scene/scene_test.cc`（`kTerrain` sync）

**Steps:**
1. [x] `seed_dem_height_field_into_world(World*, DemHeightField&, name)` → `attach_terrain`（elev→Z）。  
2. [x] `dem_stereo_test`：synthetic DEM → World `kTerrain`；`scene_gpu_test`：`attach_terrain` → `GpuScene::sync_from`（拆测避免 legacy_render+gis+render 三 DLL 上 `World::nodes_` ABI）。  
3. [x] `map_to_scene.h` 注释指向 `dem_to_world`；本轮不扩 `seed_*` 签名。

---

### Task 4: 验证（第一刀）

**Steps:**
1. [x] `dem_stereo_test` / `scene_gpu_test` / `land_mask_test` / `scene_test` 绿。  
2. [x] `build.bat` exit 0（LNK1168 时曾 rename 解锁 `legacy_render_d.dll`）。  
3. [x] 确认未改 bridge present 文件。

---

### Task 5: DEM mesh + map/Views 实线（第二刀）

**Files:**
- Modify: `src/gis/world/scene.h` / `scene.cc`（`set_terrain_mesh` / Node mesh 字段）
- Modify: `src/legacy/render/scene3d/dem_to_world.*`（`build_mesh` → World）
- Modify: `src/legacy/render/scene3d/map_to_scene.*`（`seed_dem_into_map_world` + `map_seeded_world`）
- Modify: `src/render/scene/scene.*`（terrain mesh / AABB fallback upload + `record_kind(kTerrain)`）
- Modify: `src/app/views/scene3d_controller.*` + `BUILD.gn`；`dem_height_field_static` 并入 `dem_to_world`
- Modify: `dem_stereo_test` / `scene_gpu_test`（mesh 断言；仍拆测）

**Steps:**
1. [x] `World::set_terrain_mesh`；`seed_dem_height_field_into_world` 附带 coarse mesh。  
2. [x] `map_to_scene` stereo underlay 调用 seed；导出 `map_seeded_world()`。  
3. [x] `GpuScene`：sync 拷贝 mesh；rebuild 优先 mesh → geom_3d → AABB；draw `kTerrain`。  
4. [x] Views：`rebuild_local_mesh` 经 World/dem_to_world 取 mesh（仍静态链 DEM 加载；SP5 再去 `dem_height_field_static`）。  
5. [x] 相关 test + `build.bat`。  
6. [x] 不改 bridge present；不搬 `SmtScene` octree。

---

### Task 6: Octree mirror + Views DEM API + present_gpu（第三刀）

**Files:**
- Create: `src/legacy/render/scene3d/scene_to_world.*`（AABB→GIS + `seed_smt_scene_aabbs_into_world`）
- Create: `src/gis/world/dem_raster.*` + `dem_raster_test`
- Modify: `bl3d_scene.cpp`（`CreateOctTreeSceneMgr` 一切换点）、`map_to_scene.cc`
- Modify: `src/render/scene/scene.*`（`set_view_camera`）
- Modify: `src/app/views/scene3d_controller.*` + `BUILD.gn`（去 `dem_height_field_static`；`present_gpu`→`GpuScene::record`）
- Modify: docs（本 plan / SP4 spec / SP5 / `src-layout`）

**Steps:**
1. [x] 纯逻辑 `leftover_aabb_to_gis` + `attach_gis_aabb`；`dem_stereo_test` 覆盖。  
2. [x] `CreateOctTreeSceneMgr` / `seed_geojson_into_scene` 刷新 World `kEmpty` 镜像（不删 octree）。  
3. [x] `gis::DemRaster` / `seed_china_dem_into_world`；Views 去 `dem_height_field_static`（`assert_no_deps`）。  
4. [x] `GpuScene::set_view_camera`；Views `present_gpu` 走 `sync_from` + `record` + orbit。  
5. [x] 相关 test + `build.bat`；不改 bridge present。

---

### Deferred（后续）

- [ ] leftover `DemHeightField` 薄包装 `gis::DemRaster`（去重复实现）。  
- [ ] `SmtScene` octree 查询路径进一步委托 World（当前仅 AABB 镜像）。  
- [ ] SP5 编译闸门收口文档与 `test_shell` 再对齐。
