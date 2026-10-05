<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/render`（终局）

SmartGIS 渲染终局树：只保留 RHI / 帧图。`WorldPass` 落在 [`src/vista/pass/world`](../vista/pass/world/)（`vista::WorldPass`，**vista.dll**，不进 `render.dll`）。场景数学在 `src/base/math`（命名空间仍为 `render`）。壳画布在 `src/ui/gfx`。GPU 地图 pass 在 `src/vista/pass/map`，大气 pass 在 `src/vista/component/atmosphere`。2010 leftover 设备与三维引擎（代号 **Scenic**）在 [`src/legacy/render/`](../legacy/render/)（磁盘不改 `src/scenic`）。

## 目录

| 路径 | 角色 |
| --- | --- |
| `rhi/` | 统一 2D+3D Facade（公开 `rhi.h` / `rhi.cc`）；零 FlyCube 类型进公开头；含 compute（ocean GPU FFT） |
| `rhi/stub/` | `StubDevice(Backend)`：合并 Null / GDI / GL leftover HWND 适配（非真光栅） |
| `rhi/flycube/` | 唯一真 GPU。`device.h`、`command/command_list.h`、`resource/resources.h` 是 RHI 适配。`command/recorder.h`、`resource/gpu.h`、`pipeline/`、`compute/` 不包含 `rhi.h`。子目录文件不带 `flycube_` 前缀 |
| `graph/` | `present` + Effect 槽调度。一帧一个相机、一条命令列表、一次 present。只按 `Effect` 槽走：`kBeforeOpaque`、`kOpaque`、`kAfterOpaque`、`kOverlay`。不点名 `WorldPass`、`MapIR`、`AtmosphereFrame`。opaque 适配器（`OpaqueEffect`）与 `WorldPass` 在 **`vista/pass/world`** |
| `testing/` | 业内对照场景库 + `rhi_suite_test`（Null）+ `rhi_bench`（google/benchmark，进 `benchmark_all`）+ `rhi_gpu_bench`（可选 DX12）。Console：`:rhi test\|bench` |

GN：`//src/render:render_all` 进日常 `src_all`。leftover DLL 另编 `//src/legacy/render:legacy_render_all`（默认不进 `src_all`）。`WorldPass` / `scene_gpu_test` → `//src/vista:vista` / `//src/vista:vista_test_all`。

## 依赖方向

- 新代码 / `src_all` → `render::rhi`；GPU 场景缓存 → `vista::WorldPass`（`vista/pass/world`）；场景数学 → `base/math`（命名空间 `render`）
- 壳画布 → `src/ui/gfx`（仅经 `//:ui_views`；默认 GDI；真 Skia 见 `src/ui/gfx/README.md`）
- `legacy_render` → `render/rhi` / `base/math` 允许（单向）
- `render` 终局 → `legacy_render` **禁止**

设计：[`docs/superpowers/specs/2026-09-13-render-legacy-split-design.md`](../../docs/superpowers/specs/2026-09-13-render-legacy-split-design.md)、[`docs/superpowers/src-layout.md`](../../docs/superpowers/src-layout.md)。

## As-built（Views 2D 主路径 = RHI，2026-09-27）

- **Views 主像素路径**：地图是 `Layout` → 一个 `record_all` 的 `MapEffect` → `render::graph::present`。`Scene3dPresenter::present_gpu` 走同一个 `present`，效果顺序是大气前段、`OpaqueEffect`、大气后段。录制顺序只有四个槽：`kBeforeOpaque`、`kOpaque`（地形 / 模型 / 地图世界网格）、`kAfterOpaque`、`kOverlay`（屏幕图标和文字）。`ViewInput` 只有宽高、相机和 `Effect*` 列表。地图相机是视口经纬度的正交；三维在非 Null 后端传入透视相机。`View::mode == kPerspective` 时由宿主提供相机。壳栅格仍用 `ui/gfx`；**禁止** Skia 画 GIS 地图。
- GDI `MapScene::paint` 为过渡/导出/强制 overlay；GPU 成功后只叠 `paint_annotation_overlay`（注记/选中）。`FORCE_GDI_MAP_OVERLAY=1` / `PREFER_FLYCUBE_2D=0` 可退回。
- Views 主路径不经 leftover RHI session；`rhi3d/public/bridge`（`LeftoverRecorder` / `bind_rhi_present` / `leftover_session`）已删除。
- Views `MapViewport`：Map Edit/Data/Scene3d 默认优先 FlyCube；`PREFER_GDI_DEVICE=1` 跳过。
- 投影只在相机上：地图 `make_ortho_camera`；三维 `set_view_camera`。`set_view_ortho` 只在没设视图相机时给旧的二维种类用。
- 测试：`map_scene_test` 覆盖 Null `present_gpu`；`rhi_test` / `scene_gpu_test` 默认 Null，`RUN_FLYCUBE_GPU=1` 真 DX12。
- 海洋 GPU FFT：FlyCube `supports_compute()` 时 compute；否则 CPU。设计：[`docs/superpowers/specs/2026-09-27-views-2d-map-rhi-design.md`](../../docs/superpowers/specs/2026-09-27-views-2d-map-rhi-design.md)。
- **MapLibre 式 2D 帧**：`vista::MapPass::record` 清 `background_rgba`（`0xAARRGGBB` × opacity，与 `ResolvedPaint` 相同），按 `items` 顺序画到与 `WorldPass::set_view_ortho` 相同的经纬度 ortho。`pixel_space` 的 icon/text 先绕 `anchor_x/anchor_y` 转 `angle_rad`，再从视图像素（y 向下）换进该 ortho。文字经 `GlyphRasterizer` 打进一张图集再画四边形；`halo_width_px > 0` 时先画一圈更大的 `halo_rgba` 实色四边形。栅格/图标由调用方 `load_raster` / `load_icon` 提供 RGBA8，失败则跳过该项。Windows 字形是 `WindowsGlyphRasterizer`（GDI+，无窗口）。Null `create_device(kNull)` 可录空帧和填充三角形。测试 `//src/vista/pass/map:map_effect_test`。

## Parallel / GPU (as-built gates)

Product-track parallel + GPU accelerate（L0 UI never joins · vista `parallel_for` · Display 单 CL）locked in living **§src_render + vista parallel accelerate**:

- Spec: [`docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md`](../../docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md)（Types / Call graph / Grains / Env / API sketches）
- Diagram: [`docs/superpowers/diagrams/render-accelerate-topology.html`](../../docs/superpowers/diagrams/render-accelerate-topology.html)（A×B 整合 · Topology A §2–§3）
- Plan: [`docs/superpowers/plans/2026-10-02-src-render-vista-parallel-accelerate.md`](../../docs/superpowers/plans/2026-10-02-src-render-vista-parallel-accelerate.md)
- WorldPass prep env: [`src/vista/pass/world/README.md`](../vista/pass/world/README.md)

Product env:

| Env | Default | Effect |
| --- | --- | --- |
| `VISTA_LAYOUT_PARALLEL=0` | (layout wiring) | layout tess N=1 |
| `SCENE3D_FRUSTUM_CULL` | **off** | AABB frustum cull for 3D meshes |
| `GPUSCENE_PREP_PARALLEL` | **off** (unset/`0`) | prep workers only when **also** `SCENE3D_FRUSTUM_CULL=1`; else serial / no-op |

`prep_par_on` matrix rows that set prep=1 without cull stay safe serial (no expensive wrong parallel work). Leftover `RHI2D_*` / `RHI3D_*` must **not** drive this path. Warm Map2d presents still skip re-record via `Map2dFrameCache` StaticReuse.

## Optional FlyCube GPU smoke

CI / default builds stay on Null. To exercise real DX12 present + lit solid (`kLitSolid` / `set_light_params`; skip-not-red if no adapter):

```bat
set RUN_FLYCUBE_GPU=1
ninja -C out rhi_test
ninja -C out scene_gpu_test
```

Optional: `ninja -C out unified_draw_test`. GN labels: `//src/render:rhi_test`, `//src/vista/pass/world:scene_gpu_test`, `//src/vista/pass/world:unified_draw_test`. `rhi_test` with env=1 logs `dx12 present ok` and `lit ok` on success, or `skip present` when initialize fails.
