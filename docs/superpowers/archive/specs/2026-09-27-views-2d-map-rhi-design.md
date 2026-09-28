<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Views 2D Map → RHI（GpuScene present）


> **Status: superseded** (2026-09-28 merge). Merged into `2026-09-27-map2d-frame-design.md` §Views RHI present. Do not revise here except mechanical link fixes.

**Status:** active  
**Date:** 2026-09-27  
**Note (2026-09-27):** Product 2D RHI Pass 终态为 **`src/effect/map`** (`effect::map::Pass`)。`src/render/map2d` 已从磁盘移除。CPU 布局在 `gis/vista/frame`。  
**Frame composition:** 命令列表的拼接与 present 改由 [`2026-09-27-render-frame-graph-design.md`](2026-09-27-render-frame-graph-design.md) 负责。本规格留下的是 Views 挂上 FlyCube / Null，以及 GDI 降级。二维像素现经 `gis::vista::Layout` → `effect::map::Pass`，不再经 `GpuScene::record_draws`。  
**Related:** RHI + dual scene [`2026-09-13-render-rhi-scene-design.md`](2026-09-13-render-rhi-scene-design.md)；Present Facade [`2026-09-19-legacy-render-present-facade-design.md`](2026-09-19-legacy-render-present-facade-design.md)；Scene3d / World [`2026-09-19-scene3d-world-gpuscene-design.md`](2026-09-19-scene3d-world-gpuscene-design.md)；UI Views [`2026-09-13-ui-views-mfc-migration-design.md`](2026-09-13-ui-views-mfc-migration-design.md)；2D CPU 帧 [`2026-09-27-map2d-frame-design.md`](2026-09-27-map2d-frame-design.md)；MapLibre Native pin removed（deferred）[`../archive/specs/2026-09-27-maplibre-out-of-gpu-design.md`](../archive/specs/2026-09-27-maplibre-out-of-gpu-design.md)。  
**Plan:** [`../plans/2026-09-27-views-2d-map-rhi.md`](../plans/2026-09-27-views-2d-map-rhi.md)

## Goal

1. Views 主图路径（`MapViewport` Map/Data + `MapScene`）默认经 **`GpuScene::record_draws` → `rhi::Device` execute/present** 上屏 GIS 2D 像素。  
2. Views 壳 chrome 仍走 `ui/gfx`；地图子 HWND content 区走 RHI（FlyCube 真 GPU；无 FlyCube 时 Null 可测录制）。  
3. 复用已有 `World::attach_vector_geoms` / `GpuScene` ortho / style paint 缝；GDI `MapScene::paint` 降为过渡（注记 / 调试），**不再是默认主像素路径**。

## Non-goals

- 不用 Skia Canvas 画 GIS 地图/地形。  
- 不引入 Qt；不把 `kGdi`/`kGl` stub 扶成第二 2D 引擎。  
- 不改 `legacy/render/bridge` Present 热点语义（`bind_rhi_present` 仍只记 HWND + Null）；Views 用独立 HWND 上的 `create_device(preferred_gpu)`。  
- 不在本轮做完整注记避让 / 底图瓦片 GPU 上传 / 风格完整度对标 GDI。  
- 不碰 `src/render/atmosphere` sky/fog 实现与 plugin 大搬迁。

## Locked decisions

| Topic | Choice |
| --- | --- |
| 调用链 | `MapScene::present_gpu(Device*, w, h)` → 自建 `gis::World`（层→`attach_vector_geoms`）→ `GpuScene::sync_from` + `set_view_ortho(view_world_extent)` → `record` / execute / present |
| 坐标 | MapScene 存 lon/−lat；上传 OGR 时 **Y 反翻转** 为 CRS84；ortho 用 `view_world_extent`（lon/lat） |
| 默认挂接 | Map Edit / Map Data：**优先 FlyCube**（与 Scene3d 对齐）；`SMT_PREFER_FLYCUBE_2D=0` 或 `SMT_FORCE_CONTENT_MAPVIEW_2D=1` 回退 ContentMapView/OOP/GDI |
| GDI 过渡 | GPU present 成功：overlay 仅注记/选中描边（`paint_annotation_overlay`）；失败或强制：`MapScene::paint` 全量 GDI |
| 调试开关 | `SMT_FORCE_GDI_MAP_OVERLAY=1`：跳过 2D `gpu_present_`，强制 GDI 全量；`SMT_PREFER_GDI_DEVICE=1`：跳过 FlyCube 创建设备（既有） |
| Null | 无 FlyCube / 测试：`create_device(kNull)` + `present_gpu` 稳定返回 true（有可见层或至少 clear pass） |
| 依赖 | `map_scene` → `gis::World` + `render::rhi` + `render::scene`；**禁止** `render` → legacy；**禁止** Views `#include "legacy/…"` |
| 注记 | `kText` 本轮仍走 GDI annotation overlay；向量 fill/line/point 走 RHI |

## Architecture

```
BrowserView::wire_map_scene
        |
        +-- map_edit_/map_data_->set_gpu_present → MapScene::present_gpu
        +-- overlay: GPU ok → paint_annotation_overlay
        |             else  → MapScene::paint (GDI full)
        v
MapViewport (Role::kMapEdit|kMapData, AttachMode::kFlyCube)
        |
        WM_PAINT → gpu_present_(rhi_device_, w, h) → device->present()
        |
        v
MapScene::present_gpu
        |
        +-- rebuild owned OGR geoms (Y unflipped) when doc dirty
        +-- World::attach_vector_geoms per visible layer
        +-- GpuScene::set_view_ortho(view extent)
        +-- set_background / set_instance_paint (style or Baidu defaults)
        +-- record → execute → present
```

## Success criteria

1. Living spec + plan 存在；Related 链到 RHI scene / present facade。  
2. `MapScene::present_gpu` 在 Null device 上对 seed 文档返回 true，且 StubCommandList 有 draw 或至少 clear。  
3. MapViewport 2D FlyCube 路径调用 `gpu_present_`（与 Scene3d 对称）。  
4. 默认开关：FlyCube 优先；文档写明 GDI 降级 env。  
5. `map_scene_test` / 相关测试绿；`build.bat` 目标可编。  
6. `src/render/README.md` / `src/app/views/README.md` 写明「Views 2D 主路径 = RHI」。

## Out of scope for later

- GPU 注记 / 碰撞；basemap TileProvider → GpuScene 纹理。  
- Per-feature 风格完整度对标 GDI china_city 全量。  
- MFC leftover GDI 主路径删除（仅 Views 默认切轨）。
