# `src/render`（终局）

SmartGIS 渲染终局树：只保留 RHI / GpuScene / 帧图。场景数学在 `src/base/math`（命名空间仍为 `render`）。壳画布在 `src/ui/gfx`。GPU 地图与大气 pass 在 `src/effect/map` 与 `src/effect/atmosphere`，经 `source_set` 链进调用方，不进 `render.dll`。2010 leftover 设备与三维引擎已迁到 [`src/legacy/render/`](../legacy/render/)。

## 目录

| 路径 | 角色 |
| --- | --- |
| `rhi/` | 统一 2D+3D Facade（公开 `rhi.h` / `rhi.cc`）；零 FlyCube 类型进公开头；含 compute（ocean GPU FFT） |
| `rhi/stub/` | `StubDevice(Backend)`：合并 Null / GDI / GL leftover HWND 适配（非真光栅） |
| `rhi/flycube/` | 唯一真 GPU。`device.h`、`command/command_list.h`、`resource/resources.h` 是 RHI 适配。`command/recorder.h`、`resource/gpu.h`、`pipeline/`、`compute/` 不包含 `rhi.h`。子目录文件不带 `flycube_` 前缀 |
| `scene/` | GPU 缓存：`render::scene::GpuScene`。网格、细分和 `frustum_aabb` 留在这里 |
| `graph/` | `present` 加 opaque 适配器（`OpaqueEffect`，槽 `kOpaque`）。一帧一个相机、一条命令列表、一次 present。只按 `Effect` 槽走：`kBeforeOpaque`、`kOpaque`、`kAfterOpaque`、`kOverlay`。不点名 `GpuScene`、`MapFrame`、`AtmosphereFrame` |
| `testing/` | 业内对照场景库 + `rhi_suite_test`（Null）+ `rhi_bench`（google/benchmark，进 `benchmark_all`）+ `rhi_gpu_bench`（可选 DX12）。Console：`:rhi test\|bench` |

GN：`//src/render:render_all` 进日常 `src_all`。leftover DLL 另编 `//src/legacy/render:legacy_render_all`（默认不进 `src_all`）。

## 依赖方向

- 新代码 / `src_all` → `render::rhi`、`GpuScene`；场景数学 → `base/math`（命名空间 `render`）
- 壳画布 → `src/ui/gfx`（仅经 `//:ui_views`；默认 GDI；真 Skia 见 `src/ui/gfx/README.md`）
- `legacy_render` → `render/rhi` / `base/math` 允许（单向）
- `render` 终局 → `legacy_render` **禁止**

设计：[`docs/superpowers/specs/2026-09-13-render-legacy-split-design.md`](../../docs/superpowers/specs/2026-09-13-render-legacy-split-design.md)、[`docs/build/src-layout.md`](../../docs/build/src-layout.md)。

## As-built（Views 2D 主路径 = RHI，2026-09-27）

- **Views 主像素路径**：地图是 `Layout` → 一个 `record_all` 的 `MapEffect` → `render::graph::present`。`Scene3dPresenter::present_gpu` 走同一个 `present`，效果顺序是大气前段、`OpaqueEffect`、大气后段。录制顺序只有四个槽：`kBeforeOpaque`、`kOpaque`（地形 / 模型 / 地图世界网格）、`kAfterOpaque`、`kOverlay`（屏幕图标和文字）。`ViewInput` 只有宽高、相机和 `Effect*` 列表。地图相机是视口经纬度的正交；三维在非 Null 后端传入透视相机。`View::mode == kPerspective` 时由宿主提供相机。壳栅格仍用 `ui/gfx`；**禁止** Skia 画 GIS 地图。
- GDI `MapScene::paint` 为过渡/导出/强制 overlay；GPU 成功后只叠 `paint_annotation_overlay`（注记/选中）。`SMT_FORCE_GDI_MAP_OVERLAY=1` / `SMT_PREFER_FLYCUBE_2D=0` 可退回。
- 主图 leftover 会话：`LeftoverRecorder` 默认 Null 录制；`bind_rhi_present` 只记 HWND（不在 GDI HWND 上建 FlyCube）。
- Views `MapViewport`：Map Edit/Data/Scene3d 默认优先 FlyCube；`SMT_PREFER_GDI_DEVICE=1` 跳过。
- 投影只在相机上：地图 `make_ortho_camera`；三维 `set_view_camera`。`set_view_ortho` 只在没设视图相机时给旧的二维种类用。
- 测试：`map_scene_test` 覆盖 Null `present_gpu`；`rhi_test` / `scene_gpu_test` 默认 Null，`SMT_RUN_FLYCUBE_GPU=1` 真 DX12。
- 海洋 GPU FFT：FlyCube `supports_compute()` 时 compute；否则 CPU。设计：[`docs/superpowers/specs/2026-09-27-views-2d-map-rhi-design.md`](../../docs/superpowers/specs/2026-09-27-views-2d-map-rhi-design.md)。
- **MapLibre 式 2D 帧**：`effect::map::Pass::record` 清 `background_rgba`（`0xAARRGGBB` × opacity，与 `ResolvedPaint` 相同），按 `items` 顺序画到与 `GpuScene::set_view_ortho` 相同的经纬度 ortho。`pixel_space` 的 icon/text 先绕 `anchor_x/anchor_y` 转 `angle_rad`，再从视图像素（y 向下）换进该 ortho。文字经 `GlyphRasterizer` 打进一张图集再画四边形；`halo_width_px > 0` 时先画一圈更大的 `halo_rgba` 实色四边形。栅格/图标由调用方 `load_raster` / `load_icon` 提供 RGBA8，失败则跳过该项。Windows 字形是 `WindowsGlyphRasterizer`（GDI+，无窗口）。Null `create_device(kNull)` 可录空帧和填充三角形。测试 `//src/effect/map:map_effect_test`。

## Optional FlyCube GPU smoke

CI / default builds stay on Null. To exercise real DX12 present + lit solid (`kLitSolid` / `set_light_params`; skip-not-red if no adapter):

```bat
set SMT_RUN_FLYCUBE_GPU=1
ninja -C out rhi_test
ninja -C out scene_gpu_test
```

Optional: `ninja -C out unified_draw_test`. GN labels: `//src/render:rhi_test`, `//src/render/scene:scene_gpu_test`, `//src/render/scene:unified_draw_test`. Style paint regression (Null): `ninja -C out leftover_record_test` (`//src/legacy/render/rhi3d:leftover_record_test`). `rhi_test` with env=1 logs `dx12 present ok` and `lit ok` on success, or `skip present` when initialize fails.
