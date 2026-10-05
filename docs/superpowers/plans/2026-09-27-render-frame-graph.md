<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Render Frame Graph — Implementation Plan


> **Design living:** [../specs/2026-09-13-render-rhi-scene-design.md](../specs/2026-09-13-render-rhi-scene-design.md) §Frame graph. This file is the checklist only.

**Status:** active  
**Spec:** [`../specs/2026-09-27-render-frame-graph-design.md`](../archive/specs/2026-09-27-render-frame-graph-design.md)

人跑编译。地图是正交相机下的同一套网格，不是第二条渲染器。每步结束后在仓库根执行下面列出的 `build.bat` 目标。不要新开分支。

当前 `present` 合同在第 8 节：一条按槽排序的 `Effect` 列表。第 1–7 节里写过的 `ViewInput::scene` / `map` 字段已经删除。

## 1. Frame Graph 录制（行为与今天两条 present 相同）

- [x] 新增 `src/render/graph/frame_graph.h` / `frame_graph.cc`：`render::graph::ViewInput`、`render::graph::present`，签名以规格为准。
- [x] `present` 创建一条 `CommandList`，按槽调用 `Effect::record`。任一步失败则 `destroy_command_list` 并返回 false。成功则 `close`、`execute`、`destroy_command_list`、`present`。
- [x] `effects` 为空：仍 Clear 一次并 present。宽或高为 0、或 `device == nullptr`：返回 false，不建列表。
- [x] `//src/render/graph:graph_sources` 编进 `//src/render:render` 的 `deps`。`graph_sources` 只依赖 `rhi_sources`。`scene_sources`、`map2d_sources`、大气效果反过来依赖 `graph_sources`，避免 GN 环。
- [x] `frame_graph_test`（Null `Device`）：空效果、四槽、`MapEffect` `record_all`、空 `GpuScene` 的 `OpaqueEffect`。见第 8 节。
- [ ] 人跑：`build.bat frame_graph_test`

## 2. 三维改走 present

- [x] `Scene3dController::present_gpu` 在 `prepare_atmosphere_*` 之后组 `AtmosphereEffects`（云质量在后段效果上），按顺序推入前段效果、`OpaqueEffect(&gpu_scene_)`、后段效果，调用 `render::graph::present`。`kNull` 不传相机指针。本函数不再 `create_command_list` / `record_*` / `execute` / `present`。
- [x] Load/Clear 由 Frame Graph 写进 `RecordContext`。`OpaqueEffect` 再设到 `GpuScene`。`clears_color()` 为 false，所以 `kLoad` 时不透明 pass 不清颜色。
- [ ] 人跑：`build.bat scene3d_controller_test` 与 `build.bat atmosphere_frame_test`

## 3. 二维改走 present

- [x] `MapScene::present_gpu` 仍先 `Layout::build`，然后 `ViewInput` 只填宽高、视图范围的正交 `camera`，以及一个 `record_all` 的 `MapEffect`。没有 `scene` 指针。
- [ ] 人跑：`build.bat map_scene_test` 与 `build.bat map2d_pass_test`

## 4. 地图世界网格与屏幕注记分开录

- [x] `map2d::Pass::record` 增加 `world_items`、`overlay_items`、`color_op`，默认全画且 Clear。`map2d_pass_test` 仍用默认值。
- [x] `MapEffect` 在 `kOpaque` 时只画世界网格，在 `kOverlay` 时只画图标和文字，颜色 Load 用 `RecordContext::color_op`。`record_all` 时两个开关都为 true。
- [x] `frame_graph_test`：`MapEffect` `record_all` 加正交相机，`present` 返回 true。
- [ ] 人跑：`build.bat frame_graph_test` 与 `build.bat map2d_pass_test`

## 5. 文档与词

- [x] `src/render/README.md`：二维主路径写成 `Layout` → `render::graph::present` → `map2d::Pass`。`present` 按 Effect 槽描述，大气不是环境类型。`GpuScene::record_draws` 写成不透明 pass。
- [x] `GpuScene::set_view_ortho` 注释改为正交相机绑定，供不透明 pass 使用。
- [ ] 人跑：`build.bat frame_graph_test`

## 6. GIS 目录缝（present / map2d / scene）

人跑编译。这一节只钉 GIS 侧职责，不实现其余领域，也不为每个领域新开 render 目录。`present` 仍是样式和瓦片；CPU 帧是同一份 `MapFrame`（正交现在，透视以后）；`World` 加 `DomainSession`，大气只是 `kAtmosphere`。目录终态见第 11 节（11a 已并进 `vista`；11b 再收成 `world` + `domain/atmosphere`）；本节不搬目录。

- [x] `src/gis/scene/domain/domain.h`：`gis::DomainKind` 与 `gis::DomainSession`（头文件，虚析构默认在头里）。`//src/gis:gis` 依赖 `//src/gis/scene/domain:domain_sources`。不实现 factory / geology / offshore / storm-surge / space，不搬大气代码。
- [x] `gis::map2d::ViewMode`（`kOrtho` 默认、`kPerspective`）挂在 `View` 上。`frame.h` 写明网格在视图 CRS，正交是 map2d，透视或球体相机由宿主交给同一 `Layout`。不改 `DrawKind`，不加 RHI / HWND / `CameraMatrices`，不做球体或 ECEF。
- [x] `Environment` 用一行注释标明自己是 `kAtmosphere` 会话，并提供 `kind()`。不继承 `DomainSession`，以免 `World` 的包含被带进来。
- [x] `present` 的 README 写明：产出 `StyleDocument`、符号和瓦片；`map2d::Layout` 消费；`World` 与 `DomainSession` 不解析 Style JSON。不搬 `style` / `tile`。
- [x] `render::graph::present` 仍是一个相机、一条 `Effect` 列表。`AtmosphereFrame` 只被前段和后段包装。factory / geology / offshore / storm-surge / space 是以后的 `Effect` 实现，不是这次的代码。
- [ ] 人跑：`build.bat map2d_test`

## 7. Effect 槽（大气不是唯一环境）

- [x] `render::graph::Effect` / `EffectSlot`。`ViewInput` 去掉 `AtmosphereFrame*`，改为非拥有 `std::vector<Effect*>`。
- [x] `src/render/atmosphere/frame/atmosphere_effects.h/.cc`：`PreOpaqueEffect`、`PostOpaqueEffect`、`AtmosphereEffects`。`clears_color` / `uses_shared_depth` 转给 `AtmosphereFrame`。编进 `atmosphere_sources`，该目标依赖 `graph_sources`。
- [x] `frame_graph_test` 覆盖空效果、null device、零宽，以及四槽各录一次。见第 8 节。
- [x] `Pass` 注释：`kPerspective` 不在 gis / `Pass` 里发明透视矩阵；用宿主传入的相机。
- [ ] 人跑：`build.bat frame_graph_test`、`build.bat map2d_pass_test`、`build.bat scene3d_controller_test`、`build.bat map_scene_test`

## 8. present 只走一条 Effect 列表

`frame_graph.cc` 不包含 `scene.h`、`map2d/pass.h` 或大气头。`GpuScene` 与地图知识在各自 pass 旁边的 adapter 里。

- [x] `EffectSlot` 四个值，录制顺序：`kBeforeOpaque`、`kOpaque`、`kAfterOpaque`、`kOverlay`。`kOpaque` 是地形、模型、地图世界网格。`kOverlay` 是屏幕图标和文字。
- [x] `RecordContext`（device、命令列表、宽高、相机、`color_op`、`shared_depth`）。`Effect::record` 只收这个上下文。大气 `PreOpaqueEffect` / `PostOpaqueEffect` 与测试里的 `CountingEffect` 已改签名。
- [x] `color_op` 从 `kClear` 开始；任一 `clears_color()` 为 true 的效果之后改为 `kLoad`。任一 `uses_shared_depth()` 之后 `shared_depth` 为 true。效果不 `execute` / `present`。`present` 关闭一次命令列表。
- [x] `effects` 为空时仍 Clear 一次。null device 或零宽返回 false。
- [x] `OpaqueEffect` 在 `src/render/scene/opaque_effect.h`（`render::scene`，槽 `kOpaque`）。`MapEffect` 在 `src/render/map2d/map_effect.h`（`render::map2d`）。两者编进 `scene_sources` / `map2d_sources`，并依赖 `graph_sources`。`graph_sources` 不再编译 `GpuScene` 或 map2d 的 cpp。
- [x] `MapScene::present_gpu` 只放相机、尺寸和一个 `record_all` 的 `MapEffect`。`Scene3dController::present_gpu` 推前段效果、`OpaqueEffect`、后段效果。`kNull` 相机仍为空。
- [x] `ViewInput` 只剩 `width_px`、`height_px`、`camera`、`effects`。
- [x] `frame_graph_test`：空效果为 true；null device 为 false；零宽为 false；四槽各调用一次，且前段 `clears_color()` 为 true 时后面的效果观察到 `ColorLoadOp::kLoad`；`MapEffect` `record_all` 加正交相机为 true；空 `GpuScene` 的 `OpaqueEffect` 为 true。
- [ ] 人跑：`build.bat frame_graph_test`、`build.bat map2d_pass_test`、`build.bat scene3d_controller_test`、`build.bat map_scene_test`

## 9. Move map and atmosphere GPU passes to src/effect

人跑编译。不要新开分支。不要在 `src/render/` 留转发头。第 1 节把 `map2d_sources` / `atmosphere_sources` 编进 `render.dll`；本节把它们移出。行为不变：`present` 仍只走 `Effect*`。`src/render/graph/frame_graph.h` 与 `frame_graph.cc` 继续只包含 `render/render_export.h` 和 `render/rhi/rhi.h`。

CPU 不动：`src/gis/map2d`（`MapFrame` / `ViewMode`）、`src/gis/scene/atmosphere`（`Environment`，`kAtmosphere`）、`src/gis/scene/domain`。不实现 factory / geology / offshore / storm-surge / space。本节不搬 `OpaqueEffect`，也不把它放进 `src/vista`。它的终态在第 10 节。CPU 目录终态是 `src/gis/vista`，见第 11 节，不要在本节搬。

嵌套停在 `src/<layer>/<module>`。新层是 `effect`，模块是 `map` 与 `atmosphere`。`ocean/`、`detail/`、`frame/` 留在模块内部。不要做成 `src/render/effect/...`（那是第三层公开目录），也不要放进 `gis`（这些 pass 包含 `render/rhi` 与 `render/graph`，而 `gis` 不包含 `render`）。

### 链接（先读再写 BUILD.gn）

今天 `map2d_sources` / `atmosphere_sources` 在 DLL **里面**，所以它们 `deps` `//src/render:rhi_sources` 和 `//src/render/graph:graph_sources`，并且不能 `deps` `//src/render:render`（那是环：DLL 依赖它们，它们再依赖 DLL）。

搬出之后环的方向反过来：`//src/render:render` **先删掉**对这两个 `source_set` 的 `deps`，然后产品 `source_set` 只依赖 DLL 的导入库。不要再 `deps` `rhi_sources` 或 `graph_sources`。那两个目标的 `.obj` 已经在 `render.dll` 里；Views 又链 `:rhi` / `:scene` / `:graph`（都转到 `:render`）时再链一份会重复 `Effect`、`present` 和整个 RHI。

产品 `source_set` **不要**定义 `RENDER_EXPORTS`。从头里去掉 `RENDER_EXPORT` 和 `#include "render/render_export.h"`。`Effect` 仍由 `frame_graph.h` 从 DLL 导出；`MapEffect` / `PreOpaqueEffect` / `PostOpaqueEffect` 是调用方二进制里的普通类。

不加载 `render.dll` 的测试保持今天的形状：自己列出 `.cc`，`deps = [ "//src/render:rhi_sources" ]`，`defines = [ "RENDER_EXPORTS" ]`（让 `rhi.h` 的导出宏和静态 `rhi_sources` 一致），不 `deps` `:render`。这是为了避开 DLL 文件锁（现注释里的 LNK1168），不是把 pass 再编进 DLL。

- [x] 新建 `src/vista/component/map/BUILD.gn`。`source_set("map_sources")` 的 `sources` 照搬今天 `src/render/map2d/BUILD.gn` 的 `map2d_sources`（`pass`、`map_effect`、`glyph_windows`、`detail/*`）。`include_dirs += [ "//src" ]`。`libs = [ "gdiplus.lib" ]`。`deps = [ "//src/render:render" ]`。没有 `RENDER_EXPORTS`。不要 `group` 转发到 `:render`。
- [x] 新建 `src/vista/component/atmosphere/BUILD.gn`。`source_set("atmosphere_sources")` 的 `sources` 照搬今天 `atmosphere_sources`（ocean / cloud / sky / fog / common / frame，含 `atmosphere_effects`）。`include_dirs += [ "//src" ]`。`deps = [ "//src/render:render" ]`。没有 `RENDER_EXPORTS`。不要 `group` 转发到 `:render`。
- [x] `git mv src/render/map2d` 下的源文件到 `src/vista/component/map/`（保持 `detail/`）。`git mv src/render/atmosphere` 下的源文件到 `src/vista/component/atmosphere/`（保持 `ocean/` `cloud/` `sky/` `fog/` `common/` `frame/` `detail/`）。删掉两个旧 `BUILD.gn` 和空目录。`src/render/map2d` 与 `src/render/atmosphere` 不再存在。
- [x] 命名空间：`render::map2d` → `effect::map`，`render::atmosphere` → `effect::atmosphere`。`detail` 仍是第三段。只替换 `render::map2d` / `render::atmosphere`，不要动 `gis::map2d` / `gis::atmosphere`。`gis::map2d` 改成 `gis::vista` 是第 11 节。
- [x] Include：`"render/map2d/` → `"vista/component/map/`，`"render/atmosphere/` → `"vista/component/atmosphere/`。Include guard 前缀 `RENDER_MAP2D_` → `EFFECT_MAP_`，`RENDER_ATMOSPHERE_` → `EFFECT_ATMOSPHERE_`。
- [x] 从 `//src/render:render` 的 `deps` 删除 `//src/render/atmosphere:atmosphere_sources` 和 `//src/render/map2d:map2d_sources`。改 `src/render/BUILD.gn` 文件头注释：DLL 不再包含 atmosphere 与 map2d。
- [x] 调用方改依赖（见下面的 GN 表）。`frame_graph_test` 增加 `//src/vista/component/map:map_sources`，不要加 atmosphere。
- [x] 改 `src/render/README.md` 目录表：删掉 `atmosphere/` 与 `map2d/` 两行；写明 GPU pass 在 `src/vista/component/map` 与 `src/vista/component/atmosphere`，经 `source_set` 链进调用方，不进 `render.dll`。As-built 段里的 `render::map2d::Pass` 改为 `vista::Pass`，测试标签改为 `//src/vista/component/map:map_effect_test`。
- [x] `docs/README.md` 帧图那一行补半句：GPU 地图与大气 pass 在 `src/vista`，不在 `render.dll`。规格路径不变。
- [x] `src/README.md` 与 `docs/superpowers/src-layout.md` 的「五层」补上 `effect`：`src/vista/{map,atmosphere}` 是 `source_set`，不是 DLL。`render` 行保持 `rhi` / `scene` / `graph` / `skia` / `math`。根 `README.md` 若仍只写 GIS / UI / render，补上 `effect` 是调用方链接的 pass，不是新产品 DLL。
- [x] 不要改归档计划，不要实现其余领域，不要搬 CPU 树。CPU 并进 `src/gis/vista` 是第 11 节。

### 测试目标

`map_effect_test` 照搬今天 `map2d_pass_test` 的 `sources`（`pass.cc`、`glyph_windows.cc`、`detail` 的四个 `.cc`、`pass_test.cc`），不要把 `map_effect.cc` 算进这个测试（今天也没有）。`output_name` 仍是 `map2d_pass_test`。`build.bat` 吃的是 ninja 目标名，所以命令是 `build.bat map_effect_test`，不是旧的 `map2d_pass_test`。

大气四个 pass 测试和 `atmosphere_frame_test` 保持同名 GN 目标（`cloud_pass_test`、`ocean_pass_test`、`sky_pass_test`、`fog_pass_test`、`atmosphere_frame_test`），只改目录和 include。`sources` 列表照搬今天的 `BUILD.gn`。

- [ ] 人跑：

```bat
build.bat frame_graph_test
build.bat map_effect_test
build.bat atmosphere_frame_test
build.bat cloud_pass_test
build.bat ocean_pass_test
build.bat sky_pass_test
build.bat fog_pass_test
build.bat scene3d_controller_test
build.bat map_scene_test
```

### Include（必须改）

`render/map2d/` → `vista/component/map/`：

| 旧 include | 文件 |
| --- | --- |
| `render/map2d/map_effect.h` | `src/render/map2d/map_effect.cc`，`src/content/browser/document/map_scene.cc`，`src/render/graph/frame_graph_test.cc` |
| `render/map2d/pass.h` | `map_effect.cc`，`pass.cc`，`glyph_windows.cc`，`detail/atlas.cc`，`pass_test.cc`，`map_scene.cc`，`frame_graph_test.cc` |
| `render/map2d/detail/atlas.h` | `pass.cc`，`detail/atlas.cc`，`detail/upload.h` |
| `render/map2d/detail/encode.h` | `pass.cc`，`detail/encode.cc` |
| `render/map2d/detail/place.h` | `pass.cc`，`detail/place.cc`，`detail/atlas.h`，`pass_test.cc` |
| `render/map2d/detail/upload.h` | `pass.cc`，`detail/upload.cc`，`detail/encode.h` |
| `render/map2d/detail/color.h` | `detail/encode.cc`，`detail/place.cc`，`detail/atlas.cc` |

`map_effect.h` 与 `pass.h` 自己不包含 `render/map2d/`。它们包含的 `gis/map2d/frame.h` 和 `render/graph/frame_graph.h`、`render/rhi/rhi.h` 保持不动。删掉其中的 `render/render_export.h`。

`render/atmosphere/` → `vista/component/atmosphere/`：

| 旧 include | 文件 |
| --- | --- |
| `render/atmosphere/frame/atmosphere_effects.h` | `frame/atmosphere_effects.cc`，`src/app/views/scene3d/scene3d_controller.cc` |
| `render/atmosphere/frame/atmosphere_frame.h` | `atmosphere_effects.h`，`frame/atmosphere_frame.cc`，`frame/atmosphere_frame_test.cc`，`src/app/views/scene3d/scene3d_controller.h` |
| `render/atmosphere/cloud/cloud_pass.h` | `cloud/cloud_pass.cc`，`cloud/cloud_pass_test.cc`，`frame/atmosphere_frame.cc`，`frame/atmosphere_frame_test.cc`，`scene3d_controller.h` |
| `render/atmosphere/fog/fog_pass.h` | `fog/fog_pass.cc`，`fog/fog_pass_test.cc`，`frame/atmosphere_frame.cc`，`frame/atmosphere_frame_test.cc`，`scene3d_controller.h` |
| `render/atmosphere/sky/sky_pass.h` | `sky/sky_pass.cc`，`sky/sky_pass_test.cc`，`frame/atmosphere_frame.cc`，`frame/atmosphere_frame_test.cc`，`scene3d_controller.h` |
| `render/atmosphere/ocean/ocean_pass.h` | `ocean/ocean_pass.cc`，`ocean/ocean_pass_test.cc`，`ocean/gpu_fields.cc`，`frame/atmosphere_frame.cc`，`frame/atmosphere_frame_test.cc`，`scene3d_controller.h`，`scene3d_controller.cc` |
| `render/atmosphere/ocean/cpu_waves.h` | `ocean/cpu_waves.cc`，`ocean/gpu_fields.cc`，`ocean/ocean_pass.cc` |
| `render/atmosphere/ocean/gpu_fields.h` | `ocean/gpu_fields.cc`，`ocean/ocean_pass.cc` |
| `render/atmosphere/common/field_texture.h` | `common/field_texture.cc`，`ocean/ocean_pass.cc`，`ocean/ocean_pass_test.cc` |
| `render/atmosphere/detail/math.h` | `ocean/cpu_waves.cc`，`ocean/gpu_fields.cc`，`ocean/ocean_pass.cc`，`sky/sky_pass.cc`，`fog/fog_pass.cc`，`cloud/cloud_pass.cc` |
| `render/atmosphere/detail/mesh.h` | `ocean/ocean_pass.cc`，`sky/sky_pass.cc`，`fog/fog_pass.cc`，`cloud/cloud_pass.cc` |
| `render/atmosphere/detail/raster.h` | 同上四个 pass `.cc` |

公开 pass 头（`ocean_pass.h`、`cloud_pass.h`、`sky_pass.h`、`fog_pass.h`、`field_texture.h`、`atmosphere_frame.h`）今天只包含 `render/render_export.h`，不包含兄弟 atmosphere 头。搬迁时删掉这份 include，并去掉类上的 `RENDER_EXPORT`。`detail/*.h` 没有 atmosphere include。

`src/render/graph/frame_graph.h`、`frame_graph.cc` 没有 map / atmosphere include。不要加。

`map_scene.h` 没有 include，只有前向声明 `render::map2d::Pass`，改成 `vista::Pass`。

下面这些不是 include，但字符串要改，否则还指向旧命名空间：

| 位置 | 今天 |
| --- | --- |
| `map_scene.cc` | `render::map2d::WindowsGlyphRasterizer`、`Pass`、`MapEffect` |
| `map_scene.h` | `namespace render { namespace map2d { class Pass;` |
| `frame_graph_test.cc` | `render::map2d::Pass`、`MapEffect` |
| `pass_test.cc` | `render::map2d::GlyphRasterizer`、`detail::PlacedMesh`、`detail::place_frame`、`Pass` |
| `scene3d_controller.h` | `render::atmosphere::OceanPass`、`CloudPass`、`SkyPass`、`FogPass`、`AtmosphereFrame` |
| `scene3d_controller.cc` | `OceanDrawParams`、`FogDrawParams`、`AtmosphereEffects` |
| `sky/sky_pass_test.cc` | `render::atmosphere::SkyDrawParams`、`SkyPass` |
| `src/gis/scene/atmosphere/systems/ocean_system.h` | 注释 `render::atmosphere::OceanPass` |
| `src/gis/scene/atmosphere/systems/atmosphere_params.h` | 注释 `render::atmosphere` POD |
| `src/app/views/README.md` | `render::map2d::Pass` |

搬完后在 `src/` 再搜 `render::map2d`、`render::atmosphere`、`render/map2d/`、`render/atmosphere/`，只应剩文档里尚未改的历史规格（不要为了这次去改归档计划）。

### GN 标签（必须改）

从 `src/render/BUILD.gn` 的 `//src/render:render` `deps` 删除：

- `//src/render/atmosphere:atmosphere_sources`
- `//src/render/map2d:map2d_sources`

删除整个旧目录后，这些标签消失（不要留转发 `group`）：

| 旧标签 | 新标签 |
| --- | --- |
| `//src/render/map2d:map2d_sources` | `//src/vista/component/map:map_sources` |
| `//src/render/map2d:map2d`（`public_deps` → `:render`） | 删除。调用方改依赖 `map_sources`。RHI 仍走已有的 `//src/render:rhi` |
| `//src/render/map2d:map2d_pass_test` | `//src/vista/component/map:map_effect_test` |
| `//src/render/atmosphere:atmosphere_sources` | `//src/vista/component/atmosphere:atmosphere_sources` |
| `//src/render/atmosphere:atmosphere`（`public_deps` → `:render`） | 删除 |
| `//src/render/atmosphere:cloud_pass_test` | `//src/vista/component/atmosphere:cloud_pass_test` |
| `//src/render/atmosphere:ocean_pass_test` | `//src/vista/component/atmosphere:ocean_pass_test` |
| `//src/render/atmosphere:sky_pass_test` | `//src/vista/component/atmosphere:sky_pass_test` |
| `//src/render/atmosphere:fog_pass_test` | `//src/vista/component/atmosphere:fog_pass_test` |
| `//src/render/atmosphere:atmosphere_frame_test` | `//src/vista/component/atmosphere:atmosphere_frame_test` |

调用方 `deps`：

| 文件 | 今天 | 改成 |
| --- | --- | --- |
| `src/app/views/BUILD.gn` `map_scene` | `//src/render/map2d:map2d` | `//src/vista/component/map:map_sources` |
| `src/app/views/BUILD.gn` `map_scene_test` | `//src/render/map2d:map2d` | `//src/vista/component/map:map_sources` |
| `src/app/views/BUILD.gn` `scene3d_controller` | 无直接 atmosphere 标签（符号来自 DLL） | 增加 `//src/vista/component/atmosphere:atmosphere_sources` |
| `src/app/views/BUILD.gn` `scene3d_controller_test` | 无 | 增加 `//src/vista/component/atmosphere:atmosphere_sources` |
| `src/render/graph/BUILD.gn` `frame_graph_test` | `:graph`、`//src/gis:gis`、`//third_party:gdal` | 保持，并增加 `//src/vista/component/map:map_sources`。不要加 atmosphere |
| 根 `BUILD.gn` `test_shell` | `//src/render/map2d:map2d_pass_test` | `//src/vista/component/map:map_effect_test` |

不要改 `//src/gis/map2d:map2d`、`//src/gis/map2d:map2d_sources`、`//src/gis/scene/atmosphere:atmosphere_sources`。那是 CPU，终态在第 11 节。

`//src/render:render` 搬完后的 `deps` 只剩：`:rhi_sources`、`//src/render/graph:graph_sources`、`//src/render/scene:scene_sources`。场景数学是 `//src/base/math:math` 与 `:bounds`，不进这张 DLL。第 10 节再给同一 DLL 加上 opaque 适配器的 `source_set`，不新开 DLL，也不把 `GpuScene` 放进 `src/vista`。壳栅格是 `//src/ui/gfx:gfx`，不进这张 render DLL deps。

## 10. Fold scene recording into graph

人跑编译。不要新开分支。第 9 节整节保留：`vista/component/map` 与 `vista/component/atmosphere` 实现 `graph::Effect` 并离开 `render`。本节不把 `GpuScene` 放到 `src/vista`。不透明录制是 graph 自己的 `kOpaque` pass，因为 Render Scene 属于 render。

`src/render/scene` 只留 GPU 缓存。投影、pass 顺序、Clear/Load 归 `src/render/graph`。

- [x] 把 `src/render/scene/opaque_effect.h` 与 `opaque_effect.cc` 移到 `src/render/graph/`，命名空间改为 `render::graph`。类仍是 `OpaqueEffect`，槽仍是 `kOpaque`。`scene_sources` 去掉这两个文件，并去掉对 `//src/render/graph:graph_sources` 的 `deps`（`GpuScene` 不再包含 `frame_graph.h`）。不要把 `opaque_effect.cc` 放进 `graph_sources`：那个目标继续只依赖 `rhi_sources`，`frame_graph.cc` / `present` 继续不包含 `scene.h`、gis 地图头或大气头。在 `src/render/graph/BUILD.gn` 增加仍编进 `//src/render:render` 的 `source_set`（依赖 `graph_sources` 与 `//src/render/scene:scene_sources`）。`scene_sources` 与 `graph_sources` 不要互相 `deps`。不新开 DLL。第 9 节若已改过 `render` 的 `deps`，在那份列表上追加这个 `source_set`。
- [x] 改 `GpuScene::record_draws`：调用方已经提供 `RecordContext` 的相机时，不再 `bind_camera`，也不再在正交和透视之间选择。graph 里的 `OpaqueEffect::record` 在 draw 之前把 `ctx.camera` 绑定一次，并设置 `color_op` / `shared_depth`（深度 Load 仍跟颜色 Load），然后调用 scene 的窄绘制入口，不再经 `set_view_camera` 让 `record_draws` 绑第二遍。没设相机时保留今天的分支（先 `make_ortho_camera` 画栅格和矢量，有三维实例再 `make_perspective_camera`），在该分支上加注释标明临时。`set_view_ortho` 继续把网格标脏（`meshes_dirty_`，`rebuild_meshes` 用包络算 `world_units_per_pixel`）。它不是第二套投影；`scene.h` 里对应注释改成这个意思。网格数组、细分、`frustum_aabb` 留在 `scene/`。
- [x] 改 include 与限定名：`src/app/views/scene3d/scene3d_controller.cc`、`src/render/graph/frame_graph_test.cc` 从 `render/scene/opaque_effect.h` 改为 `render/graph/opaque_effect.h`，`render::scene::OpaqueEffect` 改为 `render::graph::OpaqueEffect`。
- [x] `src/render/README.md` 目录表：`scene/` = GPU 缓存；`graph/` = `present` + opaque 适配器。若第 9 节已删掉 `atmosphere/` 与 `map2d/` 行，不要加回去。
- [ ] 人跑（目标名来自 `src/render/scene/BUILD.gn` 的 `test("scene_gpu_test")` 与 `src/render/graph/BUILD.gn` 的 `test("frame_graph_test")`）：

```bat
build.bat frame_graph_test
build.bat scene_gpu_test
```

## 11. Merge gis/map2d into gis/vista

人跑编译。不要新开分支。不要搬文件以外的行为。第 9 节与第 10 节整节保留：GPU 地图与大气 pass 在 `src/vista`，不透明录制在 `src/render/graph`。本节不把 GPU pass 放进 `gis/vista`，也不把 `present` 并进 `vista`。

`vista` 是视口所持的那一幅景象：正交或透视都是同一帧，不是第二颗行星，也不是 GPU scene。`gis::World` 类名不动。不要引入 `gis::vista::frame`。

### 11a. 第一次合并（磁盘已落地）

- [x] 建 `src/vista/`，整目录搬走，不留转发头。`src/gis/map2d` 与 `src/gis/scene` 不再存在。

| 已完成（旧 → 当时终态） | |
| --- | --- |
| `src/gis/map2d/` | `src/vista/frame/` |
| `src/gis/scene/world/` | `src/vista/scene/`（含 `scene/scene/` + `terrain/`） |
| `src/gis/scene/assets/` | `src/vista/assets/` |
| `src/gis/scene/domain/` | `src/vista/domain/` |
| `src/gis/scene/atmosphere/` | `src/vista/component/atmosphere/`（11b 再收到 `domain/atmosphere/`） |

- [x] 命名空间：`gis::map2d` → `gis::vista`（含 `detail`）。`gis::World`、`gis::DomainKind`、`gis::DomainSession` 留在 `gis`。
- [x] Include / guard / `BUILD.gn` / present README：已按当时 `vista/{frame,scene,assets,domain,atmosphere}` 前缀改完（见下方历史表，仅作考古）。
- [x] 不要改第 9、10 节，不要改归档计划，不要实现其余领域。历史 docs 旧路径本次不追改。

### 11b. Follow-up：锁定 world + domain/atmosphere（已落地）

规格 GIS 节已锁定。本步只改路径与 include / GN，不改行为；**不改名 `Environment`**；不引入 `gis::vista::atmosphere`。

- [x] `git mv src/vista/scene` → `src/vista/component/world`。把 `world/scene/scene.{h,cc}`（及 `scene_test.cc`）升到 `world/` 并列：`world.h` / `world.cc` / `world_test.cc`（不要 `world/world/`）。`terrain/` 留在 `world/terrain/`。不留转发头。搬完后 `src/vista/scene` 不再存在。
- [x] Include：`"vista/scene/scene/scene.h"` → `"vista/component/world/world.h"`；`"vista/scene/terrain/"` → `"vista/component/world/terrain/"`。Guard：`GIS_VISTA_SCENE_H_` → `GIS_VISTA_WORLD_H_`。类仍是 `gis::World`。
- [x] GN：`//src/vista/scene:` → `//src/vista/component/world:`（目标名 `world_sources`、`land_mask`、`world`；测试 `world_test`；已删 `scene` 别名 group）。更新 `src/gis/BUILD.gn`、`frame/BUILD.gn`、`domain/atmosphere` 与所有树外 `deps`。
- [x] `git mv src/vista/component/atmosphere` → `src/vista/domain/atmosphere`（保留 `field/`、`systems/`）。不留转发头。搬完后顶栏不再有 `vista/component/atmosphere`。
- [x] Include：`"vista/component/atmosphere/"` → `"vista/domain/atmosphere/"`。GN：`//src/vista/component/atmosphere:` → `//src/vista/domain/atmosphere:`（独立 `domain/atmosphere/BUILD.gn`）。`domain.h` 仍在 `vista/domain/domain.h`。
- [x] 命名空间：不要引入第三层 `gis::vista::atmosphere`。现有 `gis::atmosphere`（`Environment` 等）可保持；本步不改类名。
- [x] `src/gis/BUILD.gn` 文件头改成 `vista/{frame,world,assets,domain}`，并写明 `domain/atmosphere` 是唯一已实现的 `DomainKind` 会话包；`frame` = CPU `MapFrame`；`world` = `gis::World` 节点图。
- [ ] 人跑：`build.bat frame_test`、`build.bat world_test`、`build.bat environment_test`，以及 `scene3d_controller_test` / `map_scene_test` / `scene_gpu_test`（由人执行；代理不编译）。

### 11a 考古：当时 include / GN 表

以下表描述 **11a 已完成** 的替换，不是 11b 目标。

#### 11a Include（已完成时的命中；树外今天已是 `vista/...`）

11b 前缀对照（考古；磁盘已是 11b）：

| 前缀（11a 后 / 11b 前） | 11b 后 |
| --- | --- |
| `vista/scene/scene/scene.h` | `vista/component/world/world.h` |
| `vista/scene/terrain/` | `vista/component/world/terrain/` |
| `vista/component/atmosphere/` | `vista/domain/atmosphere/` |

#### 11a GN 标签（已完成）

| 11a 前 | 11a 后 | 11b 后（磁盘现状） |
| --- | --- | --- |
| `//src/gis/map2d:map2d_sources` | `//src/vista/frame:frame_sources` | 不变 |
| `//src/gis/scene/world:` | `//src/vista/scene:` | `//src/vista/component/world:` |
| `//src/gis/scene/assets:` | `//src/vista/assets:` | 不变 |
| `//src/gis/scene/domain:` | `//src/vista/domain:` | 不变（`domain.h`） |
| `//src/gis/scene/atmosphere:` | `//src/vista/component/atmosphere:` | `//src/vista/domain/atmosphere:` |

`//src/vista/component/world:` 目标：`world_sources`、`land_mask`、`world`、`world_test`、`land_mask_test`、`dem_raster_test`、`tessellate_style_test`（无 `scene` 别名 group）。`//src/vista/domain/atmosphere:`：`atmosphere_sources`、`atmosphere`、各 `*_test`。

docs 里还有更旧路径，11b 不强制追改归档规格。
