<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Render 分层：Frame Graph + 单次 present


> **Status: superseded** (2026-09-28 merge). Merged into `2026-09-13-render-rhi-scene-design.md` §Frame graph. Do not revise here except mechanical link fixes.

**Status:** active  
**Date:** 2026-09-27  
**Related:** RHI + 双场景 [`2026-09-13-render-rhi-scene-design.md`](../../specs/2026-09-13-render-rhi-scene-design.md)（Backend 与 `World` / `GpuScene` 仍以该规格为准）；Views 二维接线 [`2026-09-27-views-2d-map-rhi-design.md`](2026-09-27-views-2d-map-rhi-design.md)；CPU 帧 [`2026-09-27-map2d-frame-design.md`](2026-09-27-map2d-frame-design.md)；环境顺序 [`2026-09-19-atmosphere-ocean-cloud-design.md`](2026-09-19-atmosphere-ocean-cloud-design.md)；GPU 进程合成加速（`CompositorFrame` + `FrameComposer`，与本规格的 in-process `present` 分路）[`2026-09-27-gpu-rhi-accelerate-design.md`](2026-09-27-gpu-rhi-accelerate-design.md)。  
**Plan:** [`../plans/2026-09-27-render-frame-graph.md`](../../plans/2026-09-27-render-frame-graph.md)

## Goal

一个视口、一个相机、一条 `CommandList`、一次 `execute` / `present`。Pass 只录制。`MapScene` 与 `Scene3dController` 不再各自拼命令列表。

`render/map2d` 不是第二条渲染器。地图是三维绘制的正交特例：同一份网格，相机用 `make_ortho_camera`（视口经纬度范围，与 `GpuScene` 的正交跨度相同）。三维视图用透视 `CameraMatrices`。一帧只绑定这一份相机。`src/render/scene` 也不是第二条渲染器：投影、pass 顺序和 Clear/Load 的终态在 `src/render/graph`（见 Render Scene）。没传入相机时，`record_draws` 可以暂时留下旧的「二维正交、三维透视」，注释标明临时。

## Layers

| 层 | 类型 | 落点 |
| --- | --- | --- |
| Backend | `render::rhi` | `src/render/rhi`，本规格不改 |
| Render Scene | `render::scene::GpuScene` | GPU 驻留缓存：`sync_from(gis::World)`、网格、材质、实例绘制、`release` / `abandon`。不选投影、pass 顺序或 Clear/Load。见 Render Scene |
| Pass | 同一相机下的网格，经 adapter 进入 `Effect` | `OpaqueEffect` 终态在 `src/render/graph`（graph 自己的 `kOpaque`）。`MapEffect` 与大气 GPU pass 在 `src/vista`（见 Effect tree）。不为每个领域新开 render 目录，也不做新 DLL |
| Frame Graph | `render::graph` | `src/render/graph`。四个 `EffectSlot`、`present`，以及 `OpaqueEffect`。`present` 仍是槽遍历，不包含 gis 地图或大气头。`ViewInput` 不点名 `GpuScene`、`MapFrame`、`AtmosphereFrame` |
| View | `render::graph::present` | 同上。`ViewInput` 只有宽高、一个相机、非拥有的 `Effect*` 列表 |

`gis` 不包含 `render`。公共命名空间停在两层。帧类型是 `gis::vista`；`gis::style`、`gis::tile` 不动；`World` 与 `DomainSession` 留在 `gis`。不要写成 `gis::vista::frame`，也不要发明 `gis::vista::atmosphere`。新函数用 `snake_case`。壳层继续走 `ui/gfx`，不进这张图。

## Effect tree

`render`（`src/render`，`dll_stem = render`）只保留 RHI、GPU 场景、帧图、壳画布和场景数学。地图网格和大气 pass 要 `#include` `render/rhi` 与 `render/graph`，所以不能放进 `gis`（`gis` 不包含 `render`）。它们也不是帧图本身：`present` 只按 `Effect*` 走，`graph` 不包含 map 或 atmosphere 头。继续编进 `render.dll` 会让 DLL 编译并依赖这些 pass。

终局目录（嵌套停在 `src/<layer>/<module>`；`ocean/`、`detail/`、`frame/` 仍是模块内部，与今天 `render/atmosphere/ocean` 相同）：

| 今天 | 终局 | 命名空间 | 链接 |
| --- | --- | --- | --- |
| `src/render/map2d`（`Pass`、`MapEffect`、glyph、`detail`） | `src/vista/map` | `effect::map`（内部 `effect::map::detail`） | `//src/vista/map:map_sources` |
| `src/render/atmosphere`（ocean / cloud / sky / fog、`AtmosphereFrame`、大气 Effect） | `src/vista/atmosphere` | `effect::atmosphere`（内部 `effect::atmosphere::detail`） | `//src/vista/atmosphere:atmosphere_sources` |

Include：`vista/map/pass.h`、`vista/atmosphere/frame/atmosphere_effects.h`。`src/render/` 下不留转发头。

它们是 `source_set`，不是新 DLL。`//src/render:render` 不编译这些源文件，也不 `deps` 它们。调用方自己依赖：`src/app/views` 的 `MapScene`、`Scene3dController` 及其测试。`frame_graph_test` 仍依赖 `//src/render:render`；只有构造 `MapEffect` 的测试再依赖 `//src/vista/map`。

`render` 留下：

| 目录 | 留下的内容 |
| --- | --- |
| `rhi/` | Backend |
| `scene/` | Render Scene：`GpuScene` GPU 缓存。不含 `OpaqueEffect` |
| `graph/` | `Effect`、`EffectSlot`、`RecordContext`、`present`，以及 `OpaqueEffect`（`kOpaque`） |
| `skia/` | 壳画布，不是 GIS 帧 |
| `math/` | 场景数学 |

这一步不搬 CPU。CPU 已在 `src/gis/vista`（见 GIS）；Effect 步只动 GPU pass。不实现 factory、geology、offshore、storm-surge、space。不把 `GpuScene` 放到 `src/vista`。`OpaqueEffect` 留在 render，终态在 `render/graph`（见 Render Scene）。

## Render Scene

`src/render/scene` 的角色叫 **Render Scene**。它只保留 GPU 驻留缓存：`GpuScene::sync_from(gis::World)`、网格、材质、实例绘制（`set_instance_paint`）、`release` / `abandon`。它不选择投影，不排 pass 顺序，也不决定 Clear / Load。

这些录制策略的终态在 `src/render/graph`，不再堆在 `GpuScene` 里：

- `OpaqueEffect`（或其后继）从 `src/render/scene/opaque_effect.h` 移到 `src/render/graph`，命名空间 `render::graph`。它是 graph 自己的 `kOpaque` 适配器。
- `GpuScene::record_draws` 里的双相机绑定是遗留。今天 `view_camera_set_` 为 false 时，先 `bind_camera(make_ortho_camera)` 画栅格和矢量，有三维实例再 `bind_camera(make_perspective_camera)`。终态：调用方给出 `RecordContext` 的相机时，`record_draws` 只为当前上下文吐网格，不调用 `bind_camera`，也不在正交和透视之间选择。graph（或 graph 里的 `OpaqueEffect`）在 draw 之前把 `context.camera` 绑定一次，并设置 `color_op` / `shared_depth`。没传入相机时可以留下旧分支，注释标明临时。
- `set_view_ortho` 若仍把 `meshes_dirty_` 置上，并让 `rebuild_meshes` 用该包络计算 `world_units_per_pixel`（线宽、圆半径的世界单位），它只是网格标脏输入。它不是第二套投影。
- `present()` 仍按 `EffectSlot` 遍历。它仍然不包含 gis 地图头或大气头。`ViewInput` 仍不点名 `GpuScene`。

留在 `scene/`、不进 graph 的：`GpuScene` 的网格数组、细分、数据面的视锥提取（`frustum_aabb`）。graph 可以调用一个窄的「画实例」入口。不新开 DLL。

与 Effect tree 的关系：`vista/map` 与 `vista/atmosphere` 实现 `graph::Effect` 并离开 `render`。`GpuScene` 的不透明录制不是外部效果，而是 graph 自己的 `kOpaque` pass，因为 Render Scene 属于 render。不要把 `GpuScene` 放到 `src/vista`。

## GIS

`vista` 是视口所持的那一幅景象：正交或透视都是同一帧，不是第二颗行星，也不是 GPU scene。父目录不叫 `scene`：它和 `render/scene` 撞名，也像只装三维。

磁盘上已完成：`src/gis/map2d` + `src/gis/scene` → `src/vista/{frame,world,assets,domain}`，且 `domain/atmosphere` 为唯一已实现会话包（旧 `vista/scene`、顶栏 `vista/atmosphere` 已不存在）。

| 路径（磁盘现状） | 状态 | 职责 |
| --- | --- | --- |
| `src/vista/frame/` | 已落地 | CPU `MapFrame`。正交是今天的地图；`ViewMode::kPerspective` 以后仍是这一帧。没有 RHI、HWND、`CameraMatrices`。网格在视图 CRS |
| `src/vista/world/` | 已落地 | 节点图：`class gis::World`、地形、陆地掩膜。`world/terrain/` 下放 DEM / 掩膜。源文件是并列的 `world.h` / `world.cc`（不要 `world/world/`） |
| `src/vista/assets/` | 已落地 | CPU mesh / tileset |
| `src/vista/domain/` | 已落地（`domain.h`） | `DomainKind` / `DomainSession`，类型仍在命名空间 `gis` |
| `src/vista/domain/atmosphere/` | 已落地 | CPU `Environment`（`kAtmosphere` 会话包：field + systems）。不是 World 的兄弟，也不是第二套「环境 = 大气」顶栏 |

**为什么 atmosphere 嵌在 `domain/` 下：** `Environment` 已经是 `DomainKind::kAtmosphere` 会话（`kind()` 返回 `kAtmosphere`；不强制继承 `DomainSession`）。挂在 `vista/atmosphere` 会让每个未来领域都变成 `vista/` 顶栏同伴，并复现「atmosphere = environment」的旧味。嵌在 `domain/atmosphere/` 后，factory / geology / offshore / storm / space 是 atmosphere 的兄弟，不是 World 的兄弟。

**为什么节点图目录叫 `world` 不叫 `scene`：** 类已是 `gis::World`；`vista/scene` 与 `render/scene`（GpuScene）撞词，且 `scene/scene/` 是冗余嵌套。改名只动路径与文件名，不改类名。

**拒绝的备选：**

- 保留 `vista/atmosphere` 与 `vista/world` 平级：扁平，但 taxonomy 不是 domain。
- 把 atmosphere 并进 `world/`：会话时钟 / FieldStore 与空间节点图混在一处。
- 把 atmosphere 放进 `frame/`：`MapFrame` 是制图布局，不是领域会话。

**路径与命名（11b 已落地）：**

- 节点图：`vista/world/world.h`；guard `GIS_VISTA_WORLD_H_`；地形 `vista/world/terrain/`。
- 大气：`vista/domain/atmosphere/`（`field/`、`systems/`）。**不**引入 `gis::vista::atmosphere`。CPU 类型继续用 `gis::atmosphere`；**不改名 `Environment`**。
- 帧头保持 `vista/frame/frame.h`。公开帧类型是 `gis::vista`（`Layout`、`MapFrame`、`View`、`ViewMode` 等）；`gis::vista::detail` 放碰撞等内部。`gis::World` 仍是 `gis::World`。不要引入 `gis::vista::frame`。

`src/gis/present` 仍是样式和瓦片输入：`StyleDocument`、符号、tile。`gis::vista::Layout` 消费它们。`World` 与 `DomainSession` 不解析 Style JSON。`present` 不并进 `vista`。

GPU 地图 pass 与大气 pass 在 `src/vista/{map,atmosphere}`（见 Effect tree），消费 `gis::vista` 的 CPU 类型。`gis` 不包含 `render`。不透明录制仍由 `src/render/graph` 持有。GPU pass 不放进 `gis/vista`。

`render::graph::present` 绑定一个相机，并持有非拥有的 `Effect*` 列表。槽位按录制顺序是 `kBeforeOpaque`、`kOpaque`、`kAfterOpaque`、`kOverlay`。`kOpaque` 是地形、模型和地图世界网格；`kOverlay` 是屏幕图标和文字。`AtmosphereFrame` 仍排大气 pass（`sky → ocean` 与 `cloud → fog`），由 `PreOpaqueEffect` / `PostOpaqueEffect` 包进前段和后段，不继承 `DomainSession`。不透明体是 `OpaqueEffect`，地图是 `MapEffect`。factory、geology、offshore、storm-surge、space 以后各写一个 `Effect`，不是这次的实现，也不为它们各开一个 render 目录。

`DomainKind`：`kAtmosphere`、`kFactory`、`kGeology`、`kOffshore`、`kStormSurge`、`kSpace`。本规格不实现后五项。CPU `Environment` 终态在 `vista/domain/atmosphere`。GPU pass 的目录见 Effect tree。

`ViewMode` 挂在 `gis::vista::View` 上：`kOrtho`（默认）、`kPerspective`。`DrawKind` 不变。图标和文字仍是屏幕 HUD；填充、线、圆、栅格留在视图 CRS。`Vertex` 已有 `z`。`Layout` 不按 `ViewMode` 分支，也不做球体或 ECEF。

## Order

`present` 只走 `effects`。空指针跳过。同一槽内保持调用方给出的顺序。槽的顺序固定：

1. `EffectSlot::kBeforeOpaque`。
2. `EffectSlot::kOpaque`。地形、模型、地图世界网格。
3. `EffectSlot::kAfterOpaque`。
4. `EffectSlot::kOverlay`。屏幕图标和文字。

图拥有 Load 策略，并写进每次 `record` 的 `RecordContext`。`color_op` 从 `kClear` 开始；任一效果 `clears_color()` 为 true 之后，后面的效果得到 `kLoad`。`shared_depth` 在任一效果 `uses_shared_depth()` 为 true 之后为 true。效果自己不 `close`、不 `execute`、不 `present`。`present` 成功路径上关闭一次命令列表。

`effects` 为空（或只有空指针）时，仍 Clear 一次再 present。`device == nullptr` 或宽高为 0 返回 false。

只画地图的宿主推一个 `MapEffect`，`record_all == true`（槽是 `kOpaque`，一次 `Pass::record` 同时画世界网格和注记）。`Pass::record` 在两个开关都为 true 时会 `close`；`present` 再 `close` 一次。`StubCommandList::close` 只把 `closed` 设为 true，重复调用无额外动作。默认 `Pass::record`（两个开关都为 true、`kClear`、会 close）不变，`map2d_pass_test` 仍走这条。

终态：graph 的 `OpaqueEffect` 在 draw 之前绑定 `RecordContext::camera` 一次，并写入 `color_op` / `shared_depth`，然后调用 scene 的窄绘制入口。`clears_color()` 为 false，因此 `color_op == kLoad` 时不透明 pass 不清颜色。深度 Load 跟颜色 Load 走：前面清过颜色时不把共享深度清掉。有相机时 `record_draws` 不再 `bind_camera`，也不经 `set_view_camera` 绑第二份投影。见 Render Scene。

`View::mode == kPerspective` 时，gis 不发明透视矩阵。`Pass` 使用宿主传入的 `camera`；未传入时仍绑定视图范围的正交，而不是透视。

## ViewInput

```cpp
namespace render {
namespace graph {

enum class EffectSlot {
  kBeforeOpaque,
  kOpaque,       // terrain, models, map world meshes
  kAfterOpaque,
  kOverlay,      // screen icons and text
};

struct RecordContext {
  rhi::Device* device = nullptr;
  rhi::CommandList* list = nullptr;
  uint32_t width = 0;
  uint32_t height = 0;
  const rhi::CameraMatrices* camera = nullptr;
  rhi::ColorLoadOp color_op = rhi::ColorLoadOp::kClear;
  bool shared_depth = false;
};

class Effect {
 public:
  virtual ~Effect();  // defined in frame_graph.cc; anchors the vtable in render.dll
  virtual EffectSlot slot() const = 0;
  virtual bool clears_color() const { return false; }
  virtual bool uses_shared_depth() const { return false; }
  virtual bool record(const RecordContext& ctx) = 0;
};

struct ViewInput {
  uint32_t width_px = 0;
  uint32_t height_px = 0;
  const rhi::CameraMatrices* camera = nullptr;
  // Non-owning. Nothing else: no GpuScene, no MapFrame, no AtmosphereFrame.
  std::vector<Effect*> effects;
};

bool present(rhi::Device* device, const ViewInput& in);

}  // namespace graph
}  // namespace render
```

终态 `OpaqueEffect` 在 `render::graph`（`src/render/graph/opaque_effect.h`）。今天的文件还在 `src/render/scene/opaque_effect.h`。`MapEffect` 在 `effect::map`（`src/vista/map/map_effect.h`）。`present` 所在的 `graph_sources` 仍只依赖 `rhi_sources`，不包含 map、atmosphere 或 `scene.h`。`OpaqueEffect` 另编进同一个 `render.dll`，调用 scene 的窄绘制入口。

云质量留在 `PostOpaqueEffect` 上，不进 `ViewInput`。`MapScene::present_gpu` 用视图范围的 `make_ortho_camera` 填 `camera`，`effects` 里只有一个 `record_all` 的 `MapEffect`。`Scene3dController` 在 `prepare_atmosphere_*` 之后按顺序推入前段效果、`OpaqueEffect(&gpu_scene_)`、后段效果；`backend == kNull` 时 `camera` 为空，避免 Null 上的视锥裁剪。

`effect::map::Pass::record` 增加两个开关，默认都为 true，现有调用保持全画：

```cpp
bool record(..., bool world_items = true, bool overlay_items = true,
            rhi::ColorLoadOp color_op = rhi::ColorLoadOp::kClear);
```

`world_items` 为 false 时不画 `pixel_space == false`，也不因背景而 Clear。`overlay_items` 为 false 时不画图标和文字。

## Non-goals

- Effect tree 与 graph 录制这两步不搬 `gis/map2d`、`gis/present`、`gis/scene`。CPU 终态是 `src/gis/vista`（见 GIS），单独一步，不在这两步里做。`present` 不并进 vista。GPU pass 不放进 `gis/vista`。不把 `map2d` 改名为 `map3d`。GPU 地图与大气 pass 迁到 `src/vista`（见 Effect tree）。`OpaqueEffect` 迁到 `render/graph`（见 Render Scene），不把 `GpuScene` 放到 `src/vista`。
- 不实现 factory、geology、offshore、storm-surge、space（它们是以后的 `Effect`，不是这次的类型或目录），也不在 GIS 里做球体数学或 ECEF。
- 不为每个领域新建 render 子目录或新的 DLL。`src/vista` 的两个模块是 `source_set`。
- 不把 `GpuScene` 的网格数组、细分和数据面视锥提取搬进 graph，也不为此新开 DLL。有 `RecordContext` 相机时，`record_draws` 不再选择投影；没相机的双绑定是临时遗留。
- 不让 `GpuScene` 再承担二维样式和注记。
- 不把 Skia 放进 Frame Graph。
