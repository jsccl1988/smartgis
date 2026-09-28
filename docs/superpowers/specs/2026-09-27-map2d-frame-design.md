<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Views 2D MapFrame（CPU layout）

**Status:** active  
**Date:** 2026-09-27  
**Updated:** 2026-09-28 — §Shell HUD-in-frame: ShellOverlayEffect PSO blend kSrcAlpha + hole-punch TODO; earlier same day DrawRequest.shell → FlyCube overlay; §Perf dual-speed + Phase 2a/2b/2c landed; MSAA / text-color / interpolate still deferred.  
**Note:** Product RHI Pass under **`src/effect/map`**. CPU layout under `src/gis/vista/frame` (`gis::vista`). MapLibre Native pin removed (deferred) — [`../archive/specs/2026-09-27-maplibre-out-of-gpu-design.md`](../archive/specs/2026-09-27-maplibre-out-of-gpu-design.md).  
**Related:** RHI living [`2026-09-13-render-rhi-scene-design.md`](2026-09-13-render-rhi-scene-design.md)；desktop shell [`2026-09-27-views-desktop-shell-design.md`](2026-09-27-views-desktop-shell-design.md)；archived RHI-present twin [`../archive/specs/2026-09-27-views-2d-map-rhi-design.md`](../archive/specs/2026-09-27-views-2d-map-rhi-design.md)。  
**Plan:** [`../plans/2026-09-27-map2d-frame.md`](../plans/2026-09-27-map2d-frame.md)

## Goal

Views 2D 要素绘制是一帧新的 CPU 布局，不是 `GpuScene::rebuild_meshes` 里再塞逻辑，也不是 maplibre-native。

```
StyleDocument + features + viewport
        |
        v
gis::vista::Layout     pure CPU, no RHI, no HWND, no legacy includes
        |                (was transitional gis::map2d)
        v
MapFrame               POD
        |
        v
effect::map::Pass      product RHI pass (src/effect/map; was render/map2d)
```

`gis` 不 `#include` `render` / `effect`，不链接 GDI DLL。字形像素光栅化属于 effect map pass（Windows 光栅器）。本模块只消费注入的 `GlyphMetrics`。

## Non-goals

- 不引入 glyphs PBF、sprite atlas JSON、heatmap、hillshade、fill-extrusion、`interpolate`。
- 不把调色板常量写进 GPU pass。颜色只活在 Style JSON / `ResolvedPaint`。
- 不改 `src/gpu/paint`（那是 GPU 进程的 preview / basemap 入口，不是这块 CPU 布局）。
- 不把逻辑写进 `GpuScene::rebuild_meshes`。Views `Map2dPresenter` 帧缓存与 Pass 视觉增强见 §Perf（本文件）。

## Locked API

公共头（as-built）：`src/gis/vista/frame/frame.h`。命名空间 `gis::vista`（历史文稿曾写 `gis::map2d`）。辅助在 `gis::vista::detail`。

```cpp
MapFrame Layout::build(const LayoutInput& in,
                       const std::vector<LayerBatch>& layers) const;
std::string default_carto_style_json();
```

`LayerBatch` 取代扁平 `OGRFeature` 列表：样式按 `source-layer` 匹配，一批几何加一份属性表。

```cpp
struct LayerBatch {
  std::string source_layer;
  std::vector<const OGRGeometry*> geoms;
  std::vector<std::map<std::string, std::string>> attrs;
};
```

`LayoutInput` 在冻结字段之外多一个符号表，供 icon / fill-pattern 查像素尺寸：

```cpp
struct LayoutInput {
  View view;
  const gis::style::StyleDocument* style = nullptr;
  double zoom = 0;
  std::vector<TileSlot> tiles;
  const GlyphMetrics* metrics = nullptr;
  std::vector<SymbolAsset> symbols;
};
```

`DrawItem` 在冻结字段之外多了像素旋转枢轴 `anchor_x` / `anchor_y`。`kRaster` 把 `TileSlot::texture_key` 放进 `codepoint`（`symbol_id` 留空）。颜色是 `0xAARRGGBB`，与 `ResolvedPaint` 一致。

几何网格是世界坐标（`pixel_space == false`）。Icon / text 四边形是视图像素、旋转前轴对齐（`pixel_space == true`）；pass 绕 `anchor_x/y` 转 `angle_rad`。同一段文字的每个 codepoint 是一条 `kText`，`u/v` 为 0。Icon 的 `u/v` 为整张符号的 0..1，并设置 `symbol_id`。

屏幕 Y 向下：`sx = (x - min_x) / (max_x - min_x) * width`，`sy = (max_y - y) / (max_y - min_y) * height`。

## Layers

画家顺序 = 样式文档顺序。

| 类型 | 输出 |
| --- | --- |
| background | `background-color` × `background-opacity` 写到帧上，不是 mesh。后写的覆盖先写的。 |
| raster | 每个 `TileSlot` 一个世界坐标两三角四边形，`u/v` 0..1，`v = 0` 在北边缘（`max_y`）。不透明度 = `tile.opacity * raster-opacity`。输入里没有的瓦片不出现。 |
| fill | `gis::tessellate_geometry`。`fill-color` / `fill-opacity`。`fill-pattern` 在 `symbols` 里有同名 id 时写入 `symbol_id`，否则纯色。 |
| line | `gis::tessellate_line` + `LineTessOptions`。`line-width` 像素经 `world_units_per_pixel` 变成世界半宽；dash 同样从像素换到世界单位。路缘是样式里更早的一条 line，不是写死的 road 模式。 |
| circle | 世界单位三角扇。半径 = `circle-radius` 像素 × 每像素世界单位。32 段，顶点数 > 4。不发旧菱形。 |
| symbol | 点或沿线。`symbol-placement` 为 `line` 时取中点切线（角度保持正向可读），否则点或包络中心、角度 0。 |

`symbol-placement`、`text-halo-color`、`text-halo-width` 只解析常量。缺省 placement 是 `point`。样式里写了 halo 就用样式值（宽度 0 表示不要晕）；没写宽度时用 carto 的 `halo_px(priority)`，没写颜色且宽度 > 0 时晕色为 `0xFFFFFFFF`。

文字 `{key}` 用要素属性替换。`text-field` 为空时回退 `anno` / `name` / `text`。

## Collision

从 `src/legacy/render/gdi/map_carto2d.cc` 移植的纯函数（不 include 该头）：priority、LOD max、budget、halo px、label box、沿线中点姿态、空间哈希。`fblc` = 视口宽度像素 / 世界宽度。分数更小的优先；同一 symbol 层内按 priority 稳定排序后再占格，重叠的低优先级不进 `MapFrame`。Icon 与文字占同一张网格，一个要素一次 `try_keep`（两者的盒子求并）。超出视口或超出 LOD / budget 的同样省略。

## Errors

- 找不到 icon（`symbols` 里没有该 id，或宽高 ≤ 0）：跳过 icon，文字照常放置。
- `metrics == nullptr`：跳过文字；icon 仍可放置。
- 两者都没有：该要素不占格、不产出。
- `style == nullptr` 或没有任何要素：帧仍带默认背景 `0xfff5f0e6`，`items` 为空。
- 镶嵌失败的几何跳过，不让整帧失败。
- heatmap / hillshade / fill-extrusion 忽略。

## Default style

`default_carto_style_json()` 内嵌一份 Style JSON，只用 background / fill / line / symbol，复现 land、water、river、road casing + fill、admin stroke、注记。`source-layer`：`land`、`water`、`river`、`admin`、`road`（casing 与 fill 以及沿线注记共用）、`label`（点注记）。路缘层 `road-casing` 在 `road` 之前。

## Style keys

`gis::style::ResolvedPaint` 增加 `symbol_placement`（默认 `"point"`）、`text_halo_color`、`text_halo_width`。`fill_resolved_paint` 只读这三个常量键。`style_test` 覆盖缺省与显式值。

## Tests

`//src/gis/map2d:map2d_test`（`expect` 名，不是 gtest）：

- painter order fill before line
- painter background is not a mesh
- overlapping labels drop the lower priority
- along-line angle
- circle has more than 4 vertices
- line width passed through
- raster quad in lon/lat
- default style JSON parses
- missing icon still places text
- missing metrics skips text
- empty frame keeps background

字形 advance 用固定比例 stub。不在 gis 里光栅化像素。

---

## §Views RHI present（merged 2026-09-28）

Views Map/Data content HWND：优先 FlyCube；失败或 env 强制则 GDI 全量。命令列表拼接 / 单次 present 归 RHI living §Frame graph；本节只锁 **Views 挂接与降级**。

| Locked | Choice |
| --- | --- |
| 主路径 | `present_gpu` / `map2d/map2d_presenter` → `map2d/frame` batches → `gis::vista::Layout` / `effect::map` → present；GDI 在 `map2d/paint/` |
| Chrome | 仍 `ui/gfx`；地图子 HWND 走 RHI |
| 默认 | FlyCube 优先；`SMT_PREFER_FLYCUBE_2D=0` / `SMT_FORCE_CONTENT_MAPVIEW_2D=1` / `SMT_FORCE_GDI_MAP_OVERLAY=1` 降级 |
| GDI | GPU 成功：注记/选中 overlay；否则全量 `paint` |
| Null | 测试：`create_device(kNull)` 稳定 true |
| 依赖 | Views **禁止** `#include "legacy/…"`；不改 SP2 `bind_rhi_present` 语义 |

Non-goals: Skia 不画 GIS 地图；不扶 GDI/GL 为第二 2D 引擎；本轮不做完整 GPU 注记避让。

### Atmosphere（out of scope for Map2d）

`effect::atmosphere`（ocean/cloud/sky/fog）只挂在 **3D Scene** `Scene3dPresenter` + `OrbitGeoFrame`。Map2d / Data 页不叠 3D 大气；见 [`2026-09-19-atmosphere-ocean-cloud-design.md`](2026-09-19-atmosphere-ocean-cloud-design.md) §China 3D geo-alignment。

---

## §Perf + MapLibre-style visual alignment（2026-09-28）

**Decision:** Approach 1 — 自研 Style Spec 语义对齐 + 交互双速；**不**重新引入 MapLibre Native。性能优先（Phase 1），视觉按 **线面 → Style 驱动 → 注记**（Phase 2）。

### Goals

| Phase | Goal |
| --- | --- |
| 1a | 确认默认走 FlyCube/`effect::map`；`last_gpu_present_ok` 为真时 GDI 仅 overlay（`SMT_FORCE_GDI_MAP_OVERLAY=1` 除外） |
| 1b | 交互（pan / 同 zoom-bucket 内微调）：复用 cached `MapFrame` 世界项 + 新 ortho camera，目标 ≥ ~30fps |
| 1c | 静止：同视口重复 present **跳过** `Layout::build`；内容/zoom-bucket/像素尺寸变化或交互刚停稳时完整 rebuild；默认预算 layout+present **&lt;50ms**（可测可报） |
| 2a | 线面质量：AA、线宽/dash、fill 半透明与画家顺序（对齐 MapLibre 观感，非 Native） |
| 2b | 样式驱动：Catalog「渲染参数」表降级为调试面；主路径 Style JSON / `ResolvedPaint`（zoom **常量**；`interpolate` 仍 non-goal） |
| 2c | 注记：halo、碰撞、沿线字、字体观感 |

### Architecture

```
MapScene + ViewFrame + StyleDocument
        |
        v
Map2dPresenter  (dual-speed)
   |-- interactive: cached MapFrame, world-only Pass, new camera
   |-- settled / dirty: Layout::build → cache → full Pass (world+overlay)
        |
        v
effect::map::Pass + render::graph::present
        |
        +-- fail / force → GDI paint（全量）；GPU ok → annotation/selection overlay
```

### Frame cache (Phase 1 locked)

| Event | Action |
| --- | --- |
| 无缓存 / content fingerprint 变 / zoom-bucket 变 / 视口像素尺寸变 | 完整 `Layout::build`，缓存 `MapFrame`，`record_all` present |
| 仅相机（extent / pan/scale）变，且 zoom-bucket 不变 | **interactive**：复用缓存，**仅世界项** present（跳过 stale 像素注记）；GDI overlay 可补注记 |
| 上一帧 interactive，本帧相机未变 | **settle**：完整 rebuild + full present（注记回 GPU） |
| 相机与内容均未变 | 复用缓存，full present，**不** rebuild |

Content fingerprint（最小集）：`feature_count` + `layer_count` + `style_document*` +「是否走默认 carto」。`invalidate_frame_cache()` 供文档变更显式失效。

### Non-goals（本 § 追加）

- 不重新链接 `maplibre-native` / 不建 `src/render/maplibre`。
- Phase 1 不做异步 worker 线程 layout（可后续加）；双速与缓存先同步落地。
- Phase 2 仍不做 glyphs PBF、heatmap、hillshade、fill-extrusion、`interpolate`（与上文 Non-goals 一致）。

### Acceptance

- Env 未强制 GDI 时，seed/默认数据路径 `present_gpu` 成功且 `last_gpu_present_ok`。
- 交互路径：连续 pan 时 `Layout::build` 次数远小于 present 次数（测试可断言 `layout_build_count`）。
- 静止重复 present：第二次起 `layout_build_count` 不增加。
- 视觉 Phase 2：对照同 Style JSON 的 MapLibre 参考图（线宽/色/注记），人工 diff；自动化保持现有 `map2d_test` / `map2d_presenter_test` 扩展。

## §Shell HUD-in-frame (`DrawRequest.shell`)

**Status:** active (in-process FlyCube path landed; PresentMailbox merge deferred)

Normative: final map compose stays on the GPU / `DrawRequest` path. Shell chrome CPU compositor stays separate (`ui::views` Widget / `ShellCompositor`). Do **not** GDI `overlay_paint_` after a successful DXGI present on the same map HWND.

### Pipeline (as-built)

```
Widget::shell_raster (full client BGRA)
        |
        v
BrowserView::commit_widget_shell_to_maps(dirty)
  - skip panes when dirty does not intersect pane HWND (widget space)
  - crop shell to pane HWND → MapViewport::commit_shell_overlay
        |
        v
MapViewport shell_bgra_ + shell_generation_
  - on actual change: request_frame / signal_display
  - chrome-only dirty already filtered by BrowserView
        |
        v
Display BeginFrame → GpuPresentFn
  - snapshot_shell_overlay → present_gpu(..., shell, generation)
        |
        v
Map2dPresenter / Scene3dPresenter
  - map / scene Effects, then ShellOverlayEffect (kOverlay, src-over)
  - generation skip reuses uploaded texture (same contract as
    gpu::attach_shell_raster / DrawRequest.shell_generation)
```

`gpu::draw_and_swap` / `PresentMailbox` already call `attach_shell_raster(pass, req.shell, req.shell_generation)`. FlyCube in-process present does **not** hard-link `//src/ui/views` → `//src/gpu` yet; it fills the same shell fields and composes via `ShellOverlayEffect` on the shared command list before `device->present()`.

### Remaining TODOs

- Wire MapViewport `GpuSubmitFn` → `gpu::PresentMailbox` once views may depend on `gpu_backend` (or keep the views sibling mailbox).
- Map annotation / Scene3d `paint_hud` GDI still skipped on successful FlyCube present (separate from shell chrome HUD).
- Punch alpha-0 holes for native `MapViewport` HWND rects in the shell raster (or clear those rects transparent before Commit) so Skia shells do not cover the map with opaque `shell_bg` under src-over; GDI shells often leave alpha 0 already.
- DWM / DXGI frame clock for BeginFrame (existing compositor TODO).
