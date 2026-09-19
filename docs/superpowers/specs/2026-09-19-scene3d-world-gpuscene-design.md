<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# SP4 — Scene3D → World / GpuScene

**Status:** active  
**Date:** 2026-09-19  
**Scope:** 将 leftover `scene3d` / DEM / map 种子路径对齐 RHI **双场景**（`gis::World` 逻辑世界 + `render::scene::GpuScene` GPU 镜像）；leftover `SmtScene` 逐步变薄适配器。  
**Related:** 伞状 [`2026-09-19-legacy-deep-abstraction-umbrella-design.md`](2026-09-19-legacy-deep-abstraction-umbrella-design.md) §SP4；RHI 双场景 [`2026-09-13-render-rhi-scene-design.md`](2026-09-13-render-rhi-scene-design.md)；Present Facade [`2026-09-19-legacy-render-present-facade-design.md`](2026-09-19-legacy-render-present-facade-design.md)（**SP2 缝已落地；本规格不改 bridge present 热点**）。  
**Plan:** [`../plans/2026-09-19-scene3d-world-gpuscene.md`](../plans/2026-09-19-scene3d-world-gpuscene.md)

## Goal

1. leftover DEM / 地图种子的**逻辑 envelope** 可写入 `gis::World`（`kTerrain`），并由 `GpuScene::sync_from` 镜像；不在 `scene3d` 内长第二套引擎。  
2. 纯几何掩膜（lon/lat ring）以 `gis::world::land_mask` 为权威；`DemHeightField` 只保留 leftover 导出壳。  
3. 增量：热点提取可测纯逻辑 → 切换一处 → 测；不 wholesale 搬迁 `SmtScene` octree。

## Non-goals

- 不破 leftover DLL `dll_stem` / `SCENE3D_EXPORT_*` / `Smt_*` 导出名（本子规格无 leftover-only break）。  
- 禁止 Qt；不复活 D3D9；不 vendor 第二套场景图。  
- **不**改 `src/legacy/render/bridge/**` Present 热点（SP2 所有权）。  
- **不**改 `src/legacy/tool/**`；不大改 `src/legacy/app|ui`（SP3）。  
- 不在本轮把 leftover `StereoTerrain` / `Smt2DGeoObject` 网格整包上传到 FlyCube。

## Locked decisions

| Topic | Choice |
| --- | --- |
| 手法 | Facade strangler：纯逻辑进 `gis::world` / 薄适配；`scene3d` 保留 DEM 网格与 leftover 对象壳 |
| 双场景 | 与 RHI 规格一致：`gis::World` + `render::scene::GpuScene`；禁止第三套逻辑场景 |
| 本轮热点 | **(A)** DEM envelope → `World::attach_terrain`；**(B)** `DemHeightField` 掩膜委托 `gis::land_mask`；**(C)** DEM CPU mesh → World → GpuScene；**(D 第三刀)** octree AABB→World 镜像；Views `gis::DemRaster`；`present_gpu`→`GpuScene::record`+外置相机 |
| AABB 约定 | World 节点用 GIS `(min_x,min_y,min_z)–(max_x,max_y,max_z)`：水平 lon/lat，**竖向 elev（已乘 vertical_exaggeration）进 Z**；leftover mesh 仍为 Y-up `(lon, elev, lat)`，适配器负责语义注释，不混写 |
| Present | 继续走 SP2：`bind_rhi_present` Null + GDI BitBlt；本规格不碰 |
| ABI | 保留 `render::LonLatRing` / `point_in_lonlat_ring` 导出；实现转发 `gis::`；DEM→GpuScene **拆测**（legacy 测 seed+mesh；render 测 `set_terrain_mesh`+sync） |

## Path ownership

| 可改 | 禁改 |
| --- | --- |
| `src/legacy/render/scene3d/**` | `src/legacy/render/bridge/**`（SP2 present） |
| `src/gis/world/**`（land_mask / World 薄扩展） | `src/legacy/tool/**`、`src/tool/**` |
| `src/render/scene/**`（仅当 GpuScene 需补 terrain 镜像缝；本轮优先测已有 `sync_from`） | `src/legacy/app/**`、`src/legacy/ui/**`（SP3） |
| 本 spec / plan；伞状 Child 表 | 终局 → legacy 新依赖 |

## Dependency

```
legacy/render/scene3d (DemHeightField / map_to_scene / scene_to_world)
        │  单向允许
        ▼
gis::World  +  gis::land_mask  +  gis::DemRaster
        │
        ▼
render::scene::GpuScene::sync_from / record(+set_view_camera)  →  render::rhi
```

Views **不得** `#include "legacy/…"`（第三刀已切 `gis::DemRaster`）。`src/render` 终局 **不得** 依赖 `legacy_render`。

## ABI

- leftover：`dll_stem` / `SCENE3D_EXPORT_*` / 现有 `map_to_scene` / `DemHeightField` 导出名：**不变**。  
- 新 API：`snake_case`；公开命名空间两层（`gis` / `render`）。

## Success criteria

1. [x] Living design 含 Goal / Non-goals / Locked / Path / Dependency / ABI / Success / Out of scope。  
2. [x] `DemHeightField` 掩膜 / `point_in_lonlat_ring` 委托 `gis::land_mask`（无第二套 even-odd 算法体）。  
3. [x] `seed_dem_height_field_into_world` 存在；`dem_stereo_test` 覆盖 synthetic DEM → World `kTerrain` + CPU mesh；`scene_gpu_test` 覆盖 `kTerrain` → `GpuScene` upload/draw（AABB fallback + `set_terrain_mesh`）。  
4. [x] 热点切换：`point_in_lonlat_ring` / 掩膜路径委托 `gis::`；`map_to_scene` stereo 实线 seed + `map_seeded_world()`；Views mesh 经 World。  
5. [x] 不改 SP2 bridge present 文件行为。  
6. [x] 相关 `*_test` 绿；`build.bat` exit 0。  
7. [x] 第三刀：`leftover_aabb_to_gis` / `seed_smt_scene_aabbs_into_world`；`CreateOctTreeSceneMgr` 一切换点；Views 无 `dem_height_field_static`；`present_gpu` 经 `GpuScene::record` + `set_view_camera`。

## Out of scope for later

- SP2：Present / Paint 继续收口（本规格只消费已落地缝）。  
- SP3：Host / Catalog / attrs。  
- leftover `DemHeightField` 包装 `gis::DemRaster`（去重复）。  
- `SmtScene` octree 查询整段委托 World（当前仅 AABB 镜像节点）。  
- SP5 闸门与 `test_shell` 再对齐。

## Done when（第三刀）

- Octree→World 可测 AABB 缝 + 一切换点；Views DEM 经 GIS API；`present_gpu` 走 GpuScene+外置相机。  
- 不 wholesale 删 octree；不 commit / 不新开分支 / 不强制 reindex。