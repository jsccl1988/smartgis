# `src/render`（终局）

SmartGIS 渲染终局树：只保留 RHI / GpuScene / Skia / 场景数学。2010 leftover 设备与三维引擎已迁到 [`src/legacy_render/`](../legacy_render/)。

## 目录

| 路径 | 角色 |
| --- | --- |
| `rhi/` | 统一 2D+3D Facade（FlyCube DX12/Vulkan）；公开头零 FlyCube 类型 |
| `scene/` | `render::scene::GpuScene`（GPU 实例 / 录制）；不含 `leftover_*` |
| `skia/` | 桌面壳画布（默认 GDI stub；`smt_has_skia` 可选真 Skia）；**不是** GIS GPU；**不**进 `render_all` / `src_all` |
| `math/` | 场景数学（Eigen POD 适配：`math.h` 聚合；禁 glm；可选 AVX2 SIMD） |

GN：`//src/render:render_all` 进日常 `src_all`。leftover DLL 另编 `//src/legacy_render:legacy_render_all`（默认不进 `src_all`）。

## 依赖方向

- 新代码 / `src_all` → `render::rhi`、`GpuScene`、`render/math`
- 壳画布 → `render/skia`（仅经 `//:ui_views`；默认 GDI；真 Skia 见 `skia/README.md`）
- `legacy_render` → `render/rhi` / `render/math` 允许（单向）
- `render` 终局 → `legacy_render` **禁止**

设计：[`docs/superpowers/specs/2026-09-13-render-legacy-split-design.md`](../../docs/superpowers/specs/2026-09-13-render-legacy-split-design.md)、[`docs/build/src-layout.md`](../../docs/build/src-layout.md)。

## As-built（2026-09-13 GIS loop）

- 主图 leftover 会话：`LeftoverRecorder` 优先 `preferred_gpu_backend()`（Win = DX12），失败回退 null；GDI 传入 HWND。
- `bind_rhi_present`：优先 FlyCube，失败回退 GDI。
- Views `MapViewport`：`try_flycube_device` 在 LoadLibrary GDI 之前；`SMT_PREFER_GDI_DEVICE=1` 可强制 leftover DLL。
- 2D：`set_solid_color` / 地图默认 brush cyan；`record_map` 可从首个 `MapLayer::style_name` 取 brush；`set_view_ortho` + envelope。Per-layer/per-feature style 仍 TODO。
- 3D：`ModelAsset` → GpuScene 上传；tileset `visible_uris` → `decode_content_file` → mesh，失败回退 AABB。
- 测试：`rhi_test` / `unified_draw_test` 默认跳过 FlyCube HWND/init（`SMT_RUN_FLYCUBE_GPU=1`）；`NullDevice` 的 destroy 故意泄漏 stub（避免 FlyCube 链接下 `operator delete` 挂死）；`scene_gpu_test` / `leftover_record_test` 仅走 Null。ColorCB BindingSet 缓存。
