<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# M3 — 城市 DEM + 3D Tiles 流式 + 大气旁路 Implementation Plan

> **For agentic workers:** Stay on **master**. Do **not** `git commit`. Do **not** create branches.

**Goal:** 城市场景：DEM 可用 + 至少一个 3D Tiles 集经 `select_tiles` / World 流式挂载不炸内存；大气海/云可按 upgrade 判据开关；`--self-test` 可挂 `m3-*` hooks。

**Architecture:** 复用 `gis::select_tiles`、`World::attach_tileset` / `apply_tileset_selection`、`DemRaster`、`Scene3dController` atmosphere demo。扩展：LOD 选择 + 内容缓存上限 + 失败降级；自测用小 fixture tileset（或内存假树）。

**Tech Stack:** C++23、`gis/scene/world`、`gis/scene/assets`、`render/atmosphere`、Scene3d、GN。

## Path ownership（并行硬约束）

**MAY edit:**
- `src/gis/scene/world/**`
- `src/gis/scene/assets/**`（tileset / decode）
- `src/render/atmosphere/**`、`src/render/scene/**`（仅大气/场景接线必要处）
- `src/app/views/scene3d_controller.*`、`scene3d_rhi_session.*`、`scene3d_*_test.cc`
- `src/app/views/BUILD.gn`（仅 scene3d 相关 deps）
- `testing/data/**`（小 tileset fixture，若需）
- `docs/superpowers/plans/2026-09-27-m3-city-3d-stream.md`
- atmosphere upgrade plan 小节（可选回写）

**MUST NOT edit:**
- `src/app/views/main.cc`、`browser_view.*`、`map_scene.*`
- `src/plugin/processing/**`、`src/ui/gis/processing*`
- `src/gis/model/edit/**`、`src/content/**`（除只读）
- M2/M4 计划文件

## Acceptance

1. DEM：`Scene3dController` / World 路径加载 `china_dem.tif`（或现有种子）present 不崩；测或 hook 断言 mesh/height 有信号。
2. 3D Tiles：解析 fixture → `select_tiles` → `apply_tileset_selection`；内存有上限（淘汰或 cap）；测绿。
3. 大气：已有 `enable_atmosphere_demo` / showcase 路径保持可用；必要时补 `m3-atmosphere-ok` hook API 在 `Scene3dController`。
4. 暴露 `bool Scene3dController::run_m3_self_test_hooks(std::string* err)`（或静态测试 exe）供父 agent 调用。

## Tasks

- [x] Task 1: tileset 流式选择 + 缓存 cap + 单元测试。
  - `gis::TilesetContentCache` / `select_tiles_limited` / `ensure_tileset_content`
  - `tileset_test` + `World::stream_tileset` in `scene_test`
- [x] Task 2: World/Scene3d 挂载 DEM + tileset 一条路径。
  - DEM via `seed_china_dem_into_world`; tiles via fixture + `stream_tileset`
  - Fixture: `testing/data/m3_city_tileset.json` (in-memory twin in hook)
- [x] Task 3: 大气开关 hook（复用现有 demo）。
  - `run_m3_self_test_hooks` toggles `enable_atmosphere_demo` then both off
- [x] Task 4: 文档预留 exit 90–92；不改 main.cc。

## Self-test hook + reserved exits（父 agent / main 接线时用）

| Hook tag (`err` on failure) | Meaning | Reserved Views exit |
| --- | --- | --- |
| `m3-dem-ok` | DEM seed / height signal failed | **90** |
| `m3-tiles-ok` | tileset parse / stream / cache cap failed | **91** |
| `m3-atmosphere-ok` | atmosphere on/off failed | **92** |

API: `bool Scene3dController::run_m3_self_test_hooks(std::string* err)` — returns
`true` when all three pass; on failure writes the tag above into `err`.
**Do not** wire into `main.cc` from this milestone (path ownership). Parent agent
may call the hook from `--self-test` later.

Verify:

```bat
build.bat scene3d_controller_test
build.bat tileset_test
build.bat scene_test
```

## Constraints

- **不**引入 Cesium Native。
- 禁止 Qt；新函数 `snake_case`；注释英文。
- YAGNI：不做全球 Ion；本地 fixture 即可。
