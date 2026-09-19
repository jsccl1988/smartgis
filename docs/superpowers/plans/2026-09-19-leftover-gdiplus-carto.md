<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Leftover GDI+ / 道路 / 注记 Implementation Plan

> **For agentic workers:** 按任务执行；`master`；中文进度；注释英文。

**Goal:** leftover 真 AA + 道路分层样式路径 + MapLibre 子集注记。

**Architecture:** `map_carto2d` 管样式/避让/沿线几何；`gdi_aux_api` 管 GDI+ 绘制；`gdi_renderdevice` / `gdi_renderthread_draw` 接线。

**Tech Stack:** C++23, GDI+, OGR, 现有 `map_carto2d_test` / `gdi_map_paint_test`。

---

### Task 1: carto2d API（网格、道路级、沿线）

Files: `map_carto2d.h/.cc`, `map_carto2d_test.cc`

- `carto2d_road_class`, casing/fill 色与宽度
- `MapCarto2dFrame` 网格避让
- `carto2d_line_label_pose` 沿线位姿
- 字段解析 `carto2d_label_text`

### Task 2: GDI+ 辅助

Files: `gdi_gdiplus.h/.cpp`, `gdi_aux_api.*`, `BUILD.gn` link `gdiplus.lib`

- startup/shutdown
- AA polyline / polygon / rotated string

### Task 3: Device 接线

Files: `gdi_renderdevice.*`, `gdi_renderthread_draw.cpp`

- `m_isRoad` / road class
- 双描边道路；AA 线面；DrawAnno 旋转

### Task 4: 验证

`.\build.bat e2e` && `.\build.bat te`
