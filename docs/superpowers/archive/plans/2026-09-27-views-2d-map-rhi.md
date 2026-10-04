<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Views 2D Map → RHI — Implementation Plan


> **Status: superseded** (2026-09-28 merge). Merged into map2d-frame plan / living. Do not revise here except mechanical link fixes.

**Status:** active  
**Note (2026-09-27):** Map Pass 终态树为 `src/vista/map`（非 `src/render/map2d`）。本 plan 任务勾选仍有效；接线 include 以 as-built `vista/map/pass.h` 为准。  
**Spec:** [`../specs/2026-09-27-views-2d-map-rhi-design.md`](../specs/2026-09-27-views-2d-map-rhi-design.md)

## Task 1: `MapScene::present_gpu` + annotation overlay

- [x] Add `present_gpu(render::rhi::Device*, w, h)`：World 镜像 + GpuScene ortho + record/execute/present  
- [x] Owned OGR geom cache（Y unflip）；`paint_annotation_overlay` 仅注记/选中  
- [x] `map_scene` GN deps：`//src/gis/scene/world:world`、`//src/render:rhi`、`//src/render/scene:scene`  
- [x] `map_scene_test`：Null present_gpu after seed_default

## Task 2: MapViewport 2D FlyCube present

- [x] `attach()`：Map Edit/Data 默认优先 FlyCube（env 可退）  
- [x] `child_wnd_proc` WM_PAINT：MapEdit/MapData + FlyCube 走 `gpu_present_`（对称 Scene3d）  
- [x] 成功时 backbuffer 不叠全量 GDI

## Task 3: BrowserView 接线 + docs

- [x] `wire_map_scene`：edit/data `set_gpu_present` → `document_.present_gpu`；overlay 按 `last_gpu_present_ok`  
- [x] README：`src/render` + `src/app/views` 写明 Views 2D 主路径 = RHI  
- [x] `ninja -C out map_scene_test`（Null `present_gpu` ok）+ `SmartGisViews`
