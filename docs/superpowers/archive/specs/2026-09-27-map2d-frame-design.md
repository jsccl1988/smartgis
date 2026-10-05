<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

> **Status: superseded** (2026-09-28 merge B). Merged into [`../../specs/2026-09-13-render-rhi-scene-design.md`](../../specs/2026-09-13-render-rhi-scene-design.md) — §Map2d CPU frame + Views present (folded). Do not revise here except mechanical link fixes; revise the living umbrella in place.


# Views 2D MapFrame（CPU layout）

**Status:** superseded (2026-09-28 merge B)
**Date:** 2026-09-27  
**Updated:** 2026-09-28 — §MapFrame-unified software present (B/B2/C1/D2): shared `Map2dFrameCache`, GDI rasters `MapFrame`; heuristics stay in `content/.../map2d/frame`. Earlier: present composition under `gpu/`/`software/`; §Shell HUD-in-frame; §Perf dual-speed.  
**Note:** Product RHI Pass under **`src/vista/component/map`**. CPU layout under `src/vista/frame` (`gis::vista`). MapLibre Native pin removed (deferred) — [`../archive/specs/2026-09-27-maplibre-out-of-gpu-design.md`](2026-09-27-maplibre-out-of-gpu-design.md).  
**Related:** RHI living [`2026-09-13-render-rhi-scene-design.md`](../../specs/2026-09-13-render-rhi-scene-design.md)；desktop shell [`2026-09-27-views-desktop-shell-design.md`](../../specs/2026-09-27-views-desktop-shell-design.md)；archived RHI-present twin [`../archive/specs/2026-09-27-views-2d-map-rhi-design.md`](2026-09-27-views-2d-map-rhi-design.md)。  
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
effect::map::Pass      product RHI pass (src/vista/component/map; was render/map2d)
```

`gis` 不 `#include` `render` / `effect`，不链接 GDI DLL。字形像素光栅化属于 effect map pass（Windows 光栅器）。本模块只消费注入的 `GlyphMetrics`。

## Non-goals

- 不引入 glyphs PBF、sprite atlas JSON、heatmap、hillshade、fill-extrusion、`interpolate`。
- 不把调色板常量写进 GPU pass。颜色只活在 Style JSON / `ResolvedPaint`。
- 不改 `src/gpu/paint`（那是 GPU 进程的 preview / basemap 入口，不是这块 CPU 布局）。
- 不把逻辑写进 `GpuScene::rebuild_meshes`。Views `Map2dPresenter` 帧缓存与 Pass 视觉增强见 §Perf（本文件）。

## Locked API

公共头（as-built）：`src/vista/frame/frame.h`。命名空间 `gis::vista`（历史文稿曾写 `gis::map2d`）。辅助在 `gis::vista::detail`。

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
| 主路径 | `present_gpu` / `map2d/map2d_presenter` → `Map2dGpuPresent` + `map2d/frame` batches → `gis::vista::Layout` / `effect::map` → present；GDI 在 `map2d/software/`（`Map2dSoftwarePainter`） |
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

Normative: final map compose stays on the GPU / `DrawRequest` path. Shell chrome CPU compositor stays separate (`ui::views` Widget / `ShellCompositor`). Do **not** GDI `overlay_paint_` after a successful DXGI present on the same map HWND. Same rule for **ContentMapView**: after a successful `SharedSurface` blit (`last_content_present_ok`), overlay is annotation/flash only — never a second full `Map2dPresenter::paint` (that path was multi-second layout+GDI on seed maps).

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

---

## §MapFrame-unified software present（2026-09-28）

**Status:** active  
**Locked choices:** B (software 消费 `MapFrame`) · B2 (保 content 启发式) · C1 (共享 FrameCache) · D2 (启发式留 `content/.../map2d/frame`) · 落地方式 **一次替换**（无手写 GDI 要素循环）。

### Goal

GPU 与 GDI 全量 paint 共用同一 `Layout::build` 产物；删除 `map2d_software_paint.cc` 内平行的 Baidu-like 要素绘制循环。

### Architecture

```
MapScene + ViewFrame + StyleDocument
        |
        v
Map2dFrameCache     fingerprint + dual-speed (§Perf); rebuild → enrich batches (D2) → Layout
        |
        +-- Map2dGpuPresent     Pass + shell overlay
        +-- Map2dSoftwarePainter
              basemap / MapFrameGdiRasterizer / selection+flash overlays
```

| Path | Role |
| --- | --- |
| `map2d/frame/map2d_frame_cache.*` | Shared cache; `prepare_for_present` (GPU + GDI dual-speed); `ensure_full` for settled-frame callers |
| `map2d/frame/map2d_batches.*` + `map2d_carto.*` | D2: scale/importance/line visibility filter + attrs before Layout |
| `map2d/software/map2d_frame_gdi.*` | Thin GDI raster of `DrawItem`s |
| `map2d/software/map2d_software_paint.*` | Orchestration + basemap + overlays + `paint_labels_projected` (Scene3d) + BMP |
| `map2d/gpu/map2d_gpu_present.*` | Pass + shell only; no owned MapFrame |

### Locked API / behavior

- `Map2dPresenter` public methods unchanged.
- `gis::vista::Layout` / `MapFrame` unchanged (no D1 push).
- World→screen: `sx=(x-min_x)/(max_x-min_x)*w`, `sy=(max_y-y)/(max_y-min_y)*h` (same as vista).
- Background from `MapFrame.background_rgba` (not hard-coded ocean) when `fill_background`.
- Selection / flash / basemap underlay remain Scene-driven GDI (not MapFrame).
- `paint_labels_projected` stays Scene+project callback for Scene3d HUD.

### Non-goals

- Env dual-track old paint.
- Pushing product heuristics into `gis::vista`.
- Full stem-network filter parity in v1 (importance + line scale gate first; stem follow-up if e2e regresses).
- Changing `effect::map::Pass` contract.

### Instrumentation + RenderTrace（2026-09-28）

**Status:** active — pairs with `base/trace` (§Trace in base-root-hybrid) and shell §RenderTrace.

When `base::tracing_enabled()` (env `SMT_TRACE=1` or UI Record):

| Span name | Category | Site |
| --- | --- | --- |
| `presenter` | `map2d` | `Map2dPresenter::present_gpu` |
| `presenter_paint` | `map2d` | `Map2dPresenter::paint` |
| `layout` | `map2d.layout` | `Map2dFrameCache::rebuild_layout` (nested: `batches`, `build`, `emit_fill`/`emit_line`/`emit_symbol`/`emit_circle`/`emit_raster`) |
| `present` / `present_frame` | `map2d.present` | `Map2dGpuPresent` |
| `upload` | `map2d.upload` | `effect::map::Pass::record` |
| `gdi` | `map2d.gdi` | `Map2dSoftwarePainter::paint` |
| `presenter` | `scene3d` | `Scene3dPresenter::present_gpu` |
| `presenter_paint` / `hud` | `scene3d` | `Scene3dPresenter::paint` / `paint_hud` |
| `present` | `scene3d.present` | `Scene3dGpuPresent::present` |
| `mesh` | `scene3d.mesh` | `Scene3dGpuPresent::rebuild_local_mesh` |
| `atmosphere` | `scene3d.atmosphere` | `AtmosphereSession::prepare_for_present` |
| `gdi` | `scene3d.gdi` | `Scene3dSoftwarePainter::paint` |

Panel: Inspector `RenderTrace` with Map2d / Scene3d view filters (export is full buffer).

Instant/counter optional via `SpanRecorder` for draw-item counts. Export JSON opens in Perfetto / chrome://tracing. Phase 1c budget still &lt;50ms layout+present when settled.

### Acceptance

- `map2d_presenter_test` green (GPU Null + paint/BMP + dual-speed counters).
- Default seed path: `present_gpu` ok; GDI `paint` draws from shared cache (`layout_build_count` shared).
- No hand-written polygon/line feature loop in software paint TU.
- `trace_test` green; with `SMT_TRACE=1` pan produces `map2d.*` events in dump.
