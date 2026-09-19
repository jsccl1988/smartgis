<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

Status: active

# Leftover 2D：GDI+ 抗锯齿 + 道路分层 + MapLibre 子集注记

## 目标

SmartGis.exe leftover GDI 路径对齐百度可读性：真抗锯齿、道路分层绘制（有数据才可见）、注记达到 MapLibre 子集（网格避让、沿线旋转、多字段）。

## 非目标

- Views / `MapScene` 本轮不接 GDI+。
- 不往 `china_city` 塞道路几何（样式路径先通）。
- 不做完整 Style Spec（无 `text-max-width` 折行、无跨帧全局优化）。

## 设计

### GDI+ AA

- 进程内一次 `GdiplusStartup`；失败则回退现有 GDI。
- `Graphics` 绑当前 `HDC`：`SmoothingModeAntiAlias`、`TextRenderingHintClearTypeGridFit`。
- 线/面/字走 AA 路径；`ExtCreatePen` 仅作回退。

### 道路分层

| kind/class | casing | fill | 相对宽度 |
|---|---|---|---|
| expressway | 深灰 | 黄 | 最宽 |
| highway | 灰 | 白 | 中 |
| road | 浅灰 | 白 | 窄 |

绘制：面 → 河 → 道路 casing → 道路 fill → 点 → 注记。无道路层时行为不变。

### 注记（MapLibre 子集）

- 字段：`anno` > `name` > `text`。
- 点注：优先级 + LOD + **空间哈希网格**避让（替代纯 O(n²)）。
- 沿线：折线中点切向采样 + GDI+ `Matrix` 旋转；河/路可用。
- halo / 字号沿用 `carto2d_label_px` / `carto2d_halo_px`。

## 验证

- `map_carto2d_test`：网格、道路宽度表、沿线角度。
- `gdi_map_paint_test` + `.\build.bat e2e` / `te`。
