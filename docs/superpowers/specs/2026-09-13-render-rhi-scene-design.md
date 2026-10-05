<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Unified RenderDevice RHI (FlyCube) + dual scene (GIS World / GPU Scene)

**Status:** accepted  
**Date:** 2026-09-13  
**Updated:** 2026-10-05 — **§Vista IR/Pass lanes** 拆 `atmosphere` / `atmosphere_pass`（对标 `map`/`map_pass`；CPU `session` + `cpu_waves` 留在 `atmosphere/`；GPU pass 进 `atmosphere_pass/`；图 [`vista-subdirectory-layers.html`](../diagrams/vista-subdirectory-layers.html)）。Prior same day — **§Vista world_pass deep split**（根公开 `pass`/`gpu_mesh`/`opaque_effect` + `detail/{upload,tint,rebuild,draw,record}` + `cull/`；`rgba_from_resolved_paint` 归 `world/paint.h`；图 [`vista-subdirectory-layers.html`](../diagrams/vista-subdirectory-layers.html)）。Prior same day — **§Vista mesh subdirectory**（`mesh/` 按职责拆 `fill/` · `line/` · `detail/`；公开仍 `"vista/mesh/tessellate.h"`；图 [`vista-subdirectory-layers.html`](../diagrams/vista-subdirectory-layers.html)）。Prior same day — **§Vista IR/Pass lanes**（停用「逻辑 / 物理」；目录以该节为准：`map`/`MapIR` + `map_pass`/`MapPass`，`world`/`Instance` + `world_pass`/`WorldPass`；`frame/` 与 `scene/` 退出目标布局；图 [`vista-subdirectory-layers.html`](../diagrams/vista-subdirectory-layers.html)）。Prior same day — **§Vista scene subdirectory**（取消 `scene/gpu/`；`GpuScene` 与 `map/` 一样落在模块根；公开 `"vista/scene/scene.h"`；图 [`vista-scene-octree.html`](../diagrams/vista-scene-octree.html)）。Prior same day — **§Vista map/frame scenic damage**（N0 拆回 `geom_walk` / `mesh_emit` / `tess_grain`，删掉焊在一起的 `geom_mesh`；N1–N3 锁脏瓦片 / pack / job abort；图 [`vista-map-frame-scenic-peer.html`](../diagrams/vista-map-frame-scenic-peer.html)）。Prior same day — **§Vista map deep split**（`vista/component/map` 公开 IR/Layout 头拆开 + emit 辅件按职责；图仍 [`vista-map-frame-scenic-peer.html`](../diagrams/vista-map-frame-scenic-peer.html)）。Prior same day — **§Vista scene unibn octree**（`vista/scene/index` + 渲染视锥外过滤；图 [`vista-scene-octree.html`](../diagrams/vista-scene-octree.html)）。Prior same day — **§Vista map/frame scenic-peer**（`vista/component/map` CPU × `vista/frame` GPU 对照 Scenic `paint/map`；U1–U4 as-built；图 [`vista-map-frame-scenic-peer.html`](../diagrams/vista-map-frame-scenic-peer.html)）。Prior 2026-10-04 — **§DEM / hillshade bake profile + bench**（`cat=bake` · CPU `parallel_for` vs Thrust；图 [`hillshade-bake-profile.html`](../diagrams/hillshade-bake-profile.html)）。Prior same day — **§Scenic copy-type split**：`math.h`（vector 别名）与 `scenic_impl_export.h`（宏 + 一处 autolink）；STL/`ulong` 糖在 `scenic::detail` 非全局；`render/backend_dll.h` 共用 Loader。Prior same day — **§Vista logical/physical lanes U0–U4**（Task 8）。Prior same day — **§Scenic scene3d peer lock**：`scene3d/` ↔ `vista/scene`，禁止下沉 `rhi3d/impl/common`（[`legacy-render-architecture.html`](../diagrams/legacy-render-architecture.html)）。Prior same day — **§Vista logical/physical lanes**：[`vista-subdirectory-layers.html`](../diagrams/vista-subdirectory-layers.html) 与 [`legacy-render-architecture.html`](../diagrams/legacy-render-architecture.html) 对照 CPU Emit×N 与 GPU 设备线程。Prior same day — **§Content present/scene3d slim**：`scenic_engine_host` + `frame/{look_preset,scene3d_overlays}`；规范图 [`scene3d-present-layers.html`](../diagrams/scene3d-present-layers.html)。Prior same day — **§content present accelerate**：`content/browser/present` 三车道（C cache / G GPU 合成 / E `base::execution`）；规范图 [`content-present-accelerate.html`](../diagrams/content-present-accelerate.html)。Prior same day — **§RHI2D cc frame-beat upgrade**：`cc/` 是帧拍（commit/activate/FrameJob），与 `MapScheduler`×`TileGraphRunner` **组合不合并**；升级只在 `src/scenic` / `src/content`（[`rhi2d-paint-map-strategy.html`](../diagrams/rhi2d-paint-map-strategy.html)）。Prior same day — **§RHI2D map-layer-feature paint strategy**：`paint/map/` 拆 map / layer / feature（prep → encode → submit）；与 carto/cc 保持 sibling，不并入（同图）。Prior same day — **§RHI2D public composition split**：`Device2d*` 并入 `render_device.h`（无独立 `device_parts.h`）。Prior same day — **§Carto draw lattice seam**：`paint/carto/draw` 只消费 `LatticeView2d`，不认识 `plugin::detail::OrthoLattice`（[`rhi2d-device-api.html`](../diagrams/rhi2d-device-api.html)）。Prior same day — Mini expression 路径 `gis/style/eval/expression.*`（style 子目录 nest）。Prior same day — **§RHI2D public composition split**（`Device2d*` + thin `RenderDevice2d`，与 rhi3d 同形；无 `Map2dSession`；[`rhi2d-device-api.html`](../diagrams/rhi2d-device-api.html)）。**§Scenic**：copy 制图消费 `gis::style::ResolvedPaint`。**§rhi3d public API composition**（[`rhi3d-public-api-lanes.html`](../diagrams/rhi3d-public-api-lanes.html)）。copy TUs 去 Smt 前缀 + 零 `#include "legacy/…"`。**§Vista subdirectory tighten P0–P3 landed**。Prior 2026-10-03 — Scene3d GPU present 终态：`graph::present` 1 CL（sky/depth → opaque DEM → ocean → post）· §Map2d present：**MapFrame 双出口**（制图一次在 `Layout::build`；GDI 只消费 `DrawItem`）· [`map2d-present-frame.html`](../diagrams/map2d-present-frame.html) · §src_render Scene3d equal-profile：**world3d 矩阵 + cold vs warm**（P0 cold upload；`GPUSCENE_PREP_PARALLEL` default-off） · §Vista Map2d equal-profile **P0–P3 总方案** · §GPU-process × §vista parallel **统一规范图** [`render-accelerate-topology.html`](../diagrams/render-accelerate-topology.html)（A×B + Scene3d cold） · …
**Related:** model/compute · atmosphere · map2d folded into this file (§Folded topics); legacy present SP2 in [`2026-09-19-legacy-deep-abstraction-umbrella-design.md`](2026-09-19-legacy-deep-abstraction-umbrella-design.md)；Views shell [`2026-09-27-views-desktop-shell-design.md`](2026-09-27-views-desktop-shell-design.md)；as-built [`../../../src/render/README.md`](../../../src/render/README.md)、[`../../../src/gpu/README.md`](../../../src/gpu/README.md)；multiprocess [`../ui-shell-multiprocess.md`](../ui-shell-multiprocess.md)；RHI subdir landed [`../archive/plans/2026-09-27-rhi-subdirectory-split.md`](../archive/plans/2026-09-27-rhi-subdirectory-split.md)。  
**Diagrams:** **Vista scene octree** [`../diagrams/vista-scene-octree.html`](../diagrams/vista-scene-octree.html) · **Vista map/frame scenic-peer** [`../diagrams/vista-map-frame-scenic-peer.html`](../diagrams/vista-map-frame-scenic-peer.html) · **DEM / hillshade bake** [`../diagrams/hillshade-bake-profile.html`](../diagrams/hillshade-bake-profile.html) · **Scene3d present** [`../diagrams/scene3d-present-layers.html`](../diagrams/scene3d-present-layers.html) · **content present accelerate** [`../diagrams/content-present-accelerate.html`](../diagrams/content-present-accelerate.html) · **rhi2d frame-beat + map/layer/feature** [`../diagrams/rhi2d-paint-map-strategy.html`](../diagrams/rhi2d-paint-map-strategy.html) · **rhi3d public lanes** [`../diagrams/rhi3d-public-api-lanes.html`](../diagrams/rhi3d-public-api-lanes.html) · **Vista 子目录分层** [`../diagrams/vista-subdirectory-layers.html`](../diagrams/vista-subdirectory-layers.html) · **Scenic** [`../diagrams/legacy-render-architecture.html`](../diagrams/legacy-render-architecture.html) · **RHI2D composition split** [`../diagrams/rhi2d-device-api.html`](../diagrams/rhi2d-device-api.html) · **Map2d present** [`../diagrams/map2d-present-frame.html`](../diagrams/map2d-present-frame.html) · **A×B 规范图** [`../diagrams/render-accelerate-topology.html`](../diagrams/render-accelerate-topology.html)（含 **§8 Scene3d cold vs warm**） · Views shell [`../diagrams/ui-views-shell-architecture.html`](../diagrams/ui-views-shell-architecture.html) · GIS MapFrame [`../diagrams/gis-vista-architecture.html`](../diagrams/gis-vista-architecture.html)  
**Plans:** RHI scene [`../plans/2026-09-13-render-rhi-scene.md`](../plans/2026-09-13-render-rhi-scene.md) · frame graph [`../plans/2026-09-27-render-frame-graph.md`](../plans/2026-09-27-render-frame-graph.md) · **gpu accelerate（§GPU-process 唯一 checklist）** [`../plans/2026-09-27-gpu-rhi-accelerate.md`](../plans/2026-09-27-gpu-rhi-accelerate.md) · **vista parallel** [`../plans/2026-10-02-src-render-vista-parallel-accelerate.md`](../plans/2026-10-02-src-render-vista-parallel-accelerate.md) · P0 [`../plans/2026-09-20-rhi-3d-capability-p0.md`](../plans/2026-09-20-rhi-3d-capability-p0.md) · suite/bench [`../plans/2026-09-28-render-rhi-suite-bench.md`](../plans/2026-09-28-render-rhi-suite-bench.md) · **Map2d hillshade + line casing** [`../plans/2026-09-30-map2d-hillshade-line-casing.md`](../plans/2026-09-30-map2d-hillshade-line-casing.md) · **world3d pointcloud LAS** [`../plans/2026-09-30-world3d-pointcloud-las.md`](../plans/2026-09-30-world3d-pointcloud-las.md) · **D3D leftover capability** [`../plans/2026-09-29-d3d-leftover-capability.md`](../plans/2026-09-29-d3d-leftover-capability.md) · **GL leftover capability** [`../plans/2026-09-29-gl-leftover-capability.md`](../plans/2026-09-29-gl-leftover-capability.md) · GDI leftover worker [`../plans/2026-09-29-gdi-leftover-worker.md`](../plans/2026-09-29-gdi-leftover-worker.md) · GDI carto math [`../plans/2026-09-29-gdi-carto-base-math.md`](../plans/2026-09-29-gdi-carto-base-math.md) · GDI layout/compose [`../plans/2026-09-29-gdi-layout-device-compose.md`](../plans/2026-09-29-gdi-layout-device-compose.md) · GDI profile [`../plans/2026-09-29-gdi-leftover-profile.md`](../plans/2026-09-29-gdi-leftover-profile.md) · **GDI internal RHI** [`../plans/2026-09-29-gdi-internal-rhi-reshape.md`](../plans/2026-09-29-gdi-internal-rhi-reshape.md) · **Legacy Pipeline+Arena** [`../plans/2026-09-30-legacy-render-pipeline-arena.md`](../plans/2026-09-30-legacy-render-pipeline-arena.md) · **rhi2d Chromium-cc** [`../plans/2026-10-01-rhi2d-chromium-cc-compose.md`](../archive/plans/2026-10-01-rhi2d-chromium-cc-compose.md) · **rhi2d leftover tile-raster** [`../plans/2026-10-01-rhi2d-leftover-tile-raster.md`](../plans/2026-10-01-rhi2d-leftover-tile-raster.md) · **rhi3d leftover parallel frame** [`../plans/2026-10-01-rhi3d-parallel-frame.md`](../archive/plans/2026-10-01-rhi3d-parallel-frame.md) · **rhi3d Eigen frustum** [`../plans/2026-10-01-rhi3d-eigen-base-math.md`](../archive/plans/2026-10-01-rhi3d-eigen-base-math.md)。  
**Scope:** Living RHI + dual scene + in-process frame graph + GPU-process compose. FlyCube DX12/Vulkan. Logical world in `gis`/`sdb`; GPU cache in `render/scene`. Do **not** open new dated RHI/layout twins — revise sections below.

## Goal

One `RenderDevice` path draws **2D maps and 3D worlds** through the same command-list RHI. GPU work is FlyCube (DirectX 12 and Vulkan). Logical models and the GIS world live in `sdb`; `render` only owns GPU resources and a synced render scene. Leftover `RenderDevice2d` / GDI / GL stay as adapters so MFC views keep compiling.

## Non-goals

- Do not vendor Cesium Native, OpenSceneGraph, Filament, Diligent, bgfx, or a second GDAL/GEOS.
- Do not leak FlyCube, Assimp, or tinygltf types in public headers under `src/`.
- Do not treat Skia as the **FlyCube** map RHI (`render::rhi`). Leftover `SmtSkiaRenderDevice` (`legacy_rhi2d_skia.dll`, CPU `SkCanvas` / bootstrap) is an `RenderDevice2d` paint lane only; chrome Skia stays in `ui/gfx`.
- Do not revive D3D9 / D3DX (`src/render/d3d` was deleted).
- Do not rewrite leftover `Smt_*` ABI or merge DLLs. `RenderDevice2d::Init(HWND)` remains the MFC present seam.
- Do not put logical scene graphs back under `render/scene3d` / `render/model3d`.
- Qt is banned.
- v1 does not require implicit 3D Tiles, Draco, i3dm, pnts, or cmpt.

## Decisions (locked)

| Topic | Choice |
| --- | --- |
| Spec shape | One spec: RHI + model/scene relocation |
| 2D + 3D | One RHI: both record FlyCube command lists. GDI/GL are leftover adapters, not a second CommandList rasterizer |
| FlyCube boundary | Public Facade `render::rhi`. FlyCube only in `src/render/rhi` implementation TUs |
| Windows GPU backends | v1 ships **DX12 and Vulkan** (FlyCube). Default present path is DX12 |
| Scene | Dual: `sdb::scene::World` (GIS + spatial query) + `render::scene::GpuScene` (GPU instances / cull) |
| Model I/O | `sdb::model` via **Assimp** (OBJ/FBX/DAE/glTF files) |
| 3D Tiles | v1 streams explicit `tileset.json` (REPLACE/ADD). Tile content is glTF / b3dm via tinygltf, not Assimp |
| Spatial query | World-side AABB index (linear v1; libspatialindex later). GPU BVH/frustum in `GpuScene` |
| Leftover devices | `RenderDevice2d` + GDI/GL wrap the Facade present; they do not grow a second 3D engine |
| Scenic | Product previous-gen engine. Host = **`src/content`**. Disk **`src/scenic`** (peer of `src/vista`). Public DLL **`scenic.dll`**, namespace **`scenic`**. Leftover **`src/legacy/render` frozen**. Not a FlyCube `Device` port. Not merged into `vista.dll`. |

## Architecture

```
content::MapView / gpu process
        |
        v
render::rhi::Device  (Facade: Device / CommandList / Resource)
        |
        +-- FlyCube  (DX12 | Vulkan)     default GPU
        +-- Null                         tests
        +-- GDI / GL leftover            RenderDevice2d::Init
        |
        v
render::scene::GpuScene   <----sync generation----  sdb::scene::World
        |                                              |
        | GPU mesh / texture / instance                +-- Map layers (vector/raster)
        | frustum / GPU occupancy                      +-- sdb::model::ModelAsset (Assimp)
        | record CommandList                           +-- sdb::model::Tileset (3D Tiles)
                                                       +-- AABB query / pick
```

Dependency direction is one-way: `sdb` does not include `render/rhi`. `render/scene` depends on `sdb/scene` + `render/rhi`. `content` and `gpu` talk to `render::rhi` and `content::MapView` only.

## Tree and namespaces

Nesting stays `src/<layer>/<module>`. Public C++ is two levels; helpers go in `detail` or an anonymous namespace.

| Tree | Namespace | GN | Role |
| --- | --- | --- | --- |
| `src/render/rhi/` | `render::rhi` | part of `//src/render:render` | Facade (`rhi.h`/`rhi.cc`) + `stub/stub_device.cc` + `flycube/{device,resources,command_list,pipelines,execute,compute}.cc` + internal `flycube_*.h` |
| `third_party/flycube` | (private) | `//third_party:flycube` when fetched | DX12 / Vulkan / Metal |
| `src/sdb/model/` | `sdb::model` | `//src/sdb/model:model` | Assimp CPU assets + 3D Tiles |
| `src/sdb/scene/` | `sdb::scene` | `//src/sdb/scene:scene` | World, nodes, spatial query |
| `src/render/scene/` | `render::scene` | `//src/render/scene:scene` | GPU scene cache |
| `src/render/{gdi,gl,gdi_simple}` | leftover `Smt_Rd` | existing DLLs | HWND adapters |
| `src/render/{scene3d,model3d,terrain,pointcloud,render3d}` | leftover `Smt_*` | existing DLLs | 2010 engines; not the new default path |

New modules are **source_sets**, not new DLLs. They join `//src/sdb` / `//src/render:render_all` and therefore `src_all`.

## RHI surface

Public header: `"render/rhi/rhi.h"`. Types are named to match a modern engine RHI (Unreal / FlyCube-shaped) without including FlyCube.

```cpp
namespace render {
namespace rhi {

enum class Backend {
  kNull,
  kDx12,
  kVulkan,
  kGdi,  // leftover
  kGl,   // leftover
};

enum class CommandListType { kGraphics, kCompute, kCopy };

struct DeviceDesc {
  void* native_window;  // HWND on Windows
  uint32_t width;
  uint32_t height;
};

struct RenderPassDesc {
  float clear_r, clear_g, clear_b, clear_b_alpha;
  uint32_t width;
  uint32_t height;
};

class CommandList {
 public:
  virtual ~CommandList() = default;
  virtual void set_viewport(float x, float y, float w, float h, float min_depth,
                             float max_depth) = 0;
  virtual void begin_render_pass(const RenderPassDesc& desc) = 0;
  virtual void end_render_pass() = 0;
  virtual void bind_vertex_buffer(Buffer* buffer, uint32_t offset,
                                  uint32_t stride) = 0;
  virtual void bind_index_buffer(Buffer* buffer, uint32_t offset) = 0;
  virtual void draw_indexed(uint32_t index_count, uint32_t instance_count,
                             uint32_t first_index, int32_t vertex_offset,
                             uint32_t first_instance) = 0;
  virtual void close() = 0;
};

class Device {
 public:
  virtual ~Device() = default;
  virtual bool initialize(const DeviceDesc& desc) = 0;
  virtual void shutdown() = 0;
  virtual void present() = 0;
  virtual Backend backend() const = 0;
  virtual CommandList* create_command_list() = 0;
  virtual void destroy_command_list(CommandList* list) = 0;
  virtual bool execute(CommandList* list) = 0;
  virtual Buffer* create_buffer(uint32_t byte_size, BufferUsage usage) = 0;
  virtual void destroy_buffer(Buffer* buffer) = 0;
  virtual bool upload(Buffer* buffer, const void* data, uint32_t byte_size) = 0;
};

Device* create_device(Backend backend);
Backend preferred_gpu_backend();  // Windows: kDx12

}  // namespace rhi
}  // namespace render
```

- `create_device(kDx12)` and `create_device(kVulkan)` always return a heap `Device`. If FlyCube is not compiled in, `initialize()` returns false and `backend()` still reports the requested API.
- `create_device(kNull)` always initializes. Command lists record counters for tests (`StubCommandList`).
- `create_device(kGdi)` / `kGl` keep today’s HWND present (`InvalidateRect` / no-op). Their command lists are stubs so 2D leftover views do not crash if something records a pass.
- Leftover GDI/GL/D3D `Init(HWND)` present is owned by BitBlt / SwapBuffers / D3D11 Present. The former process-wide `leftover_session` / `bind_rhi_present` bridge was removed (2026-10-01). New code (`gpu`, Views map viewport) uses `preferred_gpu_backend()`.
- FlyCube headers appear only under `rhi/flycube/` (internal split: `flycube_{types,resources,command_list,shaders,device}.h` + impl TUs). `flycube_device.h` forward-declares command-list / Buffer / Texture; full types stay in the dedicated headers. Mapping: `Device` → FlyCube `Device` + `Swapchain`; `CommandList` → FlyCube `CommandList`; `execute` → `CommandQueue::ExecuteCommandLists`; `present` → `Swapchain::Present`.

Public RHI now includes `Buffer` + `create_buffer` / `upload` / `bind_vertex_buffer` / `bind_index_buffer`, `Texture` + `create_texture` / `upload_texture` / `bind_texture` for raster/tile quads, and `bind_camera` / `CameraMatrices`. FlyCube types stay out of `rhi.h`. When `HAS_FLYCUBE` is on (Debug compiles `/MDd` FlyCube into `out/flycube`; Release links the MD prebuilt), initialized devices upload to FlyCube heaps, bind view/proj constants, sample uploaded textures in a DX12 pipeline, and can clear/present a swapchain. Null-path tests stay CPU stubs (`bind_texture` / `bind_camera` counters). Optional: `rhi_test` initializes DX12 on a hidden HWND and skips (does not fail) when the machine has no adapter. Leftover GDI/GL/D3D present stays on BitBlt / SwapBuffers / D3D11 Present — the process-wide `leftover_session` / `LeftoverRecorder` bridge was removed.

## Model (`sdb::model`)

CPU-only. No GPU types, no `render` includes.

**Assimp (standalone files).** `load_file(const char* path, ModelAsset& out)` uses Assimp when `has_assimp` is true. Supported product formats: OBJ, FBX, DAE, glTF/GLB. On failure, return false and leave `out` empty. When Assimp is not linked, `load_file` returns false except for the built-in name `"cube"` which fills a unit cube (`load_unit_cube`).

```cpp
namespace sdb {
namespace model {

struct Mesh {
  std::vector<float> positions;  // x,y,z triples
  std::vector<uint32_t> indices;
};

struct ModelAsset {
  std::string name;
  std::vector<Mesh> meshes;
};

void load_unit_cube(ModelAsset& out);
bool load_file(const char* path, ModelAsset& out);

}  // namespace model
}  // namespace sdb
```

**3D Tiles (streaming).** Explicit tileset 1.0/1.1 only: `asset`, `root` with `boundingVolume` (`box` or `region`), `geometricError`, `refine` (`REPLACE` / `ADD`, default REPLACE), `content.uri`, `children`. No implicit tiling.

Tile content decode (when tinygltf is linked): `.gltf` / `.glb` and `.b3dm` (skip 28-byte b3dm header, then glTF). Without tinygltf, the streamer still **selects** tiles; `decode_content` returns false and World keeps the URI as an unloaded payload.

```cpp
namespace sdb {
namespace model {

enum class Refine { kReplace, kAdd };

struct Tile {
  double min_x, min_y, min_z, max_x, max_y, max_z;
  double geometric_error;
  Refine refine;
  std::string content_uri;
  std::vector<Tile> children;
};

struct Tileset {
  Tile root;
};

struct ViewState {
  double eye_x, eye_y, eye_z;
  double sse_denominator;  // viewport_h / (2 * tan(fovy/2))
};

bool parse_tileset_json(const char* json, size_t len, Tileset& out);
void select_tiles(const Tileset& tileset, const ViewState& view,
                  double max_sse, std::vector<const Tile*>& visible);

}  // namespace model
}  // namespace sdb
```

`select_tiles`: walk from root; if `geometric_error * sse_denominator / max(distance, 1e-6) <= max_sse` (or the tile has no children), emit the tile; else recurse. Distance is eye-to-AABB-center. REPLACE vs ADD only changes whether the parent is emitted when children are chosen: REPLACE omits the parent when any child is selected; ADD emits both.

## World (`sdb::scene`)

Logical scene. Owns node list + generation counter. Does not record GPU commands.

```cpp
namespace sdb {
namespace scene {

enum class NodeKind {
  kEmpty,
  kVectorLayer,
  kRasterLayer,
  kModel,
  kTileset,
  kTerrain,
  kPointCloud,
};

struct Node {
  uint64_t id;
  NodeKind kind;
  uint64_t generation;
  double min_x, min_y, min_z, max_x, max_y, max_z;
  std::string name;  // layer name, asset path, or tileset URI
};

class World {
 public:
  uint64_t generation() const;
  Node* add_node(NodeKind kind, const char* name, double min_x, double min_y,
                 double min_z, double max_x, double max_y, double max_z);
  bool remove_node(uint64_t id);
  Node* find(uint64_t id);
  size_t node_count() const;
  const Node* node_at(size_t index) const;

  // GIS: one node per SmtLayer in draw order. Does not copy features.
  void attach_map(const Smt_GIS::Map* map);

  void query_aabb(double min_x, double min_y, double min_z, double max_x,
                  double max_y, double max_z, std::vector<const Node*>& hits) const;

 private:
  uint64_t generation_;
  uint64_t next_id_;
  std::vector<Node> nodes_;
};

}  // namespace scene
}  // namespace sdb
```

- Mutating methods bump `generation_`.
- `attach_map` replaces all `kVectorLayer` / `kRasterLayer` nodes with one node per `Map` layer. Envelope comes from `SmtLayer::GetEnvelope` when present; otherwise a zero box. Terrain/pointcloud leftover engines are attached later as `kTerrain` / `kPointCloud` name handles; v1 does not port `Scene` octree code.
- `query_aabb` is inclusive AABB overlap (linear scan). This is the **identify / spatial filter** path. It is not the draw path.

## GPU scene (`render::scene`)

```cpp
namespace render {
namespace scene {

struct GpuInstance {
  uint64_t node_id;
  sdb::scene::NodeKind kind;
};

class GpuScene {
 public:
  uint64_t synced_generation() const;
  size_t instance_count() const;
  const GpuInstance* instance_at(size_t index) const;

  void sync_from(const sdb::scene::World& world);
  bool record(render::rhi::Device* device, render::rhi::CommandList* list,
              uint32_t width, uint32_t height);
  void release();

 private:
  uint64_t synced_generation_;
  std::vector<GpuInstance> instances_;
};

}  // namespace scene
}  // namespace render
```

- `sync_from` copies node ids/kinds/AABB when `world.generation() != synced_generation_`.
- `record(device, list, w, h)` tessellates real GIS geometry (`SmtGeometry` Point/LineString/Polygon via `attach_vector_geoms` / `OGRLayer` via `attach_map`, and `Smt3DGeometry` / `Smt3DSurface` via `attach_3d_geometry`) into GPU meshes, then records **two passes on one CommandList** (2D then 3D): bind vertex+index, `draw_indexed` with those tessellation counts, `close`.
- Camera: `CommandList::bind_camera` takes `CameraMatrices` (column-major view + proj). `GpuScene` binds ortho for raster/vector (world AABB) and perspective for 3D models. Null records the bind; FlyCube uploads a constant buffer so present is not identity-only.

## Data flow

1. App/content opens `Map` and/or `load_file` / `parse_tileset_json`.
2. `World::attach_map` + `add_node(kModel|kTileset, …)`.
3. Camera tick: `select_tiles` updates which tileset nodes are considered loaded (generation bump when the visible set changes).
4. `GpuScene::sync_from(world)` in the GPU process.
5. `list = device->create_command_list(); gpu_scene.record(device, list, w, h); device->execute(list); device->present();`
6. Identify: `World::query_aabb` in sdb (CPU). Hit-test of GPU instances can be added later; v1 pick is World AABB.

## Error handling

- RHI: `initialize` / `execute` return false. No exceptions across DLL boundaries. Log via existing `SmtLog` when a leftover device is involved; new source_sets write to stderr in tests only.
- `load_file`: false on missing file, unsupported extension, or Assimp missing. Never throw.
- `parse_tileset_json`: false on missing `root`, non-object JSON, or unreadable bounding volume. Partial trees are not returned.
- `World::attach_map(nullptr)` is a no-op (generation unchanged).
- FlyCube missing: DX12/Vulkan devices exist but `initialize` is false. Callers fall back to `kNull` in tests and GDI leftover in MFC.

## Testing

Register on `//:test_all`. Style matches `sde_gdal_test` (`expect` + `main`, no gtest until googletest is fetched).

| Test | Always (no GPU / no Assimp) |
| --- | --- |
| `rhi_test` | Null device inits; command list begin/draw_indexed/close/execute; `preferred_gpu_backend()` is `kDx12` on Windows; `create_device(kDx12)` and `kVulkan` return non-null |
| `model_test` | `load_unit_cube` has 8 verts / 36 indices; `load_file("missing.obj")` is false; parse a minimal tileset JSON; `select_tiles` with huge SSE returns only root; small SSE walks children |
| `scene_test` | add/remove nodes bumps generation; AABB query hits overlap only; `attach_map` on empty map yields zero layer nodes |
| `scene_gpu_test` | sync copies two nodes; second sync with unchanged generation is a no-op; dummy nodes (no GIS geom) record an empty pass and close |
| `unified_draw_test` | real `SmtPoint`+`SmtLineString`+`SmtPolygon` plus `Smt3DSurface` on one list; `draw_indexed_calls >= 2`; index counts match tessellation (2D ≥ 12, 3D = 6) |

Optional: if FlyCube is linked, `rhi_test` tries `initialize` on a hidden HWND and skips (does not fail) when the machine has no DX12/Vulkan device.

## Build / third_party

- `third_party/manifest.json` pins `flycube` (andrejnau/FlyCube), `assimp`, and `tinygltf`. Fetch via existing `build.bat t` / `fetch.py`. Do not vendor Chromium or a second copy inside `src/`.
- GN args (in `build/smartgis.gni`): `has_flycube` / `has_assimp` / `has_tinygltf`, default false until the source dir exists. Stub TUs always compile.
- C++23 (`cc_std`). FlyCube / Assimp keep their own CMake dialect; do not force `cc_std` onto those CMake trees unless they already inherit it.
- `src/render/d3d` was deleted (D3D9 / D3DX). Do not resurrect.

## Docs to update in the same change

- `docs/superpowers/src-layout.md` — RHI backends; `sdb/model`, `sdb/scene`; `render/scene` GPU cache; leftover `scene3d`/`model3d`.
- `src/README.md` — RHI v1 paragraph.
- Root `README.md` — one line that map/3D GPU is FlyCube RHI; refresh **最后更新**.
- `docs/README.md` — link this spec.

## Optional GPU tile basemap (StyleDocument; not MapLibre Native)

`src/gpu` can select a 2D basemap backend **below** `content/public`
(as-built: [`../../../src/gpu/README.md`](../../../src/gpu/README.md)):

- Default **direct** (`ContentSource::kDirect`): demo grid / Scene3d DEM underlay
  via `raster/direct` → `CompositorFrame` → `SoftwareRenderer` → `OutputSurface`.
- Optional **tile** (`MAP_BACKEND=a|track_a|maplibre` or `view.backend.maplibre`):
  StyleDocument + XYZ mosaic via `raster/tile` into the same compositor path
  (shared DXGI / DIB). Chrome still only presents the shared surface. Wire name
  `maplibre` means tile, **not** MapLibre Native.
- MapLibre Native product pin / `enable_maplibre` / `maplibre_link` were
  **removed** (2026-09-27; deferred reconsider). Do not include mln/mbgl from
  `app/`, `content/public`, or `gpu/`.
- **§MapLibre Native examples (2026-09-29):** opt-in only —
  `smt_enable_maplibre_example` + `//third_party/maplibre:maplibre_examples`
  builds upstream `mbgl-render` (headless) and `mbgl-glfw` via CMake
  (`third_party/maplibre/build_native.py`). Not a product pin; default off;
  as-built [`../../../third_party/maplibre/README.md`](../../../third_party/maplibre/README.md).
- **§MapLibre StyleDocument align (2026-09-29):** long-term capability
  alignment **without** Native product pin. Shared Style under
  `third_party/maplibre/example/style_align.json`; **data is the main-app
  china_city pack** (`out/data/china_city.gpkg` / `.geojson` via
  `//testing/data:china_map_samples`). GN copies Style to
  `$root_out_dir/maplibre/example/`. Product:
  `SmartGIS.exe --map2d-showcase=align` opens china_city + loads
  StyleDocument (framing 80–128°E / 20–48°N, 640×480). Optional Native still
  when `smt_enable_maplibre_example=true`. Dual stills:
  `python testing/tools/maplibre_align.py` → `out/*/maplibre/align/`.
  Wire name `maplibre` on tile backend remains StyleDocument tile, not Native.

## Risks

- FlyCube CMake + DX12/Vulkan SDK on the agent machine: stub path keeps `build.bat` green.
- Assimp FBX: large dependency; cube + `load_file` false is the green path until fetched.
- Dual scene drift: only `generation` is the sync key; callers must bump World when tile selection changes.
- Leftover `Smt3DRenderDevice` (GL immediate/matrix stack) is not the FlyCube path. 3D MFC views keep the 2010 engine until they switch to `GpuScene`.

## Success

- `build.bat` stays green with the new source_sets in `src_all`.
- `build.bat te` runs `rhi_test`, `model_test`, `scene_test`, `scene_gpu_test`.
- 2D layer nodes and 3D model nodes record into **one** `CommandList` via `GpuScene::record`.
- Public headers under `src/` do not `#include` FlyCube, Assimp, or tinygltf.
- `RenderDevice2d::Init` still compiles and still calls `BindRhiPresent`.

---

## §Generic pipeline（merged 2026-09-28）

**Supersedes** earlier `PipelineId` / `set_ocean_params` / `set_cloud_params` / `set_light_params` / `set_solid_color` command contract in this file.

| Locked | Choice |
| --- | --- |
| Pipeline identity | Heap `Pipeline*` from `create_graphics_pipeline` / `create_compute_pipeline` |
| Effect params | `set_constants(slot, bytes, size)` + `BindingSlot` |
| Camera | Keep `bind_camera`; optional `camera_slot` on graphics desc |
| Vertex layouts | `kPosition` / `kPositionUv` / `kPositionNormal` |
| No effect names | `rhi.h` has no ocean/cloud/light/FFT/solid symbols |
| Shared programs | solid / textured / lit under `src/render/programs/` |
| Null / GDI / GL | Stub pipelines when no FlyCube compile |

Non-goals: no second raster backend; no FlyCube types in public headers; atmosphere physics stays in atmosphere living. Archive: [`../archive/specs/2026-09-27-rhi-generic-pipeline-design.md`](../archive/specs/2026-09-27-rhi-generic-pipeline-design.md).

---

## §Frame graph（merged 2026-09-28）

One viewport, one camera, one `CommandList`, one `execute`/`present`. Passes only record.

| Layer | Home |
| --- | --- |
| Backend | `render::rhi` |
| Render Scene | `render::scene::GpuScene` (GPU cache only) |
| Frame graph | `render::graph` — `Effect` / slots / `present` / `OpaqueEffect` |
| Map / atmosphere GPU passes | `src/vista/component/map`, `src/vista/component/atmosphere` (`effect::*`); **not** compiled into `render.dll` deps of those passes |
| CPU frame | `vista` (`MapFrame`); present layering is §Map2d present |

`ViewInput` is width/height + one camera + non-owning `Effect*` list — does not name `GpuScene` / `MapFrame` / `AtmosphereFrame`. Archive detail: [`../archive/specs/2026-09-27-render-frame-graph-design.md`](../archive/specs/2026-09-27-render-frame-graph-design.md).

### Scene3d present spans（2026-09-28）

When `base::trace::tracing_enabled()`: `scene3d` / `scene3d.present` / `scene3d.mesh` / `scene3d.atmosphere` / `scene3d.gdi` wrap presenter + GPU + software paths (same `base::trace::process_trace` as Map2d). See base §Trace + views §RenderTrace.

### Scene3d legacy look（2026-09-30）

**Default product face** stays atmosphere (`Scene3dLookPreset::kAtmosphere` via `apply_china_scene3d_product_defaults`). **Opt-in** leftover stereo parity: `Scene3dLookPreset::kLegacyStereo` / `apply_china_scene3d_legacy_look` / `--atmosphere-showcase=legacy`.

| Locked | Choice |
| --- | --- |
| Clear | Black (`GpuScene` background paint) |
| Atmosphere | Ocean ON (light-blue sea); sky/cloud/fog OFF |
| Terrain | China DEM hypsometric bake (existing `seed_china_dem_into_world`) + China orbit distance 2.55 |
| Labels | Built-in major-city overlay (GDI outline paint); coast vectors when `MapScene` has china sample |
| Gate | Suite `atmosphere.legacy` + score_id `legacy_scene3d_china`; content marks `look-legacy` / `labels-ok` |
| Non-goal | Bridging leftover `StereoTerrain` HWND into Views; dual thick DEM |

Plan checkbox: [`../plans/2026-09-30-map3d-gap-pin.md`](../plans/2026-09-30-map3d-gap-pin.md) **P1-D**.

---

## §GPU-process accelerate（merged 2026-09-28 · topology fold 2026-10-02）

**Status:** active  
**Updated:** 2026-10-02 — fold Chromium-like **GPU process** compose with **§src_render + vista parallel**；**统一规范图** A×B  
**Plan (sole checklist):** [`../plans/2026-09-27-gpu-rhi-accelerate.md`](../plans/2026-09-27-gpu-rhi-accelerate.md) — includes **Task 8: Bridge to in-process L0–L3**  
**In-process peer:** [`../plans/2026-10-02-src-render-vista-parallel-accelerate.md`](../plans/2026-10-02-src-render-vista-parallel-accelerate.md)（L0–L3；不重复 GPU-process checkbox）  
**Diagram (normative):** [`../diagrams/render-accelerate-topology.html`](../diagrams/render-accelerate-topology.html)（§4–§6 Topology B + Bridge）  
**Related diagrams:** [`ui-views-shell-architecture.html`](../diagrams/ui-views-shell-architecture.html) · [`gis-vista-architecture.html`](../diagrams/gis-vista-architecture.html)  
**Code:** `src/gpu/**` · `src/gpu/device/gpu_device_hub.*` · `src/gpu/compositor/{frame,composer,underlay}/**` · `src/gpu/display/**` · `src/gpu/frame_sink.h` · `src/content/common/host_protocol.h`  
**As-built:** [`../../../src/gpu/README.md`](../../../src/gpu/README.md) · multiprocess [`../ui-shell-multiprocess.md`](../ui-shell-multiprocess.md)  
**Archive (superseded body):** [`../archive/specs/2026-09-27-gpu-rhi-accelerate-design.md`](../archive/specs/2026-09-27-gpu-rhi-accelerate-design.md)

### Normative (multiprocess on)

1. **Compose/present only in the GPU process** (`--type=gpu` / `src/gpu`). Browser / renderer / chrome / Views **never** blend final pixels.
2. **IR stays `CompositorFrame` / `DrawQuad` / `RenderPass`** (`src/gpu/compositor/frame/frame.h`). No second quad model.
3. **Topology:** **one** GPU process × **N** adapter device slots (`GpuDeviceHub` + `AdapterId`). Not N gpu processes. **No** single-frame multi-GPU split / cross-adapter mosaic.
4. Shell / Views consume **NT shared handles / DIB** only (`SharedHandle` + `FrameReady`).

### Two topologies (how they compose)

| Topology | When | Who owns final Device / present | CPU parallel (L2) | Final blend |
| --- | --- | --- | --- | --- |
| **A. In-process Views** | Single-process Views / Display mailbox (today’s Map2d/Scene3d product path) | **L3 Display** sole `rhi::Device` + `render::graph::present` (1 CL) | `vista` / `prep_cull_parallel` in browser process | In-process `graph::present` — **not** `src/gpu` `FrameComposer` |
| **B. Chromium-like GPU process** | Multiprocess shell (`--type=gpu` child) | **GPU process** `GpuDeviceHub` → per-adapter `FrameComposer` → `OutputSurface` | Same L0–L2 **semantics**; record may stay in gpu near-term or move to content → IR IPC | **Only** `FrameComposer::draw_frame` inside `--type=gpu` |

**Composition rule (locked):**

- L0–L2 （UI never joins · content schedule · vista / GpuScene prep）are **topology-agnostic**.
- **L3 splits by mode:**
  - **A:** `record_and_present` ≡ `render::graph::present` on Display’s Device.
  - **B:** map underlay / graph / effect output becomes **submit to GPU process** pinned by `AdapterId` (`draw_and_swap` / later serialized `CompositorFrame`); UI still never joins; chrome still never blends.
- Do **not** run both A and B final-present on the **same HWND** (no leftover FlyCube HWND + gpu shared surface dual-present).

```
Topology A (in-process Views):
  L0 stage → L1 schedule → L2 parallel_for → L3 Display Device → graph::present

Topology B (--type=gpu):
  L0/L1 submit IR / DrawRequest → [GPU process] raster→CompositorFrame
       → GpuDeviceHub(AdapterId) → FrameComposer → OutputSurface
       → SharedHandle → Browser present-only
  optional: underlay record_gpu_scene_underlay / record_underlay_effects (same AdapterId)
```

### Relation to §src_render + vista parallel (L0–L3)

| Layer | Topology A (Views) | Topology B (GPU process) |
| --- | --- | --- |
| L0 UI | Views Commit / Display mailbox; never join | Same; **present SharedHandle only** |
| L1 Schedule | `Map2dFrameCache` / Scene3d presenters | content / renderer submit DrawRequest (or future CF IR); gen/cancel still apply |
| L2 CPU | `Layout::build` / `prep_cull_parallel` | Same CPU work owner; **must not** touch gpu `FrameComposer` |
| L3 GPU | Display `rhi::Device` + `graph::present` | **Remap:** hub `ensure_rhi_device(AdapterId)` + `make_frame_composer` + `OutputSurface`; underlay via `compositor/underlay/underlay_bridge.*` |

Warm StaticReuse / equal-profile budgets stay owned by §vista / equal-profile §§; this § owns **process boundary + hub + IR**.

### Code-level anchors (as-built)

| Symbol / file | Role |
| --- | --- |
| `gpu::draw_and_swap` (`frame_sink.h` / `display/display.cc`) | Sole public paint entry |
| `gpu::detail::CompositorFrame` / `DrawQuad` / `RenderPass` | IR |
| `gpu::detail::GpuDeviceHub` | 1 process × N slots; bind/rebind; sticky software; texture cache; underlay Effects |
| `gpu::detail::FrameComposer` + `select_compose_backend` / `make_frame_composer` | `kRhi` default · `kSoftware` escape |
| `gpu::detail::RhiComposer` / `SoftwareComposer` | Per-adapter compose/present |
| `gpu::detail::OutputSurface` | DXGI shared / DIB @ `AdapterId` |
| `record_underlay_effects` / `record_gpu_scene_underlay` / `record_hub_underlay` | Frame Graph / `vista::GpuScene` underlay — **no** `OutputSurface` in graph |
| `AttachSurfaceBody` / `ResizeSurfaceBody` LUID fields | Monitor affinity without `HMONITOR` on wire |

### Bridge milestones (name real symbols)

| Milestone | Meaning | Status (as-built) |
| --- | --- | --- |
| Software compose fallback | `SoftwareComposer` + `upload_bgra` per sticky adapter | landed |
| RHI blit / compose → DXGI shared | `RhiComposer` · `import_shared_nt_handle` · `execute_to_imported` / `copy_bgra_to_imported_shared` | landed (FlyCube when `HAS_FLYCUBE`) |
| GPU compose quads | `kSolid` / `kBgra` / `replaces` on RHI path | landed |
| Texture cache | `GpuDeviceHub` per-slot cache + `DrawQuad::texture_cache_key` | landed |
| Frame Graph / GpuScene underlay | `underlay_bridge` record; BGRA readback / effect::map color-target copy **open** | partial |
| L3 → submit remap (Views multiproc) | Single-process A today; B path when shell spawns `--type=gpu` — **Task 8** | open |

### A+C locked decisions (2026-09-28)

| Topic | Choice |
| --- | --- |
| Default `ComposeBackend` | **`kRhi`**. Unset / empty / unknown `GPU_COMPOSE` → `kRhi`. Explicit `GPU_COMPOSE=software` is the escape hatch (case-insensitive). Test override APIs remain. |
| Sticky fallback | On hard RHI / present failure for adapter A: sticky **software** for **that adapter only** (`GpuDeviceHub` sticky bit); other adapters keep RHI when healthy. |
| Topology | **One** `--type=gpu` process × **N** adapter device slots. Not N gpu processes. **No** single-frame multi-GPU split / cross-adapter mosaic in one compose. |
| Shell role | Shell consumes NT shared handles / DIB only — **never** blends the final frame. |
| Monitor affinity | `AttachSurfaceBody` / `ResizeSurfaceBody` carry `monitor_luid_low` / `monitor_luid_high` (DXGI adapter LUID of the output’s monitor). Optional `adapter_hint` (`0xffffffff` = unset). `gpu_main` binds / rebinds the `OutputSurface` via `GpuDeviceHub` (`prefer_adapter_for_monitor` / `rebind_surface_to_monitor` by LUID). **`HMONITOR` is not sent over IPC** (shell-local only when resolving LUID). |

### Env & wire flags（accurate）

| Flag / env | Meaning |
| --- | --- |
| `GPU_COMPOSE` | unset/empty/unknown → **`kRhi`**; `software` → `kSoftware` (escape). Parsed in `select_compose_backend` (`compositor/composer/composer.cc`). |
| `view.backend.rhi` | Wire → `ContentSource::kDirect` only — **not** FlyCube / not `ComposeBackend`. |
| `view.backend.maplibre` / `MAP_BACKEND=…maplibre` | Tile StyleDocument path — **not** MapLibre Native. |
| `MAP_BACKEND` / `XYZ_URL` | Content source / XYZ hand-test (see `src/gpu/README.md`). |
| `--type=gpu` / `--in-process-gpu` | Process model (`ui-shell-multiprocess.md`); in-process-gpu debug/CI only. |
| Vista parallel envs | `VISTA_LAYOUT_PARALLEL` / `GPUSCENE_PREP_PARALLEL` — **§vista**; do not overload as compose backend. |

### Non-goals（硬）

- No Skia Ganesh map path for compose
- No leftover HWND FlyCube + gpu shared surface on the **same HWND**
- No wholesale Chromium Mojo viz / Blink `cc` clone
- No chrome / `app/` / `content/` `#include` of `render/rhi` or `gpu/compositor/`
- No second compositor IR beside `CompositorFrame`
- No N gpu processes per adapter by default

### Legacy GDI buffer → compositor IR（2026-09-29）

**Updated:** 2026-10-01 — surface/ thinned to dib/ + composer/ (no blend/frame twin dirs; no Rhi2dComposer class).

src/legacy/render/rhi2d/impl/common/surface/ is map2d **compose + present**, not a generic byte arena.

| Locked | Choice |
| --- | --- |
| Public ABI | Rhi2dSurface + Rhi2dOwnedSurface + pool helpers; no SmtRenderBuf; POINT scratch uses `base::tls_allocate` / `tls_deallocate` (no SmtBufPool) |
| Layout | surface/dib/ (DIB + pool + owned lifecycle) · surface/composer/ (soft blit · HWND present · buf→buf · IR submit) |
| Phase 1 | In-process: DIBSection BGRA + gpu::detail::blend_render_pass via narrow //src/gpu:compositor_cpu_blend (no full gpu_backend) |
| HWND present | detail::present_to_hwnd → GDI BitBlt / TransparentBlt / StretchBlt |
| buf→buf compose | detail::blit_owned_to / blit_surfaces → color-key→alpha then src-over; **stretch via nearest-neighbor** then blend |
| Blit mode | Rhi2dBlitMode::{kOpaque,kColorKey} (replaces eSwapType / GdiBlitMode) |
| Surface pool | rhi2d_surface_pool() reuses DIB by size; draw TUs allocate POINT scratch from TLS arena |
| Phase 2 | make_compositor_frame + submit_surface; C ABI Rhi2dSetBgraSubmit; GPU process binds → OutputSurface. Shell still must not final-compose. |
| Phase 3 | GPU leftover_gdi_bgra_upload builds CompositorFrame → make_frame_composer (RHI/software) → draw_frame; NN-scale when sizes differ. Test: gdi_compose_test. |

### Phases

| Phase | Scope | Status |
| --- | --- | --- |
| **P0** | Default compose = RHI; `GPU_COMPOSE=software` escape; sticky per-adapter software fallback; docs/tests match. | mostly landed |
| **P1** | LUID fields on Attach/Resize IPC; shell fills LUID; `gpu_main` rebinds via hub (not primary-only forever). | code landed; LUID pin tests open |
| **P2** | TDR / device-lost → sticky software + generation bump wired from recovery path; dual-adapter notes. | API landed; OS TDR wire open |
| **Bridge** | Topology A↔B remap + underlay BGRA / effect::map copy — Task 8 on plan | open |

### Acceptance

- [x] Living § locks two topologies + IR + hub × N + non-goals
- [x] Normative HTML diagram（A×B 整合 [`render-accelerate-topology.html`](../diagrams/render-accelerate-topology.html)）
- [ ] Task 8 bridge checkboxes green (Views multiproc L3 submit)
- [ ] Human multiprocess smoke: chrome present-only; kill GPU child recovers
- [ ] Dual-adapter sticky fallback matches as-built README

---

## §P0 3D capability（merged 2026-09-28）

Lit solid path, style→3D albedo, CPU frustum, GPU smoke — checklist in [`../plans/2026-09-20-rhi-3d-capability-p0.md`](../plans/2026-09-20-rhi-3d-capability-p0.md). Pipeline entry is via §Generic pipeline (`Pipeline*` + constants), not `PipelineId::{kOcean,kCloud}`. Archive: [`../archive/specs/2026-09-20-rhi-3d-capability-p0-design.md`](../archive/specs/2026-09-20-rhi-3d-capability-p0-design.md).

---

## §Content `present/scene3d` slim（2026-09-28）

**Diagram:** [`../diagrams/scene3d-present-layers.html`](../diagrams/scene3d-present-layers.html)

Thin facade, same seam as `present/map2d`. `Scene3dGpuPresent::present` fills viewport, look, atmosphere toggles, the orbit camera, and `render::graph::ViewInput`, then calls `render::graph::present` (one CommandList). Overlay buffers and look labels live in `frame/`; Scenic `Engine` lives in `scenic_engine_host` (not on the facade).

| Stays in present | Role |
| --- | --- |
| `Scene3dPresenter` | bind / dispatch GPU vs software vs scenic |
| `scenic_engine_host` | content-hosted `scenic::Engine` (`SCENE3D_ENGINE=scenic`) |
| `session/scene3d_rhi_session` | FlyCube prefer / engine source of truth (`CONTENT_EXPORT` in `content.dll`) |
| `Scene3dStereoSession` | scenic_impl LoadLibrary HWND stereo |
| `scene3d_phase_profile` | phase clocks, including warm `rebuild_count` |
| `Scene3dSoftwarePainter` | HDC / HUD |
| `AtmosphereSession` | toggles and sim time; `globe_surface_loaded_` short-circuit; sun direction |
| `frame/` | `OrbitGeoFrame`, `rebuild_terrain_mesh`, `Scene3dOverlays`, `TilesetStreamSession`, look preset + China labels |
| `Scene3dGpuPresent` | World / GpuScene / `graph::present` + present lock |
| sea mask | `seed_sea_mask_from_dem` |

China box, LOD cache key, and globe height sampling are not content policy. `dem_seed_lonlat_box` / `dem_seed_cache_key` sit beside `seed_dem_view_tiles_into_world` and `seed_china_dem_into_world`. `DemRaster::sample_globe_surface` owns the sample loop and hypsometric fallback. `rebuild_local_mesh` still builds into a local vector, swaps, and holds `present_mu_`. A set `set_sample_dem_path_override` does not force the China box. Globe present skips `rebuild_local_mesh` and `GpuScene::sync_from`.

Albedo mean and the solid-terrain flag live on `vista::GpuScene` (`update_solid_terrain`, next to `set_solid_color` / `synced_generation()`). `rebuild_meshes` and `detail::record_kind` read that flag. One remesh is still ordered by present: `OceanPass::prepare_gpu` → `GpuScene::sync_from` → `mark_meshes_dirty` on the cold frame only. Warm frames do not `sync_from` again. `dem_gpu_synced_after_ocean_` / `dem_gpu_synced_after_sky_` stay on present. `GPUSCENE_PREP_PARALLEL` stays off.

Callers use `atmosphere_session()` / `gpu()` / `software()`. As-built index: [`../../../src/content/browser/present/README.md`](../../../src/content/browser/present/README.md).

---

## §RHI suite / coverage / bench（2026-09-28）

**Status:** accepted  
**Plan:** [`../plans/2026-09-28-render-rhi-suite-bench.md`](../plans/2026-09-28-render-rhi-suite-bench.md)

Headless functional matrix + performance benches for `render::rhi` / `graph`, plus Debug Console harness entry. Matches industry RHI sample surfaces (Device / Resource / Pipeline / CommandList / Present / Compute / Frame graph) without vendoring a second RHI.

| Locked | Choice |
| --- | --- |
| Layout | Shared scenarios in `src/render/testing/` (`render::detail`); product DLL does **not** depend on testing |
| Functional | `rhi_suite_test` (Null) → `test_shell` / `test_all`; keep existing `rhi_test` GPU smoke (`RUN_FLYCUBE_GPU=1`) |
| Bench CI | `rhi_bench` (Null, **google/benchmark**) → `benchmark_all` / `build.bat b` |
| Bench GPU | `rhi_gpu_bench` (DX12); **not** in default `benchmark_all`; env or explicit ninja target |
| Console | `:rhi test\|bench` spawn sibling exes (same pattern as `:gis test\|bench`) |
| Coverage Phase 1 | API/feature matrix checklist below (scenarios) |
| Coverage Phase 2 | OpenCppCoverage / MSVC report scoped to `src/render` (follow-up; does not block Phase 1) |
| google/benchmark | **Required** for all `benchmark()` targets via `testing/benchmark.gni` → `//third_party:gbenchmark` (+ `gbenchmark_main` by default). Install: `build.bat t benchmark`. |

### Scenario matrix (Phase 1)

| Scenario | Industry RHI analogue | Null | GPU (opt) |
| --- | --- | --- | --- |
| `device_lifecycle` | Device create / init / shutdown | required | required when adapter |
| `resources` | Buffer / Texture create + upload | required | required |
| `pipeline_bind` | Graphics pipeline + set_pipeline / constants | required | required |
| `record_present` | begin/end pass, draw, execute, present | required | required |
| `graph_present` | Frame graph four-slot present | required | optional |
| `compute_smoke` | Dispatch when `supports_compute` | skip-ok on Null | required when compute |

### Non-goals

- No second RHI / Skia-as-map-RHI / Qt Lab window.
- No mandatory true-GPU CI.
- No embedding FlyCube types in public headers.

---

## §GDI leftover worker（dual-track）（2026-09-29）

**Status:** accepted  
**Plan:** [`../plans/2026-09-29-gdi-leftover-worker.md`](../plans/2026-09-29-gdi-leftover-worker.md)  
**Code:** `src/legacy/render/rhi2d/impl/gdi/core/worker/` + `core/host/render_device.*` (+ helpers)

### Why it exists

`Rhi2dFrameWorker` is **not** a multi-core accelerator. It is a **single worker** that runs one full-map GDI `RenderMap` off the HWND thread so the message pump stays responsive during leftover 2D paint. Dual-track keeps this path alive until Views map present is fully on RHI / `software_composer`.

### Locked choices

| Topic | Choice |
| --- | --- |
| Approach | **Thin FrameJob shell** on the existing GDI device — do **not** merge into `gpu/` compositor during dual-track |
| Worker runtime | Dedicated **`base::execution::NThreadPoolExecutor(1)`** (serial GDI lane). Do **not** use `GlobalNThreadPoolExecutor` (overlapping GDI on the shared front is unsafe) |
| Ordering | **B → A → C**: stability → interaction → retireable surface |
| Speedup | Explicit **non-goal**. No claim of parallel GDI speedup |
| Draw* growth | Forbidden on `Rhi2dFrameWorker`; paint stays internal |
| New-track coupling | Do not block RHI / frame-graph work on this leftover shell |

### Phase B — stability

1. **Bounded idle wait:** UI paths must not use unbounded `while (IsRendering()) Sleep(0)`. Use `request_cancel` + `wait_idle(timeout_ms)`; on timeout keep the last good front frame.
2. **Shutdown:** Keep “no `join()` on HWND thread” (deadlock with GDI). `stop()` sets stop + cancel, notifies, detaches if needed; `has_exited` / leak-on-close stays explicit.
3. **Buffer ownership:** Worker paints only a **private back** (`m_smtRenderBuf`). Publish to the device **front** (`ShareBuf` / map buffer) only at frame end, and only if the job **generation** still matches. Device must not `ClearBuf` / `PrepareDC` the front while the worker is publishing.
4. **Stale / cancel:** Layer loop checks `m_stop` and per-frame cancel / generation so close and superseded zooms abandon the paint before touching HWND buffers.

### Phase A — interaction

1. Single submit path: pan/zoom/`ScheduleDelayedRedraw`/`ReRenderMapByProxy` only stage a pending job; `Timer` (or one tick) submits.
2. Debounce: keep ~200 ms settle; interactive frames are droppable; settle frame must present.
3. Present when front generation advances: Timer calls `Refresh()` only after the worker is **idle**, and only then advances the present baseline. Consuming the gen while `Refresh` early-outs on `is_busy()` left the HWND on the previous composite until a mouse-driven Refresh.
4. **`ReRenderMapRealTime`:** cancel → urgent FrameJob → return immediately. **Do not** Sleep-poll for publish on the UI thread. Timer owns present-on-gen. No white clear flash of map fronts.
5. **MapLibre-aligned preview:** `PreviewZoomScale` updates windowport **and** stretches `vir_viewport2` around the cursor (`paint/carto/frame/preview_xform.h`); pan keeps `SetCurDrawingOrg` BitBlt slide. Worker publish resets `vir_viewport1/2` to 1:1. `ReRenderMapByProxy` stages debounced settle without clearing the last-good front.

### Phase C — retireable API + thread composition

Public leftover surface on **`Rhi2dFrameWorker`** (was `SmtGdiRenderThread`) shrinks to snake_case:

`init` / `resize` / `stage_frame` / `submit_frame` / `cancel` / `wait_idle` / `shutdown` / `is_busy` / `has_pending` / gens / `has_exited`.

Device (`render_device.*`) updates call sites in the **same** change — **no** dual-name wrappers.

Internal composition under `impl/gdi/core/worker/` (`render::detail` worker lane):

| Collaborator | Role |
| --- | --- |
| `Rhi2dFrameScheduler` | `NThreadPoolExecutor(1)`, cancel/gen, coalesce, `paint_loop` |
| `GdiMapPainter` | `render_map` / layer / feature / geometry |
| `GdiStyleCanvas` | pen/brush/font + all draw primitives |

`frame_worker_draw.cpp` is deleted after the move. `SmtBufPool` already gone — POINT scratch is `base::tls_allocate`. Delete `gdi/thread` when Views map no longer binds GDI leftover present.

**Note:** §GDI layout rename + device composition owns UI-thread facade helpers and worker FrameJob paint **flat under `core/`** (distinct scheduler basenames). Do not duplicate Draw* into a second lane.

### Acceptance (dual-track)

- Close / `--self-test`: no hang, no worker UAF on Viewport.
- Continuous pan: UI remains interactive; blank/flash not worse than baseline.
- Optional render-trace: `job_gen`, cancel, present generation.

### Non-goals

- No multi-threaded GDI feature drawing **on the serial FrameJob shell** (shared front / one worker). **Exception:** §rhi2d leftover tile-raster allows parallel paint with **per-thread** private HDC+DIB only.
- No new carto capability on the leftover thread.
- No new dated design twin — revise this `§` only.

---

## §GDI layout rename + device composition（2026-09-29）

**Status:** accepted  
**Updated:** 2026-09-29 (`core/paint/` dedupe — see §GDI paint dedupe)  
**Plan:** [`../plans/2026-09-29-gdi-layout-device-compose.md`](../plans/2026-09-29-gdi-layout-device-compose.md)  
**Code:** `src/legacy/render/rhi2d/impl/gdi/` only (not the stale dual-run tree `src/legacy/render/gdi/` if still present)

### Locked choices

| Topic | Choice |
| --- | --- |
| File rename | Drop redundant `gdi_` file prefix under `impl/gdi/` (`render_device.*`, `compose.*`, `render_thread.*`, `aux_api.*`, `gdiplus.*`, …) |
| Types / ABI | Keep `SmtRhi2dRenderDevice` / `Rhi2dFrameWorker` / `CreateDevice("SmtRhi2dRenderDevice")` |
| Resources | Move `resource.h` + `.rc` + icons → `impl/gdi/res/` |
| Device split | Thin facade + composition under `core/{host,worker,paint,surface}` |
| Device collaborators | Host: `render_device` + `present_controller` + `buffer_image` (+ `device_{interact,pass,draw}`). Shared paint: see §GDI paint dedupe |
| Thread collaborators | FrameJob shell: `worker_frame_scheduler` / `render_thread`. Paint bodies under `core/paint/` |
| Subdir tighten | Top: `core/` · `gdiaux/` · `res/` · `test/`. Under `core/`: `host/` · `worker/` · `paint/` · `surface/` |
| Scope | No behavior change to FrameJob contract; Phase C API rename is owned by leftover-worker plan |

### Target layout

```
impl/gdi/
  core/
    host/     render_device.* + present_controller.* + buffer_image.*
              + device_interact.* + device_pass.* + device_draw.*
    worker/   render_thread.* + worker_frame_scheduler.*
    paint/    style_canvas.* + map_painter.* + map_carto2d.*
              + device_geom.h + render_context.h
    surface/  compose.*, surface_pool.*
  gdiaux/   aux_api.*, gdiplus.*
  res/      resource.h, render_device.rc, icons
  test/     compose_test.*, map_paint_test.*, map_carto2d_test.*
```

### Acceptance

- Includes / `BUILD.gn` point at new paths; no leftover `gdi_*.` sources under `impl/gdi/` (except intentional symbol names like `gdi_surface_pool()`).
- `SmtRhi2dRenderDevice` remains the only public device type; `.cpp` no longer hosts all Draw*/scheduler bodies.
- Top-level subdirs: `core/` · `gdiaux/` · `res/` · `test/`; under `core/`: `host/` · `worker/` · `paint/` · `surface/`.
- `gdi_compose_test` / `gdi_map_paint_test` / `legacy_render` still build.

### Non-goals

- Do not rename `SmtGdi*` or break CreateDevice string.
- Do not merge leftover into `gpu/` compositor in this change.
- Do not rewrite `src/legacy/render/gdi/` dual-run copies in the same wave unless that tree is deleted separately.

---

## §GDI paint dedupe（`core/paint/`）（2026-09-29）

**Status:** accepted  
**Updated:** 2026-09-29 (host wire + force-rebuild verify)  
**Plan:** [`../plans/2026-09-29-gdi-paint-dedupe.md`](../plans/2026-09-29-gdi-paint-dedupe.md)  
**Code:** `src/legacy/render/rhi2d/impl/gdi/core/{paint,host,worker}/`

### Why

Layout split left **two paint lanes**: host `GdiGeomDrawer` / `GdiStyleState` / `Rhi2dPainter` vs worker `GdiStyleCanvas` / `GdiMapPainter`. Living leftover-worker note already forbids a second Draw* lane.

### Locked choices

| Topic | Choice |
| --- | --- |
| Approach | Neutral **`core/paint/`**; host + worker each own an instance (share code, not DC/objects) |
| UI sync paint | **Keep** BeginRender path + realtime fallback |
| Wave | One wave: Draw* + style + layer orchestration |
| Host buffers | Sync full-map uses host painter with separate back (e.g. quick) + shared map front |
| ABI | Public `Draw*` / `Render*` / CreateDevice unchanged; delete host duplicate TUs |
| `style()` | Replace with host `canvas()` / field accessors — no second pen/brush implementation |

### Acceptance

- No `host/geom_drawer.*` / `style_state.*` / `layer_painter.*`.
- FrameJob cancel/gen/publish contract unchanged.
- `map_carto2d_test` / `gdi_compose_test` / `gdi_map_paint_test` / `legacy_render` green.
- No new dated design twin — this `§` only.

### Non-goals

- Do not kill UI-thread sync paint in this wave.
- Do not share one canvas instance across HWND + worker threads.
- Do not wire into `gpu/` compositor.

---

## §GDI core cc rename（Chromium lexicon）（2026-09-29）

**Status:** accepted  
**Updated:** 2026-09-29 (rename verified green)  
**Plan:** [`../plans/2026-09-29-gdi-core-cc-rename.md`](../plans/2026-09-29-gdi-core-cc-rename.md)  
**Code:** `src/legacy/render/rhi2d/impl/gdi/core/`

### Why

After paint dedupe, leftover still had dual scheduler vocabulary (`GdiUiController` vs worker lane) plus Hungarian `m_smt*` members and paint type names that did not match Chromium/cc. Worker lane is now `Rhi2dFrameWorker` + `detail::Rhi2dFrameScheduler`. `RenderDevice2d` Draw* ABI stays.

### Locked choices

| Topic | Choice |
| --- | --- |
| Lexicon | Chromium/cc-style names inside `impl/gdi/core` |
| Directories | Keep `host/` · `paint/` · `worker/` · `surface/` |
| ABI | `RenderDevice2d` virtuals + CreateDevice string **unchanged** |
| Device class | `SmtRhi2dRenderDevice` unchanged; worker façade `Rhi2dFrameWorker` |
| Dual scheduler | `Rhi2dPresentController` (host) + `detail::Rhi2dFrameScheduler` (worker) |
| Paint types | `Rhi2dCartoDraw` · `Rhi2dPainter` · `GdiCartoFrame` |
| Context type | File `paint/carto/frame/context.h`; type `SmtRenderContext` stays |
| Dual-run tree | Out of scope |

### Acceptance

- Worker: `Rhi2dFrameWorker` + `detail::Rhi2dFrameScheduler` (no `SmtGdiRenderThread` / `GdiRasterScheduler`).
- Device members snake_case; `present()` / `canvas_` / `painter_` / `frame_worker_`.
- GDI tests + `legacy_render` green; CreateDevice still works.
- No new dated design twin — this `§` only.

### Non-goals

- Do not change carto LOD/colors.
- Do not merge into `gpu/` compositor.
- Do not rewrite `src/legacy/render/gdi/`.

---

## §GDI host thin facade（2026-10-01）

**Status:** accepted  
**Updated:** 2026-10-01  
**Code:** `src/legacy/render/rhi2d/impl/common/host/`

### Why

`host/render_device.cc` mixed lifecycle, interactive preview, encode/compose, and Draw* forwards. Collaborators `GdiUiController` / `GdiImageIo` names did not match present + buffer-image roles.

### Locked choices

| Topic | Choice |
| --- | --- |
| Split | `render_device` (lifecycle + thin schedule wrappers) · `device_interact` · `device_pass` · `device_draw` |
| Present lane | `Rhi2dPresentController` / `present_controller.*` · accessor `present()` |
| Buffer images | `Rhi2dBufferImage` / `buffer_image.*` · accessor `buffer_image()` |
| Draw*/Render* | Declarations stay on `SmtRhi2dRenderDevice`; bodies thin-forward in `device_draw.cc` |
| ABI | `SmtRhi2dRenderDevice` / CreateDevice / `RenderDevice2d` virtuals **unchanged** |

### Acceptance

- No `ui_controller.*` / `image_io.*` under `host/`.
- `legacy_render` + GDI tests green; FrameJob stage/debounce/present-on-gen behavior unchanged.
- No new dated design twin — this `§` only.

### Non-goals

- Do not add a second paint collaborator object for Draw* (no `GdiHostPaint`).
- Do not rename public `StrethImage` / other ABI spellings.

---

## §GDI carto + base/math 2D（2026-09-29）

**Status:** accepted  
**Plan:** [`../plans/2026-09-29-gdi-carto-base-math.md`](../plans/2026-09-29-gdi-carto-base-math.md)  
**Code:** `src/base/math/{affine2,simd}.*` · `…/gdi/core/paint/map_carto2d.*` · GDI paint multi-point draws

### Why

Leftover GDI multi-point paint still calls `LPToDP` per vertex. Carto declutter (`try_keep_point`) is O(n). Both should lean on `base/math` (2D affine + optional AVX2 batch) without changing MapLibre-style carto policy.

### Locked choices

| Topic | Choice |
| --- | --- |
| Approach | **`LpToDp2` + `transform_xy_batch`** in `base/math`; carto uses `Vector2` / constants; point declutter gets a cell grid |
| `LPToDP` semantics | Match leftover: `+0.5` then cast to `LONG`, then `Y = view_h - Y` |
| SIMD | Same as base/math: `base_math_simd` default **off** (scalar) |
| Style / LOD | **Unchanged** (colors, priority, budget, halo) |
| Worker | Still single GDI lane; this is CPU math on that lane, not parallel GDI |

### Acceptance

- Scalar batch ≡ prior per-point `LPToDP` for sample coords.
- `math_test` + `map_carto2d_test` pass; GDI paint smoke still draws pixels.
- No new dated design twin — revise this `§` only.

### Non-goals

- Do not force 2D through 4×4 `Matrix` / `transform_points_batch`.
- Do not wrap `MapCartoBox` in leftover 3D `Aabb`.
- Do not claim multi-core GDI speedup.

---

## §rhi2d Eigen / base/math deepen（2026-10-01）

**Status:** landed  
**Plan:** [`../plans/2026-10-01-rhi2d-eigen-base-math.md`](../archive/plans/2026-10-01-rhi2d-eigen-base-math.md)  
**Code:** `src/base/math/affine2.*` · `rhi2d/impl/common/` (+ product GDI via common) · extends §GDI carto + base/math 2D

### Why

Goal C (hot-path batch + type unify) under constraints: scope **common + gdi product path**, Eigen **only via `base/math`** (no `<Eigen/…>` in rhi2d). Leftover hand LP↔DP in `Rhi2dCartoDrawXform` / `DPToLP` and multi-point mesh/spline/primitives still bypassed `LpToDp2`.

### Locked choices

| Topic | Choice |
| --- | --- |
| Approach | Deepen `LpToDp2` + `transform_xy_batch` / new `inverse_xy`; unify xform + device DPToLP; batch remaining multi-point draws |
| Eigen surface | **A**: rhi2d uses `base/math` only (`Vector2`, `LpToDp2`, simd batch) |
| Scope | **B**: `impl/common/` (GDI DLL links common); no gdiplus/skia port drive-by |
| 4×4 | Still **do not** force 2D through `Matrix` |
| SIMD default | Remains **off** (`base_math_simd`) |
| ABI | `RenderDevice2d` LPToDP/DPToLP signatures unchanged |

### Acceptance

- [x] `inverse_xy` + `math_test` round-trip sample
- [x] `Rhi2dCartoDrawXform` / device `DPToLP` via `LpToDp2`
- [x] spline / primitives polyline / mesh grid+nodes / MBR batch
- [x] `build.bat debug math_test` + `legacy_rhi2d_gdi` green

### Non-goals

- Direct Eigen includes in rhi2d
- New `Affine2` type tree (unless a later § needs matrix compose beyond LPToDP)
- Preview stretch rewrite (policy stays in `preview_xform.h`)

---

## §rhi2d LP↔DP hot-path tune（2026-10-01）

**Status:** landed  
**Plan:** [`../plans/2026-10-01-rhi2d-lpdp-hotpath-tune.md`](../archive/plans/2026-10-01-rhi2d-lpdp-hotpath-tune.md)  
**Code:** `base/math/simd.*` · `rhi2d/.../style/xform.*` · `draw_ogr` linear_ring · prep play (`map_draw_batch`)  
**Extends:** §rhi2d Eigen / base/math deepen

### Why

Dense line/polygon paths already use `transform_xy_batch`. Remaining cost: `draw_linear_ring` still per-vertex `c_->lp_to_dp`; each `Rhi2dCartoDrawXform::lp_to_dp` rebuilds `LpToDp2`; batch SIMD is still a scalar loop. Do **not** force 2D through 4×4 `Matrix`.

### Locked choices

| Topic | Choice |
| --- | --- |
| P0 | `draw_linear_ring` → pack + `transform_xy_batch` (same as line_string) |
| P1 | Frame-level `LpToDp2` cache on `Rhi2dCartoDrawXform` (invalidate on context / port fingerprint change) |
| P2 | AVX2 fill of `transform_xy_batch` when `base_math_simd`; default **off**; ≡ scalar `LONG+0.5+flip_y` |
| P3 | Prep play keeps device `POINT` — no second LP→DP (document + guard comment) |
| 4×4 | Still forbidden for this path |

### Acceptance

- [x] No per-vertex `c_->lp_to_dp` in `draw_linear_ring`
- [x] Cached xform used by `lp_to_dp` / `dp_to_lp`
- [x] `math_test` scalar batch (+ SIMD ≡ scalar when flag on)
- [x] `build.bat debug math_test` + `legacy_rhi2d_gdi` green

### Non-goals

- Default-on SIMD
- `base::Matrix` for map LP↔DP
- gdiplus/skia drive-by; preview policy rewrite
- Claiming GDI `Polyline` speedup from math alone

---

## §GDI leftover profile（base::trace + MFC log）（2026-09-29）

**Status:** active  
**Plan:** [`../plans/2026-09-29-gdi-leftover-profile.md`](../plans/2026-09-29-gdi-leftover-profile.md)  
**Code:** `src/legacy/render/rhi2d/impl/gdi/core/worker/` + `core/surface/compose.*` · `src/base/trace/log/frame_log.h` · Legacy `shell/dock/render_trace.*` · Views `render_trace_panel` GDI filter

### Why

`base::trace::process_trace` + Views `RenderTracePanel` already cover map2d/scene3d. **GDI leftover** (`impl/gdi`) had zero `BASE_TRACE_EVENT`. Legacy SmartGIS-Legacy.exe needs a thin MFC panel that streams **text log lines** (not Views HWND host, not Chrome Trace UI dependency) so dual-track paint can be decomposed before optimizing.

### Locked choices

| Topic | Choice |
| --- | --- |
| Scope | **GDI leftover 2D only** (`rhi2d/impl/gdi`); not D3D leftover this slice |
| Tracer | Reuse `BASE_TRACE_EVENT` / `base::trace::process_trace` — **no** parallel `GdiFrameProfiler` |
| Categories | `gdi.frame` / `gdi.layer` / `gdi.geom` (prefix `gdi.`) |
| Geom depth | Per **Draw\* family** aggregated per layer (sum µs → one span); **not** per feature |
| Legacy UI | MFC thin dock: Record/Stop/Clear/Refresh + scrollable log list |
| Log shape | Continuous frame lines from `format_trace_frame_log_lines` (poll `base::trace::process_trace`) |
| Views | Add GDI filter checkbox on existing `RenderTracePanel` |
| Optimize | Data-driven follow-ups after hotspots appear in the log — not part of first land |

### Category table

| cat | name | Where |
| --- | --- | --- |
| `gdi.frame` | `submit` / `paint_loop` / `RenderMap` / `compose` / `cancel` | scheduler + paint_once + compose |
| `gdi.layer` | layer name (or `raster` / `tile` / `ogr`) | `GdiMapPainter::render_layer*` |
| `gdi.geom` | `point` / `line` / `polygon` / `anno` / `image` / … | flush after each layer |

### Acceptance

- `TRACE=1` or UI Record captures GDI spans; default off stays cheap.
- Legacy dock appends per-`RenderMap` text lines while armed.
- Views Gantt can show/hide `gdi.*`.
- `trace_test` covers frame-log formatter; GDI paint smoke still draws.

### Non-goals

- No Views Widget HWND host inside MFC.
- No per-feature spans.
- No mandatory Perfetto UI for legacy.
- No new dated design twin — revise this `§` only.

---

## §Atmosphere look pack（ocean C / fog depth / sky / cloud）（2026-09-29）

Industry look pass on `src/vista/component/atmosphere` without a second atmosphere tree. Living umbrella only — no new dated design twin.

### Scope

| Track | Deliverable |
| --- | --- |
| Ocean C | Separable Gaussian on FFT `height_map` + PS central-diff normals / Fresnel / sun specular / weak foam |
| Fog | CameraCB view-ray + optional `Device::shared_depth_texture()` sample; aerial tint |
| Sky | Fix NDC view-ray seam; sun disk; **Bruneton-lite** analytical multi-scatter (not full LUT) |
| Cloud | Powder / silver-lining; quality≤1 half-res *proxy* (fewer/coarser steps; no offscreen RT until RHI color attachments) |
| RHI | Shared depth created with DSV+SRV; `Device::shared_depth_texture()` non-owning |

### Wiring

- Passes stay under `ocean/` `fog/` `sky/` `cloud/`; `AtmosphereFrame` / `AtmosphereSession` bind depth into `FogPass::record(..., depth)`.
- Showcase `full` stacks ocean (sea-mask + horizon clip so the far lip does not replace the sky; coast stays the close-water check) and a grey cloud veil (cover cap, no white floor).
- DEM overview LOD sits in the dense bucket (cap 224). Albedo is elevation + slope rock/snow. Lit textured PS is GGX, sun self-shadow, and a two-tap derivative AA (swapchain stays 1×).
- Gate: `testing/tools/harness/atmosphere/atmosphere.full/atmosphere_full_loop.py` sky_delta + landish / cyan checks.

### Non-goals

- Full Bruneton/Hillaire LUT tables, SSR ocean, volumetric light shafts as required v1.
- No new dated design twin — revise this `§` only.

---

## §Atmosphere + Map2d look structural fix（approach B）（2026-09-29）

Dual-track structural fix (not tune-only, not full rewrite). Living umbrella only.

### Atmosphere

| Bug | Fix |
| --- | --- |
| Fog washes sky white | Cleared / sky depth (`>= 0.999`) → fog factor ≈ 0 (sky pass owns far-field) |
| Ocean cyan flare | Cap Blinn-Phong specular + slightly deeper albedo |
| Fog density floor | Keep terrain haze; do not reintroduce sun-glow in fog tint |

Gate: `atmosphere_full_loop.py` `blue_sky_frac_top` + `landish` + sky_delta.

### Map2d

| Bug | Fix |
| --- | --- |
| Roads invisible at China overview | Drop road `minzoom` 12→5; casing/fill colors readable on cream (not white-on-cream) |
| Style bind | Keep default carto + `carto_source_layer` remap (`area`→land, lines→river/admin/road) |
| Labels | Keep importance filter; no china_city.style.json scribble overwrite |

Gate: `map2d_china_loop.py` land_cream / water / detail_frac.

### Non-goals

- Full atmosphere rewrite; MapLibre expression engine; leftover GDI carto rewrite beyond present compose.

---

## §Atmosphere Google-Earth globe stack（DEM + sat cloud + sky）（2026-10-02）

**Status:** active  
**Updated:** 2026-10-02  

Three-layer product face on `src/vista/component/atmosphere` (no second engine):

| Layer | Pass | Role |
| --- | --- | --- |
| A Earth DEM | `globe/GlobePass` | UV sphere + height displace + equirect albedo |
| B Sat cloud | `globe/SatCloudPass` | Transparent shell slightly above earth |
| C Atmosphere | existing `SkyPass` (+ optional fog) | Far-field sky / scattering backdrop |

### Data entry

| Asset | Path (preferred) | Fallback |
| --- | --- | --- |
| Global DEM | `out/data/global_dem.tif` via `vista::find_sample_global_dem_path` / override | `china_dem.tif` is default `find_sample_dem_path` (China seed must not remask) |
| Global terrain albedo | `out/data/global_terrain.tif` (GeoTIFF; GDAL here is GTiff-only — use `build_globe_terrain.py --download-blue-marble`) | Hypsometric bake from DEM |
| Sat cloud | `out/data/sat_cloud.tif` / `global_cloud.tif` | Procedural cover stub |
| China overlay / imagery | `china_rs.tif` via `find_sample_imagery_path` (after global_*) | Hypsometric bake from DEM |

### Wiring

- `AtmosphereSession::{set_globe_enabled,set_sat_cloud_enabled}` stays the switch. `prepare_globe` keeps the `globe_surface_loaded_` short-circuit and sun direction. `prepare_sat_clouds` stays on the session.
- Height sampling and the hypsometric fallback are `DemRaster::sample_globe_surface` (`sample_meters`, then `bake_hypsometric_rgba` when imagery is missing). Grid density follows that call: about 512×256 global, about 768×480 regional. The `DemRaster` (GDAL buffers) is destroyed before `GlobePass::set_dem_surface`. Draw-param `height_scale` and slice counts are unchanged.
- Product DEM discovery stays `find_sample_global_dem_path` / `find_sample_global_imagery_path` (full-sphere when global_* is present). Default product DEM stays `china_dem` via `find_sample_dem_path`. Generate samples: `py -3 testing/data/build_globe_terrain.py`.
- Globe present skips flat DEM: no `rebuild_local_mesh`, no `GpuScene::sync_from`, and no `OpaqueEffect` on the `ViewInput` list.
- Record order on the one `graph::present` list: sky/depth (and globe DEM) in `record_pre_opaque`; flat ocean then cloud/fog/sat in `record_post_opaque`. Flat ocean is not drawn in the pre slot.
- Showcase: `--atmosphere-showcase=globe` (`testing/tools/harness/atmosphere/atmosphere.globe/`).
- Unit: `globe_pass_test` (Null RHI). Null showcase verified PASS (`ATMOSPHERE_SHOWCASE_GPU=0`).

### Non-goals

- Cesium Native / Ion global tiles; full WGS84 ellipsoid; animated weather GCM.
- No new dated design twin — this `§` only.

---

## §GDI internal RHI reshape（leftover A）（2026-09-29）

**Status:** accepted  
**Updated:** 2026-09-29 — Phases 1–4 landed (Task 4 stroke/clip/blit + Task 5 `gdi.encode` traces).  
**Plan:** [`../plans/2026-09-29-gdi-internal-rhi-reshape.md`](../plans/2026-09-29-gdi-internal-rhi-reshape.md)  
**Code:** `src/legacy/render/rhi2d/` (`public/` + `impl/gdi/core/`)

### Why

Leftover GDI should read like an industry 2D RHI (Device / Surface / CommandEncoder / Resource) and grow 2D capability, while remaining a **GDI present** adapter — **not** a second FlyCube `render::rhi` backend.

### Locked choices

| Topic | Choice |
| --- | --- |
| Approach | **Internal RHI vocabulary** under `impl/gdi/core/`; thin public facade |
| Phases | **1 Resource/Surface → 2 Encoder → 3 Draw ops → 4 Schedule/trace** |
| ABI | **May break** `RenderDevice2d` / callers (sync MFC/`xview`/tool) |
| Naming | `Gdi*` types; internals in `render` / `render::detail` — **never** `render::rhi::*` |
| Present | HWND still GDI BitBlt / Invalidate; compose IR stays `CompositorFrame` |
| Dirs | Keep `surface/` path for buffers; add `encode/`; device stays `host/` |
| Parallel GDI | **Non-goal** — single play lane |

### Components

`GdiDevice` (thin facade core) · `Rhi2dSurface` / `Rhi2dSurfacePool` / `Rhi2dOwnedSurface` · `Rhi2dCommandEncoder` / `Rhi2dCommandBuffer` · `GdiBackend` (existing paint) · `Rhi2dComposer` · `Rhi2dFrameScheduler` / `GdiRenderWorker`.

### Acceptance

- [x] Phase 1–2 gates green (`gdi_compose_test`, `gdi_encode_test`).
- [x] Phase 3 wire: host `BeginRender`/`EndRender` → encoder; `last_pass()`; `gdi_map_paint_test` (Draw* record = Task 4).
- [x] Phase 4 schedule/trace (Task 5): `gdi.encode` begin/end/take + `last_pass_ops`; worker `gdi.frame`/`RenderMap` + `encode_idle` (no double-draw replay); README hard line vs `src/render/rhi`.
- [x] Phase 3 richer Draw ops + replay pixel tests (Task 4: stroke_rect / clip / blit; text/path/blend deferred).
- Docs draw a hard line vs `src/render/rhi` (`impl/gdi/README.md`).
- No new dated design twin — this `§` only.

### Non-goals

- Do not embed FlyCube CommandList in the GDI encoder.
- Do not claim multi-core GDI draw speedup.
- Do not treat Skia as map RHI.

---

## §GDI core upgrade + dedupe（B then A2）（2026-09-29）

**Status:** accepted  
**Updated:** 2026-09-29 — B `.cc` + TLS layout-safe; A2 canvas-as-recorder + host/worker replay green.  
**Plan:** [`../plans/2026-09-29-gdi-core-upgrade-dedupe.md`](../plans/2026-09-29-gdi-core-upgrade-dedupe.md)  
**Code:** `src/legacy/render/rhi2d/impl/gdi/core/`

### Why

After reshape / paint dedupe / cc rename, `core/` still has (1) mixed `.cpp`/`.cc`, oversized paint TUs, and (2) a half-wired encoder (`Begin`/`End` record while Draw* stay immediate; `last_pass_` unused for paint). Upgrade = **B structural cleanup**, then **A2 canvas-as-recorder** so map geom goes record→single replay.

### Locked choices

| Topic | Choice |
| --- | --- |
| Order | **B then A** |
| A depth | **A2** — FrameJob + sync `RenderMap` geom/text/pen through encoder |
| Approach | **CartoDraw-as-recorder** — Draw* API stays; bind via TLS (`paint/carto/encode/encoder_tls.h`) so `Rhi2dCartoDraw` size does not shift `frame_worker_` |
| Road while encoding | Dual GDI pen polylines (not Gdiplus) |
| ABI | `RenderDevice2d` / CreateDevice / class names **unchanged** |
| Namespace | Never `render::rhi` |

### Acceptance

- [x] Phase B: `core/` sources `.cc`; encoder TLS layout-safe; tests green.
- [x] Phase A: blob ops + canvas record + host/worker replay; `gdi_map_paint_test` green.
- [x] No empty encode shell / double-draw; README updated.
- No new dated design twin — this `§` only.

### Non-goals

- Do not merge leftover into `gpu/` compositor.
- Do not rewrite `src/legacy/render/gdi/` dual-run tree.
- Do not claim multi-core GDI.

---

## §GDI flatten + GdiBackend（2026-09-29）

**Status:** accepted  
**Updated:** 2026-10-01 — flat top + `paint/{canvas,layer,encode,player,gdiplus}`; host canvas/layer via `unique_ptr`  
**Plan:** [`../plans/2026-09-29-gdi-flatten-player.md`](../archive/plans/2026-09-29-gdi-flatten-player.md)  
**Code:** `src/legacy/render/rhi2d/impl/gdi/{host,worker,paint,surface}/`

### Why

`core/` was an extra nesting layer; `encode/` and `gdiaux/` were a third play lane beside canvas immediate + encoder replay. Flat top + paint sub-layers keep record/play/capability separate without a second Player backend. `layer/` was later split out of `canvas/` so map orchestration does not share a directory with Draw* primitives; host owns both via `unique_ptr` so internal layout can change without shifting other `SmtRhi2dRenderDevice` members.

### Locked choices

| Topic | Choice |
| --- | --- |
| Top layout | `host/` `worker/` `paint/` `surface/` `res/` `test/` (no `core/`) |
| `paint/` | `canvas/` · `layer/` · `encode/` · `player/` · `gdiplus/` |
| Deps | `layer` → `canvas` Draw*/style; canvas must not include `layer` |
| Host paint | `unique_ptr<Rhi2dCartoDraw>` + `unique_ptr<Rhi2dPainter>` (worker already) |
| Aux | **Delete** `gdiaux/`; live helpers → `backend/gdi_backend.*` |
| GDI+ | `paint/gdiplus/` — token + AA string only (not a peer ImmediatePlayer) |
| Play | **`detail::GdiBackend`** shared by immediate canvas + `replay()` |
| Immediate roads | Dual GDI pens via `GdiBackend::road_polyline` |
| ABI | `SmtGdi*` / CreateDevice / `render::rhi` hard line unchanged |

### Acceptance

- [x] No `impl/gdi/core/` or `impl/gdi/gdiaux/`.
- [x] `paint/` split into canvas/layer/encode/player/gdiplus.
- [x] Host `canvas_` / `painter_` are `unique_ptr` (layout-safe).
- [x] `GdiBackend` owns clear/stroke/polyline/road/cross/disc/anno; encoder replay calls it.
- [x] scene3d `map_label_batch` includes `paint/gdiplus/gdiplus.h`.
- [x] `gdi_compose_test` / `gdi_encode_test` / `map_carto2d_test` green.
- [x] `gdi_map_paint_test` / `legacy_render` green.
- [x] Living § + README.

### Non-goals

- Do not force every immediate draw through encode→replay.
- Do not merge into `gpu/` compositor.

---

## §rhi2d multi-backend DLLs（2026-10-01）

**Status:** accepted  
**Updated:** 2026-10-01 — PaintBackend rename (drop Player) + LoadLibrary by chAPI (no `RHI2D_BACKEND_*`)  
**Code:** `src/legacy/render/rhi2d/impl/common/` + `impl/{gdi,gdiplus,skia}/` · `detail/renderer.cpp`

### Why

Port leftover 2D devices as separate LoadLibrary DLLs (mirror rhi3d gl/d3d), with shared facilities under `impl/common/` and **per-DLL paint backends** — not three forked device trees, and **not** compile-time backend macros.

### Locked choices

| Topic | Choice |
| --- | --- |
| Host tree | `impl/common/` (host/worker/encode/surface/canvas/layer) |
| Paint lane | virtual `PaintBackend` + `ScopedPaintBackend`; each DLL exports `emplace_paint_backend` / `rhi2d_port_api` via `create_backend.cc` |
| Concrete ports | `GdiBackend` / `GdiPlusBackend` / `SkiaBackend` under `…/backend/` (no `player/` dir; no replay-named type) |
| Traits | optional `rhi2d_backend_traits` concept (docs / static checks); `using Backend = …`; runtime path does not `#ifdef` backends |
| DLLs | `legacy_rhi2d_gdi` · `legacy_rhi2d_gdiplus` · `legacy_rhi2d_skia` |
| Loader | `legacy_render` hosts `SmtRenderer::CreateDevice(chAPI)` → LoadLibrary by name in `detail/renderer.cpp` |
| `chAPI` | `SmtGdiRenderDevice` · `SmtGdiPlusRenderDevice` · `SmtSkiaRenderDevice` |
| Present | HWND + DIB BitBlt for all three (not FlyCube) |
| Skia pin | when pin present use Skia; else GDI+ bootstrap inside `SkiaBackend` |
| `RenderBaseApi` | `RD_GDI` · `RD_GDIPLUS` · `RD_SKIA` |

### Non-goals

- Do not select backend with `RHI2D_BACKEND_*` macros in common.
- Do not put leftover Skia/GDI+ into `render::rhi::Backend`.
- Do not require Skia pin for `legacy_rhi2d_skia` to link (bootstrap allowed).
- Do not rename `play_batch` / `LayerPainter` in this slice (batch orchestration, not the HDC paint lane).

---

## §D3D leftover capability（2026-09-29）

**Status:** accepted  
**Updated:** 2026-09-29 — T3 docs + as-built README; T1 texture/FBO and T2 font/frustum landed under `resource/`.  
**Plan:** [`../plans/2026-09-29-d3d-leftover-capability.md`](../plans/2026-09-29-d3d-leftover-capability.md)  
**Code:** `src/legacy/render/rhi3d/impl/d3d/` · as-built [`../../../src/legacy/render/rhi3d/impl/d3d/README.md`](../../../src/legacy/render/rhi3d/impl/d3d/README.md)

### Why

Legacy **StereoTerrain / scene3d** still ships **`SmtD3DRenderDevice`** (`"Direct3D"` factory) beside OpenGL. This slice grows **leftover D3D11** capability inside `rhi3d/impl/d3d/` so product HWNDs keep working — **not** a FlyCube strangler on the same HWND and **not** modern `render::rhi`.

### Present strangler (this HWND)

| Topic | Choice |
| --- | --- |
| Role | **Leftover capability** on the MFC / legacy scene3d viewport — parallel to `impl/gl/` |
| FlyCube | **Do not** create FlyCube on this HWND; former `bind_rhi_present` removed |
| Present owner | **D3D11 `IDXGISwapChain::Present`** (and offscreen `color_tex_` blit) — **do not** create FlyCube swapchain on this HWND |
| Modern RHI | **`src/render/rhi` (FlyCube DX12/Vulkan)** stays the new-track default; D3D11 leftover does not become `render::rhi::Backend` |

### ABI and factory (locked)

| Topic | Choice |
| --- | --- |
| Public type | **`Smt3DRenderDevice`** virtual surface unchanged |
| Factory | **`CreateD3DRenderDevice`** export, API string **`"Direct3D"`**; **`Release3DRenderDevice`** in same `legacy_render_d3d` DLL |
| Enum slot | **`GetBaseApi()` → `RA_D3D09`** (historical name); runtime is **D3D11**, not D3D9/D3DX |
| DLL | **`legacy_render_d3d`** (host/shared stays **`legacy_render`**; GL is **`legacy_render_gl`**) |

### Directory layout

Industry-shaped seams under **`rhi3d/impl/d3d/`** (colocated `.h`/`.cpp`/`.cc`):

| Dir | Role |
| --- | --- |
| **`host/`** | `SmtD3DRenderDevice` facade (`render_device.*` + `device_present.cpp`) — Init/Destroy/Release, Begin/End/Swap, Present, capture |
| **`resource/`** | System-memory VB/IB (`buffer/`), **`texture.cc`**, **`frame_buffer.cc`**, **`font.cc`** |
| **`paint/`** | Draw paths, matrix stack, state manager, stub shader/program manager |
| **`caps/`** | `Smt3DDeviceCaps` defaults (`device_caps.*`) |
| **`ext/`** | Leftover extension interface glue (`ext_interface.cpp`) |

File stems are **snake_case** (aligned with GDI leftover / repo-global naming). See `impl/d3d/README.md` rename map. **Do not** rewrite `Smt_*` / factory export names.

GN: `//src/legacy/render/rhi3d/impl/d3d:d3d_sources` → `legacy_render_d3d` (`d3d11.lib`, `dxgi.lib`, `d3dcompiler.lib`).

### Phased capability (T0–T4)

| Track | Scope |
| --- | --- |
| **T0** | Device + swapchain + offscreen RT, lit **DrawIndexedPrimitives** (StereoTerrain mesh), staging **`CaptureBgr24`**, line strips + **DrawScreenBgra** (MapLabelBatch) |
| **T1** | **Texture** create/build/bind/release + **FBO** create/attach/clear/unbind (`resource/texture.cc`, `resource/frame_buffer.cc`) |
| **T2** | **GDI bitmap font** slots + **DrawText** (world + screen) + **`GetFrustum`** (GL-compatible clip-plane extract) |
| **T3** | Living **`§`** + plan checklist + module README + Active table link (no new dated design twin) |
| **T4** | **`d3d_texture_test`** (hidden HWND) + **`legacy_scene3d_china_loop.py --d3d`** in engine shot matrix |

**Viewport rule:** `SetViewport` adjusts rasterizer state only; **color target resize** happens when requested size equals **HWND client size** (mid-frame 120×120 leftovers must not recreate RTs).

### Non-goals

- **No full GLSL shader/program port** — fixed HLSL lit mesh + compile-at-init helpers only; **`SmtShader` / `SmtProgram` manager remains stub** (failure/null).
- **No FlyCube** device/swapchain/command list on this HWND.
- **No `SmtVideoBuffer` GPU path** — create/destroy may exist; bind/update/map **stay stub** until a later slice.
- **No D3D9 / D3DX resurrection**; **no** folding D3D11 into `render::rhi::kDx12`.
- **No** rewriting **`Smt_*` export names** or merging `legacy_render` with FlyCube.

### Acceptance

- [x] T0 baseline draw + Present documented in module README.
- [x] T1 texture/FBO sources under `resource/` wired in `BUILD.gn`.
- [x] T2 font + `GetFrustum` under `resource/font.cc` + `paint/matrix.cpp`.
- [x] T3 docs (this `§`, plan, README, superpowers index).
- [x] T4 `d3d_texture_test` + `--d3d` shot loop green in CI/local verify.

---

## §GL leftover capability（2026-09-29）

**Status:** accepted  
**Updated:** 2026-09-29 — T0 `host/`·`resource/`·`paint/` layout; capability audit + `gl_texture_test`; docs.  
**Plan:** [`../plans/2026-09-29-gl-leftover-capability.md`](../plans/2026-09-29-gl-leftover-capability.md)  
**Code:** `src/legacy/render/rhi3d/impl/gl/` · as-built [`../../../src/legacy/render/rhi3d/impl/gl/README.md`](../../../src/legacy/render/rhi3d/impl/gl/README.md)  
**Sibling:** §D3D leftover capability (same living umbrella; separate checklist plan). Joint name for both: **Leftover 3D capability (D3D+GL)**.

### Why

Legacy **StereoTerrain / scene3d** ships **`SmtGLRenderDevice`** (`"OpenGL"` factory) beside D3D11. GL already had fuller texture/FBO/font/frustum than early D3D leftover. This slice **aligns directory seams** with D3D/GDI (`host/` · `resource/` · `paint/`), keeps present ownership on WGL **`SwapBuffers`**, and adds a narrow unit smoke — **not** a FlyCube strangler and **not** modern `render::rhi`.

### Present strangler (this HWND)

| Topic | Choice |
| --- | --- |
| Role | **Leftover capability** on the MFC / legacy scene3d viewport — parallel to `impl/d3d/` |
| FlyCube | **Do not** create FlyCube on this HWND; former `bind_rhi_present` removed |
| Present owner | **GL `SwapBuffers`** — **do not** create FlyCube swapchain on this HWND |
| Modern RHI | **`src/render/rhi` (FlyCube)** stays the new-track default; leftover GL does not become `render::rhi::Backend` |

### ABI and factory (locked)

| Topic | Choice |
| --- | --- |
| Public type | **`Smt3DRenderDevice`** virtual surface unchanged |
| Factory | **`Create3DRenderDevice`** export, API string **`"OpenGL"`**; **`Release3DRenderDevice`** in same `legacy_render_gl` DLL |
| Enum slot | **`GetBaseApi()` → `RA_OPENGL`** |
| DLL | **`legacy_render_gl`** (host/shared stays **`legacy_render`**; D3D is **`legacy_render_d3d`**) |

### Directory layout

Industry-shaped seams under **`rhi3d/impl/gl/`** (colocated `.h`/`.cpp`):

| Dir | Role |
| --- | --- |
| **`host/`** | `SmtGLRenderDevice` facade (`render_device.*` + `device_present.cpp`) — Init/Destroy/Release, Begin/End/`SwapBuffers` |
| **`resource/`** | VB/IB (`buffer/`), `texture.cpp` / `frame_buffer.cpp` / `font.cpp` + `text/` glyph helper |
| **`paint/`** | Matrix/misc/fast_draw/effect/shader/VBA/VBO, `states_manager` |
| **`caps/`** | `SmtGLDeviceCaps` (`device_caps.*`) |
| **`ext/`** | Extension loaders (`*_func` / `*_func_imp`) |

File stems are **snake_case** (see `impl/gl/README.md` rename map). **Do not** rewrite `Smt_*` / factory export names.

GN: `//src/legacy/render/rhi3d/impl/gl:gl_sources` → `legacy_render_gl` (`opengl32.lib`, `glu32.lib`).

### Phased capability (T0–T4)

| Track | Scope |
| --- | --- |
| **T0** | Reshape `device/`+`buffer/`+`text/` → `host/`·`resource/`·`paint/`; BUILD.gn + includes; behavior-preserving |
| **T1** | Capability audit — texture/FBO already complete; fix clear breakage only (e.g. CreateFont fail-path leak) |
| **T2** | Font/frustum/DrawText regression via smoke (already implemented; no ABI change) |
| **T3** | Living **`§`** + plan checklist + module README + Active table link (no new dated design twin) |
| **T4** | **`gl_texture_test`** (hidden HWND) + **`legacy_scene3d_china_loop.py`** (GL default, no `--d3d`) |

### Non-goals

- **No FlyCube** device/swapchain/command list on this HWND.
- **No** rewriting **`Smt_*` export names** or merging `legacy_render` with FlyCube.
- **No** full new RHI Facade inside legacy GL.
- **No** drive-by rename of public `GL*` types or factory strings.
- **Do not** edit `impl/d3d/` in the GL slice (coordinate via separate plans).

### Acceptance

- [x] T0 layout under `host/` · `resource/` · `paint/` · `caps/` · `ext/`.
- [x] T1/T2 CreateTexture/FBO/CreateFont/GetFrustum/DrawText still wired; CreateFont fail-path leak fixed.
- [x] T3 docs (this `§`, plan, README, superpowers index).
- [x] T4 `gl_texture_test` + GL shot loop (verify locally with `build.bat debug`).

---

## §rhi2d Chromium-cc compose（2026-10-01）

**Status:** active  
**Updated:** 2026-10-01 — threading superseded for map paint by §rhi2d leftover tile-raster; `paint/carto/` peers unchanged  
**Plan:** [`../plans/2026-10-01-rhi2d-chromium-cc-compose.md`](../archive/plans/2026-10-01-rhi2d-chromium-cc-compose.md)  
**Code:** product `src/scenic/render/rhi2d/impl/common/cc/` (+ `host/` · `paint/` · `surface/`). Leftover `src/legacy/render/rhi2d/` is **frozen** — do not iterate. Upgrade path: **§RHI2D cc frame-beat upgrade**.

### Why

Align leftover map2d host with Chromium **cc** (commit / activate / draw) so dual-track scheduling, retire, and compose IR read like the industry compositor — without merging into `gpu/` or copying Chromium sources.

### Locked choices

| Topic | Choice |
| --- | --- |
| Approach | **`impl/common/cc/`** owns frame beat (`LayerTreeHost` · `Scheduler` · `LayerTreeImpl` gen/damage) |
| Pipeline | PresentController → Host stage/submit → activate → (tile raster / Painter) → submit_surface / present_to_hwnd |
| Threading | **Compose, do not merge:** serial `NThreadPoolExecutor(1)` FrameJob (`MapScheduler`) **owns** the Impl lane; paint_fn barriers on one `TileGraphRunner` (HWND still never `join`s the FrameJob; raster pool joins after idle) |
| ABI | `RenderDevice2d` / LoadLibrary / three DLL stems **frozen** |
| Ports | `impl/{gdi,gdiplus,skia}/` = `backend/` + `create_backend` only |
| Namespace | `render::detail` + `Rhi2d*` (no third public ns) |
| Concept rule | **No parallel Chromium names for the same job** — no FrameWorker/FrameSink/cc::Layer list beside Host/Composer/paint |
| `paint/carto/` | Four equal subdirs: `encode/` · `draw/` · `frame/` · `style/`. Root holds no sources; no forwarding headers. |

### Non-goals

- Blink DisplayList clone; geographic LOD tile cache (product basemap) — not this §
- FlyCube on leftover HWND; merge leftover into `gpu/` compositor — see tile-raster § for CPU MT first
- `Smt*` rename

### Acceptance

- [x] `common/cc/` present; FrameWorker / FrameSink / backend shims removed
- [x] Device uses `layer_tree_host_`; publish via surface composer only
- [x] README three-axis model; `gdi_cc_test` covers Scheduler + TreeImpl
- [x] Overdue gen does not publish (`tree.is_current` + scheduler abort)
- [x] `paint/carto/{encode,draw,frame,style}` landed; includes + `BUILD.gn` retargeted

---

## §rhi2d leftover tile-raster（2026-10-01）

**Status:** active  
**Updated:** 2026-10-04 — Scheduler ∥ TileGraphRunner **compose** (not merge); one process-wide raster pool  
**Plan:** [`../plans/2026-10-01-rhi2d-leftover-tile-raster.md`](../plans/2026-10-01-rhi2d-leftover-tile-raster.md)  
**Diagram:** [`../diagrams/legacy-render-architecture.html`](../diagrams/legacy-render-architecture.html)（浅色 SVG：rhi2d Host/cc/Paint/Surf 泳道 + commit→Raster×N→publish 动画）  
**Code:** `src/legacy/render/rhi2d/impl/common/cc/` (+ `paint/map/`, `surface/`, ports `impl/{gdi,gdiplus,skia}/`)

### Why

Product dual-track: **breakthrough Chromium-like multi-thread raster on the leftover track first**, then feed the same semantics into Views / `src/gpu`. GPU raster on leftover HWND is **out of this slice**.

Landing priority (revised): expose **two parallel grains** so wall-clock can be compared on the same map — viewport **tile** grid and per **GIS layer** — before claiming either as the sole product default.

### Decisions (locked)

| Topic | Choice |
| --- | --- |
| Track | Leftover `rhi2d/impl/common` (option C) |
| First breakthrough | **C1** multi-thread offscreen raster; GPU compose/raster later |
| Mode switch | `RHI2D_PARALLEL=serial\|tile\|layer` (default **tile**). Legacy `RHI2D_TILE_RASTER=0` → serial when PARALLEL unset |
| Grain A (scheme 1) | **Viewport device-pixel grid** (default **256×256**, adaptive up to 1024 to keep ≤~4 tiles unless `RHI2D_TILE_SIZE` set) |
| Grain B | **Per GIS layer** encode → parallel `execute` → ocean **color-key** compose (TransparentBlt; preserves z-order cartography) |
| Ports | Shared encode IR; tile/layer execute uses **thread-private** HDC (all ports) |
| Thread model | **H2 slim compose:** FrameJob (`MapScheduler` / size-1 executor) = Impl; **N Raster** via one `TileGraphRunner`. Distinct shutdown (leak vs join) — **do not collapse into one type** |
| Runner | Slim **TaskGraphRunner**: one process-wide pool, workers **pull** job indices for **tile or layer** (no per-job PostTask; no second pool) |
| Paint split (tile) | **Serial encode** → **parallel `execute_tile` + AABB cull** on **partial damage**; full-damage falls back to serial (`RHI2D_TILE_FORCE=1` to force). Prefer **layer** grain for full china frames |
| Paint split (layer) | **Serial encode per layer** → **parallel `execute`** → ocean clear + z-order ocean color-key |
| Damage | Tile mode: intersect damage with tiles; blit only dirty centers |
| Skirt | Tile **outset 16px**; compose writes center only |
| Labels | In command buffer for v1; dedicated serial label pass deferred |
| Timing | `RHI2D_PARALLEL_LOG=1` prints `execute_ms` to stderr |
| ABI | `RenderDevice2d` / Create·Destroy / three stems **frozen** |

### Architecture

```
Main(HWND): commit(rc, damage, gen) → wake FrameJob/Impl (never join)

tile:   activate → encode map once → TileTaskGraph(damage)
        Raster×N: pull tile → execute_tile (private HDC+DIB)
        Impl: barrier → blit centers → publish if gen current

layer:  activate → encode per GIS layer (serial OGR)
        Raster×N: pull layer → execute full buffer (private DIB)
        Impl: ocean clear → ocean color-key z-order → publish if gen current

serial: encode once → execute full frame
```

N default: `clamp(2, 4, hardware_concurrency/2)`.

### Code map

| Path | Role |
| --- | --- |
| `cc/tile_graph_runner.*` | Raster×N pool, ready queue, barrier, cancel |
| `cc/raster_tile.*` | Rect, outset, enumerate, `Rhi2dParallelMode` / env |
| `cc/scheduler.*` | FrameJob Impl lane; stage/submit/cancel/wait_idle/gens |
| `paint/map/map_draw.*` | Map strategy: walk + parallel grain dispatch (`MapPainter`) |
| `paint/map/layer_draw.*` | Layer strategies (OGR / raster / tile) |
| `paint/map/feature_{prep,batch,draw}.*` | Feature prep / encode / fallback |
| `paint/map/map_submit.*` | Execute + compose + publish |
| `paint/carto/encode/command_encoder.*` | `execute` / `execute_tile` |
| `surface/` | Tile/layer DIB pool; blit; HWND present |

### Non-goals

- FlyCube / SkiaGPU on leftover HWND this slice
- Geographic / slippy tile cache (scheme 2)
- Merging leftover into `src/gpu`
- Sharing one GDI DC across threads
- Blink DisplayList clone
- Full dedicated Impl thread (optional follow-up)
- Layer-mode **pixel parity** with serial (TransparentBlt / keyed compose deferred)

### Relation to older §§

- Supersedes §GDI leftover worker **non-goal** “no multi-threaded GDI feature drawing” for the **tile/layer execute path** (per-thread HDC only).
- §Chromium-cc compose keeps commit/activate/gen; FrameJob lane remains Impl wake.
- Product-track reuse of these **semantics** (not GDI HDC / tile blit) is owned by **§src_render + vista parallel accelerate**.

### Acceptance

- [x] `cc/raster_tile.*` + `cc/tile_graph_runner.*` + encode `execute`/`execute_tile`
- [x] `gdi_cc_test` + `tile_raster_test` (stitch, damage, cancel, env fallback, layer color-key compose)
- [x] `impl/common/README.md` pipeline documents TileGraph + layer mode
- [x] `RHI2D_PARALLEL=serial\|tile\|layer` (+ legacy `TILE_RASTER=0`)
- [x] `legacy_rhi2d_{gdi,gdiplus,skia}` build green with tile/layer path
- [ ] Pan/zoom smoke: HWND never joins; stale gen does not publish
- [ ] Measurable wall-clock compare on mid-size PLP (`PARALLEL_LOG` + serial/tile/layer)

---

## §rhi2d modern C++ / base harden（2026-10-01）

**Status:** active  
**Plan:** [`../plans/2026-10-01-rhi2d-modern-cpp-base-harden.md`](../plans/2026-10-01-rhi2d-modern-cpp-base-harden.md)  
**Code:** `src/legacy/render/rhi2d/` (`detail/renderer.cpp`, `impl/common/`, `impl/{gdi,gdiplus,skia}/`, `public/` drive-by)

### Why

Mirror §rhi3d harden for leftover 2D: loader still uses `MessageBox`, `Release` does not `FreeLibrary`, tree still has C-era `NULL` / C-casts / raw `POINT[]` scratch. Paint hot path should use modern C++ + `base` allocators without changing `RenderDevice2d` ABI or multi-DLL LoadLibrary shape.

### Decisions

| Topic | Choice |
| --- | --- |
| Depth | **B**: loader harden + full-tree modern C++ + paint hot-path base |
| Boundary | **Full tree**: `common` + `gdi`/`gdiplus`/`skia` ports (+ stale `impl/gdi` mirrors when present) |
| ABI | **`RenderDevice2d` / Create*/Destroy* exports frozen**; `public/` only `NULL`→`nullptr`-class edits |
| Load errors | **`LOGGING(LOG_ERROR)` + return code** — no `MessageBox` |
| DLL lifetime | `Release` destroys device then **`FreeLibrary`**; fail paths unload; `CreateDevice` replaces prior via `Release()` first |
| POINT scratch | Keep heap path (not HybridOptimized TLS — known worker crash); prefer `base::allocate` / RAII over raw `new[]` |
| Paint style | `std::span` / range-for / `static_cast` where hot; no algorithm rewrite this slice |
| Non-goals | No FlyCube on leftover HWND; no `Smt*` rename; no layout mega-move; no re-enable TLS POINT pools |

### Phases

1. `detail/renderer.cpp` mirror §rhi3d loader
2. Tree-wide safe `\bNULL\b` → `nullptr` (preserve `NULL_BRUSH` / `NULL_PEN` / `BS_NULL`)
3. Paint hot path: `points.h` + draw/prep/encoder ownership + light modern C++
4. Port DLLs (`gdi` player / `gdiplus` / `skia`) same hygiene

### Acceptance

- [x] `build.bat debug legacy_render legacy_rhi2d_gdi legacy_rhi2d_gdiplus legacy_rhi2d_skia` green
- [x] `gdi_encode_test` run ok; `gdi_map_paint_test` paint path ok (teardown FreeLibrary hang/abnormal exit — worker unload race; not loader MessageBox path)
- [x] No `MessageBox` in `rhi2d/detail/renderer.cpp`; `FreeLibrary` on `Release` + fail paths
- [x] Safe `NULL`→`nullptr` under `rhi2d/` (excluding Win32 `NULL_*` stock macros)

---

## §rhi3d modern C++ / base harden（2026-10-01）

**Status:** active  
**Code:** `src/legacy/render/rhi3d/` (`detail/renderer.cpp`, `impl/{gl,d3d}/host`, `resource/buffer`, factories)

### Why

Leftover 3D host/loader still carried C-era patterns (`MessageBox` on load fail, missing `FreeLibrary`, raw `new[]` for host stacks/IB, substring `GL_EXTENSIONS` checks). Harden internals with modern C++ and `base` without touching `Smt*` virtual ABI or FlyCube.

### Decisions

| Topic | Choice |
| --- | --- |
| Scope | `detail/renderer` + `impl/gl` + `impl/d3d` internals; `public/` only drive-by |
| ABI | **`Smt3DRenderDevice` / Create*/Release* exports frozen** |
| Load errors | **`LOGGING(LOG_ERROR)` + return code only** — no `MessageBox` |
| DLL lifetime | `Release` calls `Release3DRenderDevice` then **`FreeLibrary`**; fail paths unload too |
| Host ownership (D3D) | `std::unique_ptr` for state manager / caps / matrix stacks |
| Host buffers | `base::allocate` / `deallocate` for GL+D3D index host arrays (VB already Arena) |
| GL extensions | Cached `GL_EXTENSIONS` string; **token** match via `string_view` |
| Non-goals | No FlyCube on leftover HWND; no rhi2d sync in this slice; no `Smt*` rename |

### Acceptance

- [x] `build.bat debug legacy_render legacy_render_gl legacy_render_d3d` green
- [x] Tree-wide `\bNULL\b` → `nullptr` under `rhi3d/` (paint/ext/public leftovers)
- [ ] Optional: `gl_texture_test` / `d3d_texture_test` smoke

### §rhi3d camera modern C++（2026-10-01）

**Status:** active  
**Plan:** [`../plans/2026-10-01-rhi3d-camera-modern-cpp.md`](../archive/plans/2026-10-01-rhi3d-camera-modern-cpp.md)  
**Code:** `src/legacy/render/rhi3d/public/camera/`

| Topic | Choice |
| --- | --- |
| Type names | Keep `Smt*` (`SmtPerspCamera`, …) |
| Methods | **`snake_case`** (breaking); `const&` inputs |
| Combined | Merge into `SmtPerspCamera`; **delete** `combined_camera.h` |
| Factory | `make_view3d_camera` → `std::unique_ptr<SmtPerspCamera>` |
| Layout | Decl `camera.h` + body `camera.cc`; shared orbit in `detail` |
| Math | Reuse `base/math` `Vector3` |
| Non-goals | No FlyCube camera; no `Scene` ownership rewrite |

**Acceptance**

- [x] `combined_camera.h` gone; Combined APIs on `SmtPerspCamera`
- [x] Call sites updated; `build.bat debug legacy_render` green (+ `legacy_tool` / `ui_legacy`)

---

## §Legacy render Pipeline + Arena（trace + memory/execution）（2026-09-30）

**Status:** active  
**Plan:** [`../plans/2026-09-30-legacy-render-pipeline-arena.md`](../plans/2026-09-30-legacy-render-pipeline-arena.md)  
**Code:** `src/legacy/render/detail/` · `rhi2d/impl/gdi` · `rhi3d/impl/{d3d,gl}` · `scene3d/`

### Why

GDI already has `BASE_TRACE_EVENT` (`gdi.*`). **rhi3d / scene3d** had near-zero process_trace coverage. Hot paths still use raw `new[]` / `parallel_for` without `base::execution::Pipeline` ContextHooks / Arena. Dual-run leftover needs one skeleton so RenderTrace + Memory counters tell a coherent story before further strangler cuts.

### Locked choices

| Topic | Choice |
| --- | --- |
| Scope | Full leftover tree: `rhi2d` + `rhi3d` + `scene3d` |
| Depth | **C**: Pipeline stages + Arena / ObjectAllocator on frame temps; SpanRecorder optional |
| External ABI | Keep `Init` / `BeginRender` / `EndRender` / `Draw*` / `Present` / `SwapBuffers` signatures (**B**) |
| Scheduler | Keep `Rhi2dFrameScheduler` coalesce/cancel lane; **inside** paint, prep runs `Pipeline` (produce→map→sink) |
| Categories | `gdi.*` (existing) · `rhi3d.d3d.*` / `rhi3d.gl.*` · `scene3d.*` · `memory` counters |
| Shared | Header-only `legacy/render/detail/frame_pipeline.h` (finish-frame memory sample + `[legacy.flow]` logs) |
| Legacy UI | AMBox tabs **Console** (`DebugConsoleDockBar` ← `log_sink`) + **RenderTrace** (aligned with Views Diagnostic Tools) |

### Phases

1. Shared detail + GDI prep → Pipeline + frame-end `sample_memory_counters_to_process_trace`
2. rhi3d present/draw spans + host VB arrays via `base::allocate`
3. scene3d `Update`/`Render`/`build_mesh` spans + Arena for mesh scratch
4. Legacy AMBox Console + RenderTrace docks (key `[legacy.flow]` lines in Console)
5. (Follow-up) optional scheduler→Pipeline facade — not required for first green
6. **Encode style dedupe (2026-09-30):** recording `prepare_for_drawing` cache hit skips redundant `set_pen`/`set_brush`; `set_encoder` flushes style cache so a new pass always emits the first pen/brush. (`GdiBackend` same-args skip deferred — exposed AV in replay.)
7. **Line PolyPolyline coalesce (2026-09-30):** `play_prepared_batch` merges consecutive ordinary `PrepKind::Line` runs (no road dual-pen, no river line label) into one `draw_device_polylines` → `PolyPolyline`.
8. **Prep overview thin (2026-09-30):** stronger `overview_vertex_step` + Chebyshev `overview_thin_chebyshev(scale)` on device thin (cell 2–3 at overview). Cuts prep verts and line play density together.
9. **Round D extreme (2026-09-30):** raise overview thin/step so china `fblc≈10` gets cell≥3; OGR `getPoints` bulk only when `step==1` (`pack_curve_xy_strided`); unlabeled roads coalesce to dual-pen `PolyPolyline` (skip style prep); encoder + `GdiBackend` same-pen skip; `::Polyline` for replay.

### Acceptance

- Armed tracing shows `gdi.prep.*` / `rhi3d.*` / `scene3d.*` spans; default-off stays cheap.
- Frame end emits `memory` counters when armed.
- Legacy SmartGis AMBox shows **Console** (flow logs) and **RenderTrace** tabs.
- Console surfaces `[legacy.flow]` + INFO/WARN process logs via `base::log_sink`.
- `gdi_map_paint_test` / leftover mesh-or-scene tests / `legacy_render` / `SmartGis` link green.
- No change to public `Smt*` virtual signatures.
- Encode→replay: consecutive same-style features do not re-emit pen/brush ops (style cache hit).

### Non-goals

- Do not rip `Rhi2dFrameScheduler` coalesce in v1 of this §.
- Do not parallelize HDC play.
- No new dated design twin — revise this `§` only.

---

## §Map2d richness P0 — hillshade + line casing（2026-09-30）

**Status:** accepted  
**Updated:** 2026-10-03 — look facts stay here; present ownership, blend, and software entry are §Map2d present  
**Plan:** [`../plans/2026-09-30-map2d-hillshade-line-casing.md`](../plans/2026-09-30-map2d-hillshade-line-casing.md)  
**Present layering:** §Map2d present（`Diagram:` [`map2d-present-frame.html`](../diagrams/map2d-present-frame.html)）

Capability / look alignment with MapLibre **Style Spec effects**, not MapLibre Native
implementation. Product pin of mln/mbgl stays deferred (see Optional GPU tile
basemap §). Wire name `maplibre` remains StyleDocument tile.

### Why

Industry gap: 2D carto richness lags MapLibre (hillshade / extrusion / heatmap
placeholders; roads often wash out without casing). Atmosphere / DEM 3D path is
already a differentiator — P0 closes the **2D basemap “premium”** look first.

### Locked choices

| Topic | Choice |
| --- | --- |
| Strategy | Approach A (effect-first): hillshade + line casing before full expression / extrusion |
| Hillshade | Style `type: hillshade` constants stay on `ResolvedPaint`. `emit_hillshade` emits one shaded raster `DrawItem` with `DrawBlend::kMultiply`. Bake is `bake_hillshade_slot` (§Map2d present). `kRaster` stays `kOver` |
| Line casing | Two Style layers (`road-casing` then `road`) in `default_carto_style_json`. Width is `LineTessOptions::pixel_width` on each layer; `emit_lines` tessellates both. No third casing width. No MapLibre SDF line shaders |
| DEM source | Reuse `vista::DemRaster` / china_dem samples; no new terrain stack |
| Align gate | Extend `style_align.json` + `maplibre_align` / china loops; optional Native still is reference only |
| Implementation ban | No `#include` mln/mbgl from `app/` / `content/public` / `gpu/` / `vista/component/map`; no port of hillshade_prepare / line SDF buckets |
| Expression | Own mini eval in `gis/style/eval/expression.*` — **not** a MapLibre expression VM |

### Deliverables

1. **Line casing:** Overview `minzoom` keeps casing+fill readable on the china frame; cream-safe colors live in `default_carto_style_json`; `style_align.json` includes the casing layer; draw order is Style order (`road-casing` then `road`). `emit_lines` writes `kLine` triangles. GDI draws those triangles (`append_tris`); items with fewer than 3 indices are skipped.
2. **Hillshade:** `paint_resolve` fills hillshade constants into `ResolvedPaint`. When a hillshade layer is present and DEM is bound, `emit_hillshade` sets `DrawBlend::kMultiply`. Bake, luma multiply, and the GPU/GDI exits are §Map2d present.
3. **DEM bind:** Content loads `china_dem` via `find_sample_dem_path` (getenv, china-extent gate, soft-fail if missing) and holds RGBA for `load_raster`. `shade_dem_rgba`, `HillshadeParams`, `hillshade_max_edge_for_zoom`, cache, and `TileSlot` live in `bake_hillshade_slot` next to vista terrain.
4. **Gates:** `map2d_china_loop` / align stills show road casing contrast; soft hillshade variance metric when DEM underlay signal is present.
5. **Mini expression (landed):** expand paint/layout constant eval used by line/fill/circle/symbol — see §Mini expression below.

### §Mini expression — interpolate / match（2026-09-30）

**Status:** landed (mini subset expansion)  
**Code:** `src/gis/style/eval/expression.{h,cc}` · gate `style_test`

Still **not** a full MapLibre expression VM. `fill_resolved_paint` evaluates array paint/layout values to constants via `AttrMap` + zoom.

| Era | Ops |
| --- | --- |
| Prior mini | `get`, `literal`, `zoom`, `==`, `!=`, `<`, `<=`, `>`, `>=` |
| Added | `interpolate` (`linear` \| `exponential`; numeric + `#RGB`/`#RRGGBB`/`#AARRGGBB`/`rgb()` channel lerp), `step`, `match` (scalar or label arrays), `case`, `coalesce` |

**Out of mini (remaining gaps):** `cubic-bezier` interpolation, arithmetic (`+`/`*`/`-`/`/`), `let`/`var`, `to-number`/`to-string`/`format`, feature-state, image/heatmap/SDF expression helpers, data-driven array outputs beyond scalar constants.

### §Fill-extrusion v1 (richness B first item)

**Status:** landed (v1 constants + layout prism)  
**Updated:** 2026-09-30

YAGNI slice — **not** full MapLibre extrusion / SDF / full expression VM.

| Piece | Choice |
| --- | --- |
| Paint | `fill-extrusion-height` / `base` / `color` / `opacity` constants on `ResolvedPaint` |
| Layout | Own prism: darker wall quads + roof fill (`DrawKind::kFill`) with world-CRS `z` and a small isometric xy nudge for ortho GDI readability |
| Ban | No mln/mbgl include; no port of Native buckets |

Deferred with other richness: SDF lines/glyphs, full expression VM (beyond §Mini expression). Heatmap v1 landed below. Collision multi-slot along-line is landed in §Symbol collision.

### §Heatmap v1 (richness after A+B)

**Status:** landed (v1 constants + CPU circle splat)  
**Updated:** 2026-09-30

Own math / Style Spec paint names only — **no** MapLibre Native pin or `mln`/`mbgl` includes.

| Piece | Choice |
| --- | --- |
| Paint | `heatmap-radius` / `weight` / `intensity` / `color` / `opacity` constants on `ResolvedPaint` (Spec defaults: radius 30, weight/intensity/opacity 1; color default `#ff6400`) |
| Layout | `emit_heatmap`: zoom-matched layer → soft `DrawKind::kCircle` splat per point (`MultiPoint` expands); opacity = clamp(opacity×weight×intensity) |
| Default style | Optional `heatmap` layer (`source-layer: heatmap`) after hillshade; silent when no matching points |
| Ban | No GPU density texture / kernel accumulation; no weight property expressions or Spec color-ramp `interpolate` |

Deferred: GPU heatmap underlay, full weight / color expressions, additive blend density.

### §Symbol collision / along-line slots (richness — after A+B)

**Status:** landed (v1 CPU slots + bitmap metrics)  
**Updated:** 2026-09-30

YAGNI slice toward MapLibre-ish labeling — **not** Native SDF atlas / GPU text / full collision graph.

| Piece | Choice |
| --- | --- |
| Along-line | `line_label_pose_at` + ordered slot fractions (mid → quarters); `emit_symbols` retries until `LabelGrid::try_keep` |
| Fit gate | `line_fits_label` drops text when path length &lt; ~1.05× run width |
| Collision boxes | Metrics-accurate upright box + `rotate_label_box` AABB; `estimate_run_width_px` (CJK ≈ em, Latin ≈ 0.55em) when metrics missing |
| Icon+text | Existing union box + halo pad; no auto text-offset packing yet |
| Ban | No mln/mbgl includes; no Qt |

Deferred: full SDF distance-field atlas, GPU glyph raster, text-offset / icon-text-fit layout.

### Non-goals (this §)

- Full MapLibre expression VM (beyond the mini subset in §Mini expression).
- Full SDF glyph atlas / GPU text. Collision multi-slot v1 above is the landed floor (not a Native collision index). Full fill-extrusion lighting / pitch camera remains later; v1 prism + heatmap splat above are the landed floor.
- Porting MapLibre shaders, buckets, Immutable style diff, or Native product link.
- Atmosphere rewrite. Product MapFrame present is §Map2d present. Leftover `rhi2d` carto stays on the leftover track.

### Success

- China overview: roads read as dual-stroke (casing darker, fill lighter), not cream scribble.
- With DEM: hillshade underlay visible behind land/water vectors when Style enables it.
- Fill-extrusion Style layers emit wall+roof DrawItems in unit tests.
- Heatmap Style layers emit circle splat DrawItems when zoom matches; `style_test` / `map2d_test` cover paint resolve + emit.
- Mini expression: `style_test` covers interpolate/match (and step/case/coalesce) for line/fill/circle/symbol paints.
- Along-line labels survive midpoint blockers via slot retry; short paths drop oversize text (`frame_test`).
- `build.bat debug` + map2d china / align gates green; no mln product dep.

---

## §Map2d present — MapFrame layering（2026-10-03）

**Status:** accepted  
**Updated:** 2026-10-04  
**Diagram:** [`../diagrams/map2d-present-frame.html`](../diagrams/map2d-present-frame.html)（含 Scenic rhi2d app 栈）  
**Code:** `src/vista/component/map/**` · `src/vista/pass/map/**` · `src/content/browser/present/map2d/**`  
**Look facts:** §Map2d richness P0（two style layers, `LineTessOptions::pixel_width`, hillshade/heatmap/extrusion/collision）  
**Budgets:** §Vista Map2d equal-profile — this § does not chase warm-GPU or `paint_ms`

CPU contract remains `vista::MapFrame`. Cartography runs once in `Layout::build`. Software raster and GPU only consume `DrawItem`. GDI is the first software rasterizer (project, batch, `TextOutW`, `BitBlt`) and does not own cartography decisions.

**Frame-beat (P3, content):** `Map2dFrameCache` reuses Scenic cc **semantics** (generation, full damage, `mark_published` refuses stale) on the same `MapFrame` dual exit. Do **not** copy leftover `CommandBuffer` / GDI tile blit into content. Details: `src/content/browser/present/README.md` · [`§RHI2D cc frame-beat upgrade`](#rhi2d-cc-frame-beat-upgrade2026-10-04).

### Chain

`MapScene` → boundary POD → `LayerBatch` → `LayoutInput` → `vista::Layout::build` → `MapFrame` (`background_rgba` + `items`) →

- GPU: `Map2dGpuPresent` → `vista::FramePass` → FlyCube `render::rhi::BlendMode`
- Software: `paint_map_frame_gdi` → HDC
- Scenic rhi2d (`MAP2D_ENGINE=scenic`): `ScenicRhi2dHost` → `Renderer2d` + `gis::Map` + `scenic_rhi2d_{gdi,gdiplus,skia}` → HWND/`blit_to_dc` (`RHI2D_PORT`). Not a MapFrame exit. `src/legacy` is frozen.

### Layer ownership

| Layer | Owns |
| --- | --- |
| content present | `Map2dPresenter` bind and the GPU/software/Scenic choice; `PresentAction` + `prepare_for_present`; `Map2dGpuPresent` thin adapter; `ScenicRhi2dHost` when `MAP2D_ENGINE=scenic`; phase profile; GDI `FillBatch` / `DcStyle` / `BitBlt` cache; HWND/HDC only on the software / Scenic entries |
| gis vista + carto | `Layout::build`; `default_carto_style_json` as the only color source; collision; line / label / batch policy; `shade_dem_rgba`; hillshade bake orchestration |
| effect map | `DrawItem` → command list; `DrawBlend` → `BlendMode` |
| software GDI | fill triangles, draw positioned glyphs, blit rasters using `DrawItem` blend |

`gis/style` stays `StyleDocument` / `ResolvedPaint` / expressions. It is not the batch or casing policy home.

### Locked

1. **Colors.** The only color source is `default_carto_style_json` / `MapFrame::background_rgba` (`0xAARRGGBB`). Carto headers carry no `COLORREF` palette and no second road/river `COLORREF` table.
2. **Batches.** Policy lives in `vista/component/map/detail/carto_filter` (`vista::detail`). `build_layer_batches` in `detail/batch_build.cc` takes POD features. Vista does not include `map_scene.h`. `Layout::build(LayoutInput, vector<LayerBatch>)` stays. Content copies `MapScene` → POD at the boundary.
3. **Line casing.** Two style layers, `road-casing` then `road`. Width is the existing `LineTessOptions::pixel_width`. `emit_lines` tessellates both. `LineTessOptions` has no third casing width. GDI draws `kLine` only as triangles (`append_tris`). Items with fewer than 3 indices are skipped, same as an empty mesh. There is no `flush_stroke_run` cartography.
4. **Labels.** The only labels are `DrawKind::kText` from `emit_symbols` plus `detail/collision` (scale floor, budget, and 8-direction nudge live there; along-line slots stay §Symbol collision). There is no second whole-string `paint_labels_projected` / `LabelOccupancy` path and no whole-string `DrawItem`.
5. **Blend.** `DrawItem` carries `enum class DrawBlend : uint8_t { kOver, kMultiply }` defaulting to `kOver`, defined in `vista/component/map/draw.h`. That header does not include `rhi.h`. `emit_hillshade` sets `kMultiply`; `emit_raster` stays `kOver`. One vista pure function implements luma multiply `dst.rgb * ((1 - a) + a * luma)`. GDI calls it only for `kMultiply`; `kOver` is `AlphaBlend`. GPU adds `BlendMode::kMultiply` (`dst.rgb *= src.rgb`). Fixed-function blend cannot express `dst * ((1 - a) + a * luma)`, so opacity and luma are baked into the texture with that same function before upload (alpha hard cut), and multiply items use opacity 1. `Pass` and GDI read the same `DrawBlend`. Do not multiply every `kRaster`.
6. **Hillshade bake.** `hillshade_max_edge_for_zoom`, `HillshadeParams`, cache, `shade_dem_rgba`, and `TileSlot` live next to vista terrain as `bake_hillshade_slot`. Content keeps getenv, the china-extent gate, the DEM path, and the RGBA held for `load_raster`.
7. **Software entry.** The contract is `MapFrame` + `View` + `load_raster` → HDC. `paint_map_frame_gdi` is the first implementation. The MapFrame image backend is not `src/legacy/render/rhi2d` `PaintBackend`. This design adds no Skia or GDI+ software backend. FlyCube is the RHI device, not a fourth software rasterizer.
8. **`kIcon`.** GPU atlases icons. GDI may no-op until a product style emits icons, then draw a bitmap from `symbol_id` + anchors. That GDI icon draw is an open gap.

### Out of scope

- Equal-profile warm-GPU and `paint_ms` budgets stay in §Vista Map2d equal-profile. Those cells set `MAP2D_NO_HILLSHADE=1`, so they do not prove hillshade parity.
- `PresentAction` and `FillBatch` batching stay.
- Cache / GPU compose / execution parallel for this tree: **§content present accelerate**.

---

## §DEM / hillshade bake profile + bench（2026-10-04）

**Status:** accepted  
**Updated:** 2026-10-04  
**Diagram:** [`../diagrams/hillshade-bake-profile.html`](../diagrams/hillshade-bake-profile.html)  
**Plan:** [`../plans/2026-09-30-map2d-hillshade-line-casing.md`](../plans/2026-09-30-map2d-hillshade-line-casing.md) Task 6（no twin plan）  
**Code:** `vista/component/map/detail/hillshade_bake.*` · `vista/terrain/process/{dem_hillshade,land_mask,bake_backend}.*` · tests `dem_raster_test` / `land_mask_test` · harness `testing/tools/harness/map2d/run_hillshade_bake_bench.py`  
**Related:** §Map2d present (C4 bake ownership) · §Vista Map2d equal-profile (`hillshade_ms` lump; matrix still `MAP2D_NO_HILLSHADE=1`) · `DemPhaseSample` (3D seed only)

### Why

`Map2dPhaseSample.hillshade_ms` is the **layout-lump** wall around `bake_hillshade_slot`. It cannot attribute load vs Horn vs Thrust vs disk. Terrain PIP (`fill_lonlat_mask`) is a sibling bake, not a present phase. Equal-profile map2d FPS (`MAP2D_FPS_BENCH_MS`) is **warm StaticReuse** and must not be the DEM bake bench.

### Locked — profile

| Topic | Choice |
| --- | --- |
| Span sink | `base::trace::process_trace` **`cat=bake`**. Names: `HillshadeBake`, `cache_mem`, `cache_disk`, `load_dem`, `shade_cpu`, `shade_cuda`, `disk_store`, `fill_lonlat_mask` |
| Not startup | `startup_profile` stays **`cat=startup`** (cold chrome → first map). Bake spans must **not** use `cat=startup` (content previously wrapped bake as startup — wrong bucket) |
| Last-lump POD | `vista::HillshadeBakeSample` on `bake_hillshade_slot` (mem/disk/load/shade/store ms + hits + `used_cuda` + w/h/`max_edge`). `vista::LandMaskBakeSample` on `fill_lonlat_mask`. `DemPhaseSample` stays 3D load/tess/hypso |
| Downsample | Fused into `shade_dem_rgba` (`step_x`/`step_y` from `max_edge`). No fake `downsample_ms` clock |
| Arm recording | `TRACE=1` / `--trace=1` **or** `BAKE_PROFILE=1` / `--bake-profile=1`. Dump: `TRACE_DUMP=<path>` Chrome JSON under captures |
| Backend | `BAKE_BACKEND` / `--bake-backend` = `auto` (default) \| `cpu` \| `cuda` (`gpu` alias). `cpu` skips Thrust. `cuda` must **not** silently CPU-fallback (bench cell is skip if launch fails) |
| Disk | `BAKE_DISK=0` skips disk lookup/store (kernel compare). Default on for product |
| Capture | `out/<config>/captures/analysis/hillshade_bake/` — JSON + markdown table. Not gen-root; not a new product exe |

### Locked — equal-profile bench

| Axis | Value |
| --- | --- |
| DEM | `vista::find_sample_dem_path()` → `out/data/china_dem.tif` |
| LOD | `max_edge=768` (zoom 4.5–7 china overview bucket in `hillshade_max_edge_for_zoom`) |
| Illumination | `HillshadeParams` defaults: direction **335°**, altitude **32°**, exaggeration **0.5** |
| CPU cell | `base::execution::parallel_for` Horn / PIP |
| CUDA cell | `try_shade_dem_thrust` / `try_fill_lonlat_mask_thrust` when `has_cuda` **and** runtime device; else JSON `cuda_ok=false` (not a fail) |
| Compare | Same raster / rings / illumination; CPU then CUDA; report **cold** (first timed) and **warm** (second). CUDA context must not be charged to the CPU cell |
| Entry | `dem_raster_test` (shade + optional `bake_hillshade_slot`) · `land_mask_test` (PIP) · `py -3 testing/tools/harness/map2d/run_hillshade_bake_bench.py` merges table like map2d matrix JSON |
| Env | `BAKE_BENCH=1` on those tests writes JSON; default off so `te` stays cheap |

### Relation to map2d FPS

`MAP2D_FPS_BENCH_MS` / `map2d-fps-bench.txt` may **echo** `HillshadeBakeSample` for cold leftover attribution. It is not the equal-profile bake matrix. Warm `hillshade_ms≈0` remains the present-path gate (C4 hit).

### Non-goals

- New `SmartGIS --bake-showcase` product exe
- Rewrite `thrust_gis.cu` for the bench (callers skip/require via `BAKE_BACKEND`)
- Mixing `DemPhaseSample` hypso with 2D hillshade columns
- Enabling `out/.build.lock.on`

---

## §content present accelerate（cache / GPU compose / execution · 2026-10-04）

**Status:** active  
**Updated:** 2026-10-04 — as-built mailbox `stage_frame` / `drain_layout_jobs` / one-time empty-cache wait; `PresentSnapshot` hillshade `shared_ptr` (no lock-held vector copy); `std::mutex` (not recursive) only mailbox + slot swap; C1 `layer_slices_` **deleted** (not revived)  
**Diagram:** [`../diagrams/content-present-accelerate.html`](../diagrams/content-present-accelerate.html)  
**Code:** `src/content/browser/present/{map2d,scene3d,host}/**` · `src/vista/{frame,map}/**` · `src/render/graph/**` · `src/base/execution/**` · Topology B `src/gpu/compositor/**`  
**Budgets:** §Vista Map2d equal-profile · §src_render Scene3d equal-profile（本 § 不改数字）  
**Parallel machine:** §src_render + vista parallel（L0–L3 / `stage_frame`）  
**GPU-process:** §GPU-process accelerate（Topology B `FrameComposer`；默认产品仍是 Topology A）  
**Checklist:** append on [`../plans/2026-10-02-src-render-vista-parallel-accelerate.md`](../plans/2026-10-02-src-render-vista-parallel-accelerate.md) Task 7（不新开 dated plan）

### Why（as-built bottlenecks）

Warm china StaticReuse already skips `graph::present` (`present_gpu_ms≈0`). Remaining cost is **cold / settle / interactive** on `content/browser/present`, not a missing second IR.

| Hot | As-built | Why it hurts |
| --- | --- | --- |
| Whole-frame layout | `Map2dFrameCache` facade + `map2d_layout_build` / `map2d_mailbox`. Fingerprint dirty or zoom-bucket change → `kRebuildFull` → L2 `Layout::build` all layers (C1 per-layer slices **not** in tree) | Layer toggle / restyle pays china-sized tess |
| Display join | Fingerprint/camera **off** `mu_`; snapshot copies `shared_ptr` handles only. GPU `present` does **not** hold `mu_` across Pass | HWND `WM_PAINT` still never `wait_first_published` |
| Fake pipeline | Comment in `rebuild_layout`: prior `async`+`get` on batches **spawned then immediately joined** | Pool tax with zero overlap; correctly removed |
| Nested pool | Fill/line `parallel_for` still joins inside `Layout::build`; `visible_layer_batches` is **serial POD copy** (as-built; spec’s `batches_via_parallel_for` is not this TU) | Oversub if a second outer `parallel_for` is added blindly |
| Hillshade prefetch | `prefetch_hillshade` on `base::execution`; RGBA held as `shared_ptr<const vector<uint8_t>>` | GPU/GDI `load_raster` consume snapshot handles (Pass may still copy to bake multiply) |
| GPU skip vs compose | StaticReuse skips L3 entirely. InteractiveReuse still `present_frame` unless FPS-bench. `invalidate_uploaded` on full/settle / `!last_present_ok_` | Camera-only DrawCache encode exists in `Pass::record`; cold still `place_frame` + `upload_draws` |
| Software cache | HDC BitBlt keyed by `layout_gen` + size (`try_blit_present_cache`) | Pan does not share GPU’s world-space DrawItem reuse |
| Scene3d | Warm remesh skip lives in `GpuScene`, not a present-tree `PresentAction` | Atmosphere `prepare_for_present` can still serialize with GPU |

### Core layers (locked)

| Lane | Owner | May | Must not |
| --- | --- | --- | --- |
| **C Cache** | `Map2dFrameCache` + `Pass::DrawCache` + software HDC + vista hillshade bake | published `shared_ptr<MapFrame>` + hillshade `shared_ptr`; gen refuse stale | Second CommandBuffer / leftover cc GIS list; RECT GDI dirty; revive C1 |
| **G GPU compose** | `Map2dGpuPresent` / `Scene3dGpuPresent` → `graph::present` (A) or later submit CF (B) | Camera-only encode; incremental VB/IB by item identity; raster/hillshade textured quads | Multi-CL default; worker-thread `rhi::Device`; chrome blends pixels |
| **E Execution** | L1 content schedule; L2 vista emitters on **sole** `base::execution` pool | `Pipeline` / `async` **without same-tick get**; `parallel_for` tess; prefetch on executor | `std::thread` detach; second product pool; HDC from workers |

### 1. Cache (C0–C2)

Keep `PresentAction` (`kRebuildFull` / `kInteractiveReuse` / `kSettleRebuild` / `kStaticReuse`) and Scenic **generation** (`layout_generation` / `mark_published`).

| Level | Key | Payload | Invalidate when |
| --- | --- | --- | --- |
| **C0** (as-built) | `ContentFingerprint` + `CameraKey` (pixels + extent + zoom_bucket) | Whole `MapFrame`, hillshade RGBA | pending_damage, fp change, size/zoom_bucket |
| **C1** (dead — do not revive) | — | Per-layer `DrawItem[]` was AV on china export (`absorb_layer_slices` / `layer_slices_`). **Deleted.** Settle reuses world items from published `shared_ptr<MapFrame>` instead. | — |
| **C2** (as-built, tighten) | Pass `DrawCache` world/full `UploadedDraw` | GPU VB/IB/textures | `kRebuildFull` / `kSettleRebuild` / device lost — **not** `kInteractiveReuse` |
| **C3** (as-built) | software `layout_gen` + size | Compatible HDC bitmap | layout gen; optional camera-only blit if items world-space |
| **C4** | vista hillshade bake cache + content RGBA | DEM shade | zoom LOD / illumination / DEM stamp |

**Interactive pan:** world-space `DrawItem` + new ortho camera → C0 `kInteractiveReuse`, C1 skip, C2 camera-only `encode_draws`, C3 may miss (software pan still paints or affine blit later).

**Settle (~200 ms):** keep debounce. Worker tess posts via mailbox; Display presents published. Settle reuses world DrawItems from the previous published frame (not a C1 slice table).

**Publish:** one published `shared_ptr<MapFrame>` slot (worker `make_shared` then swap under `mu_`). No `building_frame_` member. Display presents published; worker never mutates published without gen match.

**Mutex:** `std::mutex mu_` (not recursive) shrinks to mailbox + slot swap. Fingerprint/camera run **outside** the lock. Do **not** hold it across `Pass::record` / `graph::present`. No public `mutex()` / `frame()` reference API — tests use `share_frame()`.

### 2. GPU compose (G0–G2)

Topology **A** (Views in-process) stays the product default: Display thread, **one** CommandList, `render::graph::present`. Topology **B** (`src/gpu` `RhiComposer` `kSolid`/`kBgra` quads) is multiprocess remap — present does not `#include` leftover; it may later **emit** `CompositorFrame` IR. Do not dual-compose A+B in one process.

| Step | Lock |
| --- | --- |
| G0 Camera-only | Interactive + shell-stable: `Pass::record` DrawCache hit (already). Present must **not** `invalidate_uploaded` on `kInteractiveReuse`. |
| G1 Incremental upload | Item identity (`layer_ord` + mesh hash). Dirty slices `place_frame`/`upload_draws`; unchanged borrows C2 buffers. CPU pack of **new** meshes may `parallel_for` then **one** Display upload. |
| G2 Raster compose | Hillshade / XYZ tiles: textured quads (`load_raster` / `gpu/raster/tile` mosaic) not re-tess. Vector fill/line stay MapFrame meshes. |
| Warm StaticReuse | Keep skip `graph::present` when swapchain image valid (china FPS). |
| Scene3d | Same 1 CL; warm `rebuild_count=0`; frustum `prep_cull_parallel` only after `SCENE3D_FRUSTUM_CULL` honesty. |

`ShellOverlayEffect` stays an extra `ViewInput` effect on the same CL (A). Do not fold chrome pixels into `MapFrame`.

### 3. CPU parallel via `base::execution` (E0–E2)

Sole pool: `GlobalNThreadPoolExecutor` / `parallel_for` / `Pipeline`. **Hard rule:** never `async` then `get` on the same Display tick (already proven useless).

```
L0  stage FrameRequest (gen++) · never join
L1  if C0 StaticReuse → skip L2/L3
    if InteractiveReuse → L3 camera-only encode
    else post Pipeline (do not wait on UI)
E   Pipeline stages (overlap previous present):
      hs:   C4 bake / prefetch on executor   // replace std::thread
      pod:  MapScene → LayerBatch POD        // serial or grain≥N
      tess: Layout emitters parallel_for     // fill/line now; unify circle/heatmap
      pack: optional CPU VB pack parallel_for
L3  Display: wait only if published gen stale AND no previous image
    swap building → published if gen current
    G0/G1 record_and_present
```

| Do | Don’t |
| --- | --- |
| Prefetch hillshade via executor + bake cache | `std::thread().detach()` |
| One tess `parallel_for` inside `Layout::build` | Outer batches `parallel_for` **and** inner tess on the same rebuild (oversub) |
| Worker TLS arena; ordered merge by `layer_ord` | Worker `rhi::Device` / HWND / HDC |
| Labels/`LabelGrid` serial after tess | Shared collision from workers |
| Cancel stale gen; HWND never join | Join layout in `Widget::paint` |

`visible_layer_batches` stays a **boundary copy**. Parallelize tess, not the POD clone, until copy shows up in `layout_ms`.

### Phases

| Phase | Deliverable | Depends |
| --- | --- | --- |
| **C0/E0** | Mutex shrink; dual slot gen; hillshade prefetch → executor; document no same-tick async+get | — |
| **G0** | Interactive never `invalidate_uploaded`; DrawCache encode path as default pan | C0 |
| **C1** | Per-layer DrawItem cache + settle incremental | C0 |
| **E1** | `stage_frame` + Pipeline hs∥tess overlap previous present（= parallel V2） | C0 |
| **G1** | Incremental `upload_draws` + optional CPU pack `parallel_for` | G0, C1 |
| **G2** | Raster/hillshade quads; Topology B emit CF only if `--type=gpu` | G1 |
| **E2** | Unify remaining emitters; Scene3d `prep_cull_parallel` after frustum | E1 |

### Acceptance

- China 1280×720 Debug: warm StaticReuse budget **unchanged**; `layout_builds_delta≈0` on FPS window
- Pan (`kInteractiveReuse`): no `rebuild_layout`; no `invalidate_uploaded`; `gpu_upload_ms≈0`
- Settle/cold: `layout_ms` / `gpu_upload_ms` drop vs whole-frame rebuild (phase clocks)
- `VISTA_LAYOUT_PARALLEL=0` still correct; visual hillshade + labels on inspect
- Destroy/navigate: no join hang; stale gen never `mark_published`

### Non-goals

- Copy leftover `CommandBuffer` / per-thread GDI HDC / D3D11 deferred TLS
- MapLibre Native; delete MapFrame to fake ms
- Second thread pool; product `getenv` leftover `RHI2D_*`
- Dual Topology A+B compose in one process

### Relation

- **§Map2d present**: dual exit + colors/blend unchanged
- **§Vista / Scene3d equal-profile**: ms budgets
- **§vista parallel**: L0–L3 machine; this § is the **present-tree** cache+compose adoption
- **§GPU-process**: B remap only

---

## §Point cloud GPU draw（2026-09-30）

**Status:** active  
**Updated:** 2026-09-30  
**Plugin side:** [`2026-09-13-plugin-host-design.md`](2026-09-13-plugin-host-design.md) §world3d pointcloud LAS  
**Plan:** [`../plans/2026-09-30-world3d-pointcloud-las.md`](../plans/2026-09-30-world3d-pointcloud-las.md)

### Locked

| # | Choice |
| --- | --- |
| 1 | `vista::NodeKind::kPointCloud` carries CPU `point_positions` (xyz float) + optional `point_rgba`. |
| 2 | P0 draw: `tessellate_point_cloud` → tiny triangles uploaded like other lit kinds; `GpuScene::record_draws` must `record_kind(..., kPointCloud, ...)`. |
| 3 | P1+: chunk AABB cull; P2 octree/LOD — no native POINTLIST pipeline required for P0. |
| 4 | Do not route default Views path through leftover `PointCloud3d`. |

---

## §rhi3d leftover parallel frame（2026-10-01）

**Status:** active  
**Updated:** 2026-10-02 — Diagram SVG（legacy/render rhi3d P1–P3 流水线）  
**Plan:** [`../plans/2026-10-01-rhi3d-parallel-frame.md`](../archive/plans/2026-10-01-rhi3d-parallel-frame.md)  
**Diagram:** [`../diagrams/legacy-render-architecture.html`](../diagrams/legacy-render-architecture.html)（浅色 SVG：FrameJob → Prep×N → D3D deferred → publish）  
**Code:** `src/legacy/render/rhi3d/impl/common/frame/` + `scene3d/host/stereo_hwnd_view.*` (+ P2 prep in `scene3d/scene/`)

### Why

Leftover GL/D3D HWND `Begin→Update→Render→End→Swap` runs on the caller thread today, so orbit/present can stall the message pump. Industry practice (Chromium Impl, Unity Jobs, OSG DatabasePager): **off-main FrameJob first**, then **CPU prep parallel** while GPU submit stays single-threaded. GL cannot safely multi-thread Draw on one context; D3D11 deferred context is a later asymmetric option.

### Decisions (locked)

| Topic | Choice |
| --- | --- |
| Track | Leftover `rhi3d` + `scene3d` stereo HWND only — **not** FlyCube / `render::graph` |
| Port symmetry | **Homogeneous GL+D3D** for P1+P2 |
| P1 | Off-HWND **serial** FrameJob (`NThreadPoolExecutor(1)`); Main stages/submits; HWND **never joins** |
| P2 | In-frame **CPU prep** parallel (frustum AABB); then draw (serial on GL; P3 deferred on D3D) |
| P3 | D3D11 **deferred-context** multi-thread record (asymmetric); GL stays P2-only |
| Fallback | `RHI3D_FRAME_JOB=0` → sync present; `RHI3D_PREP_PARALLEL=0` → prep N=1; `RHI3D_D3D_DEFERRED=0` → serial D3D draw |
| ABI | `Smt3DRenderDevice` / Create*/Release* / `smt_stereo_hwnd_*` export names **frozen** (behavior: present may return before GPU finish when FrameJob on) |

### Architecture

```
Main(HWND/caller): stage(Rhi3dFrameRequest) → submit
Render worker×1:   Clear/Begin → Update → Render:
                     P2 frustum cull (CPU prep×N)
                     P3 D3D: bind deferred slots → object Render×N → Finish+Execute
                     GL: serial object Render
                   → End/Swap → mark_published
capture/blit:      wait_idle(bounded) then read front
```

Industry map: P1 ≈ Chromium Impl / rhi2d FrameJob; P2 ≈ Unity Jobs / Unreal ParallelFor prep; P3 ≈ D3D11 deferred contexts.

### Code map

| Path | Role |
| --- | --- |
| `rhi3d/impl/common/frame/frame_request.h` | Staged yaw/pitch/distance/size |
| `rhi3d/impl/common/frame/scheduler.*` | FrameJob lane (mirror `Rhi2dScheduler`) |
| `rhi3d/impl/common/frame/prep_runner.*` | P2 resident pull workers (+ P3 fan-out) |
| `rhi3d/impl/d3d/host/deferred_draw.*` | P3 deferred context pool + TLS bind |
| `rhi3d/impl/d3d/ext/ext_interface.*` | Cross-DLL `SmtD3DBeginDeferredDraw` / Bind / Finish |
| `scene3d/scene/d3d_deferred_objects.h` | Partition visible objs across deferred slots |
| `scene3d/host/stereo_hwnd_view.*` | Owns scheduler; present/capture contracts |
| `scene3d/scene/scene.*` · `scene/octree.*` | P2 cull + P3 deferred Render |

### Non-goals

- Shared GL context multi-thread Draw; 3D viewport tile-raster RT
- FlyCube / `render::graph` “one CommandList” contract change
- Merge leftover into `src/gpu`
- Perfect transparency order across deferred slots (opaque-first leftover OK)

### Acceptance

- [x] `Rhi3dFrameScheduler` + `frame_scheduler_test` green
- [x] FrameJob on: present returns without blocking full Update/Render; HWND destroy no join hang
- [x] `RHI3D_FRAME_JOB=0` matches prior sync present behavior
- [x] capture/blit wait published gen (bounded); destroy cancels + shutdown
- [x] P2: N≥2 prep workers when enabled; CPU cull before draw
- [x] P3: D3D deferred begin/bind/finish (`d3d_texture_test`); `RHI3D_D3D_DEFERRED=0` serial fallback
- [x] `legacy_render` / `legacy_render_d3d` green
- [x] Module README documents FrameJob + prep + deferred env (gl/d3d)

---

## §rhi3d Eigen / base/math frustum（2026-10-01）

**Status:** active  
**Updated:** 2026-10-01  
**Plan:** [`../plans/2026-10-01-rhi3d-eigen-base-math.md`](../archive/plans/2026-10-01-rhi3d-eigen-base-math.md)  
**Code:** `src/base/math/{frustum,matrix4}.*` · `rhi3d/public/device/{base,render_device}.h` · `rhi3d/impl/{gl,d3d}/paint/*` · `scene3d/{scene/octree,scene/scene,primitive/surface/pointcloud}.*`

### Why

Leftover 3D still hand-extracts frustum planes (GL NeHe / D3D transpose pack) and keeps a parallel `SmtFrustum` with `IsBoxIn`, while `base/math` already has Eigen-backed `Frustum` / `Matrix` / `Aabb`. Camera already uses `Vector3`. Need one math source for lookAt / MVP frustum / AABB cull without storage-level Eigen rewrite.

### Locked choices

| Topic | Choice |
| --- | --- |
| Approach | **1**: converge on `base::Frustum` / `Matrix`; golden-gate before switching cull |
| ABI | **`GetFrustum(Frustum&)`** (breaking); purge `SmtFrustum` from public headers |
| Cull | Callers use `Frustum::intersects(Aabb)`; planes stored **outward** for `Aabb::cull` |
| Extract | `Frustum::from_view_proj` = row-major MVP → column-major clip → Gribb/Hartmann, then **negate** to outward |
| GL | Still reads GL matrix stack into clip[]; shared extract helper |
| D3D lookAt | `Matrix::view_look_at` (gluLookAt RH); GL may keep `gluLookAt` |
| Non-goals | No Eigen-as-storage for `Matrix`/`Vector`; no FlyCube; no batch GPU frustum |

### Acceptance

- [x] `math_test` golden: leftover inward `IsBoxIn` decisions ≡ `Frustum::intersects` after `from_view_proj`
- [x] `SmtFrustum` / `FrustumSide` / `PlaneData` gone from `rhi3d/public`
- [x] `legacy_render` / `legacy_render_gl` / `legacy_render_d3d` green (`gl`/`d3d_texture_test` depend on device)
- [x] scene3d octree / pointcloud / scene use `Frustum` + `intersects(Aabb)`
- [x] Simple extract+cull timing log in `math_test`

---

## §Vista Map2d equal-profile optimize（2026-10-01）

**Status:** active  
**Updated:** 2026-10-03 — overall **P0–P3** scheme; engine codename **`vista`**; baseline from Debug equal-latitude matrix；**P3 harness** FALSE-GAP labels + `VISTA_LAYOUT_PARALLEL` env set (product getenv still parallel-plan V1)  
**Plan:** [`../plans/2026-10-01-src-render-map2d-equal-profile-optimize.md`](../plans/2026-10-01-src-render-map2d-equal-profile-optimize.md)（phased P0–P3 + Task 1–6）  
**Present layering:** §Map2d present (colors, casing, labels, hillshade blend, software entry). This § locks equal-profile budgets only; P3 stays `VISTA_LAYOUT_PARALLEL` / false-gap.  
**Related parallel:** [`../plans/2026-10-02-src-render-vista-parallel-accelerate.md`](../plans/2026-10-02-src-render-vista-parallel-accelerate.md) / **§src_render + vista parallel**（`VISTA_LAYOUT_PARALLEL` = P3；规范图已有，不另开 HTML）  
**Code:** `vista/**`, `content/browser/present/map2d/**`, `vista/component/map/**` (`Pass::record`, `upload_draws`), `render/{rhi,graph}/**`; harness `testing/tools/harness/map2d/run_parallel_port_matrix.py` (`engine=vista`)

### Why

Equal-latitude matrix (Debug china 1280×720, `MAP2D_NO_HILLSHADE=1`): leftover IR `execute_ms` ≈ **327–531 ms**; Vista `paint_ms`/`export_ms` ≈ **2161/2173**; GPU cold ≈ **30694** (`gpu_upload_ms≈29313` + `gpu_present_ms≈1381`); **warm = 0** (StaticReuse OK). Confirmed: metric asymmetry (IR vs MapFrame); cold per-mesh VB/IB in `upload_draws`; full GDI software; dual software+GPU doubles matrix wall. Need **phased** cut — protect warm, crush cold upload first — **without** deleting carto.

### Profile (locked)

| Axis | Value |
| --- | --- |
| Extent | China mainland `[80,16]–[128,52]` (same as leftover / showcase) |
| Viewport | **1280×720** (`MAP2D_SHOWCASE_W/H`) |
| Sample | `china_city` + product Style; matrix equal-latitude → `MAP2D_NO_HILLSHADE=1` |
| Leftover metric | `RHI2D_PARALLEL_LOG` → `execute_ms` (**IR replay only**) |
| Vista metrics | `export_ms` / `paint_ms`; `present_gpu_{cold,warm}_ms`; phase clocks (`gpu_upload_ms`, …) |

### Decisions

| Topic | Choice |
| --- | --- |
| Compare fairness | Phase clocks + CSV `note`; never claim leftover execute ≡ export/present |
| Product north star | **Warm** GPU StaticReuse present ≤ **80** ms (**met**); warm upload ≈ 0 (**met**) |
| Cold first frame | Budget cold present ≤ **400**, cold upload ≤ **200**; **P0 = mega-buffer / batch-by-pipeline** (not strip MapFrame) |
| Software export | Faithful default; `paint_ms` ≤ **100** via GDI batch (**P2**); `EXPORT_REUSE` bench-only |
| Layout | Incremental / LOD (**P1**); `layout_ms` cold ≤ **60** |
| Parallel grain | **P3** → `VISTA_LAYOUT_PARALLEL` (§vista parallel); do **not** copy leftover opaque wipe |
| Diagram | No new HTML — reuse `render-accelerate-topology.html` |

### Workstreams P0–P3 (see plan)

| Phase | Focus | Surfaces | Budget |
| --- | --- | --- | --- |
| **P0** MUST | Cold upload merge | `upload_draws` / `Pass::record` | cold upload ≤200; cold present ≤400 |
| **P1** | Layout / incremental | `Map2dFrameCache::prepare_for_present` / Layout | `layout_ms` ≤60 cold |
| **P2** | Software GDI batch | `Map2dSoftwarePainter::paint` / frame GDI | faithful `paint_ms` ≤100 |
| **P3** | Parallel + false-gap label | `VISTA_LAYOUT_PARALLEL` + matrix CSV | labeled IR ≠ MapFrame |

Done already (plan Task 1–3 partial): phase clocks; warm/cold split; device reuse; avoid timed invalidate; DrawCache StaticReuse; hillshade cache; EXPORT_REUSE bench. **P3 harness (2026-10-03):** matrix prints leftover vs Vista phase tables; CSV/`NOTE.txt`/`matrix_note` carry FALSE-GAP + equal-latitude (`MAP2D_NO_HILLSHADE=1`); harness sets `VISTA_LAYOUT_PARALLEL=1` — product `getenv` wire remains open (P3a / parallel V1; avoid conflicting with P1 `gis/vista` edits).

### Acceptance

- Matrix still writes leftover + `vista-china` BMPs at 1280×720 (`parallel_port_matrix_with_vista.*`)  
- Warm `present_gpu_*` ≤ **80** (**met**); warm upload ≈ **0** (**met**); cold first ≤ **400**; cold upload ≤ **200**; faithful `paint_ms` ≤ **100**  
- Visual: labels (+ hillshade when DEM on) remain on inspect PNG  
- False-gap kept labeled in matrix `note` / harness tables  

### Non-goals

- Delete hillshade / MapFrame to match leftover IR-only ms  
- MapLibre Native shader port  
- Making leftover the product default  
- Treating dual software+GPU matrix `wall_ms` as product bug (label; optimize phases)  

---

## §src_render Scene3d equal-profile optimize（2026-10-01）

**Status:** active  
**Updated:** 2026-10-03 — **world3d equal-profile matrix + cold vs warm**（P0 = FlyCube cold upload；warm already peer）；`GPUSCENE_PREP_PARALLEL` **default off** until frustum cull（cross-link §vista parallel）  
**Plan:** [`../plans/2026-10-01-src-render-scene3d-equal-profile-optimize.md`](../plans/2026-10-01-src-render-scene3d-equal-profile-optimize.md)（M1–M4）  
**Parallel contract:** [`../plans/2026-10-02-src-render-vista-parallel-accelerate.md`](../plans/2026-10-02-src-render-vista-parallel-accelerate.md) Task 4 (`prep_cull_parallel`；default-off)  
**Diagram:** [`../diagrams/render-accelerate-topology.html`](../diagrams/render-accelerate-topology.html) §8 Scene3d cold vs warm  
**Code:** `content/browser/present/scene3d/**`, `vista/scene/**`; harness `--atmosphere-showcase=legacy` · `--plugin-showcase=world3d` · `testing/tools/harness/plugin/run_world3d_backend_matrix.py`

### Why (atmosphere-showcase=legacy — landed warm path)

Equal-profile vs leftover scene3d (same 640×480 HWND): warm `ms_per_present` was **~160–444 ms** Debug while leftover sits near **~10 ms**. Root cause: `Scene3dGpuPresent::present` forced `mark_meshes_dirty` whenever ocean/sky was on, and `rebuild_local_mesh` cleared DEM buffers so terrain cache never hit — full tessellate+upload every frame. Warm path is now peer-order (~10–14 ms); see plan Task 1–3.

### Why (world3d matrix — active)

World3d equal-profile matrix (True-Earth DEM bare on FlyCube; leftover china stereo peers): **warm** `ms_per_present` already sits in leftover’s order (**~5–6.5 ms**). The remaining **P0** gap is **FlyCube cold** (~**1.8–2 s**) vs leftover cold (~**90 ms**) — first-frame cache + GPU upload, not warm StaticReuse. `prep_par_on` is currently **slower** than serial prep; product must keep `GPUSCENE_PREP_PARALLEL` **default off** until frustum cull makes parallel prep honest (§vista parallel Task 4).

### Profile — atmosphere legacy (locked)

| Axis | Value |
| --- | --- |
| Mode | `--atmosphere-showcase=legacy` (`kLegacyStereo`, ocean on, sky off) |
| Viewport | 640×480 (`kAtmosphereShowcaseW/H`) |
| Metric | warm `ms_per_present` (+ phase clocks) |
| Target | warm ≈ leftover **~10 ms** order; `rebuild_count=0` on timed frames |

### Profile — world3d equal-profile matrix (locked)

| Axis | Value |
| --- | --- |
| Entry | `--plugin-showcase=world3d` + `PLUGIN_WORLD3D_PERF_BARE=1` (FlyCube DEM-only; no sky/ocean/cloud/fog / pointcloud; `pump_ms=0`) |
| Scenic peers | `gl_scenic` / `d3d_scenic` via SmartGisViews + `SCENE3D_ENGINE=stereo_gl\|stereo_d3d` (`scenic_impl`) |
| Primary metric | **warm** `ms_per_present` — **n=5**, **discard_cold=1** (do **not** rank by process `wall_ms`) |
| Perf rows | `flycube`, `prep_par_off`, `prep_par_on`, `gl_scenic`, `d3d_scenic` |
| Smoke only | `null` (full materials; never a performance peer) |
| Artifacts | `out/Debug/captures/analysis/world3d_opt/matrix/` |
| Warm status | Peer ~**5–6.5 ms** (protect; do not regress) |
| Cold P0 | FlyCube cold ~**1.8–2 s** vs leftover ~**90 ms** → crush cache+upload |

### Decisions

| Topic | Choice |
| --- | --- |
| Ocean/sky remesh | Force DEM GPU remesh **once** after first height/depth alloc; warm in-place height upload keeps StaticReuse |
| DEM cache | Trim overlay fold; do **not** `clear()` local xyz before `rebuild_terrain_mesh` |
| Overlays | Re-attach only when DEM rebuilt or overlay dirty |
| Phase clocks | `mesh_ms` / `sync_ms` / `rebuild_ms` / `ocean_prep_ms` / `record_ms` / `present_swap_ms` (+ **cold** phase fields in matrix JSON — M1) |
| Cold vs warm | Warm = StaticReuse present budget; **P0** = cold first-frame upload/cache (peer Map2d cold-upload-first) |
| `prep_cull_parallel` | `GPUSCENE_PREP_PARALLEL` **default off** until `SCENE3D_FRUSTUM_CULL` + honest prep (§vista parallel Task 4); matrix still measures `prep_par_on` as experiment row |
| Submit | `Scene3dGpuPresent::present` calls `render::graph::present` (one CommandList, same seam as `Map2dGpuPresent::present_frame`). Warm frames keep `rebuild_count=0`. Do not turn `GPUSCENE_PREP_PARALLEL` on |
| Fairness | Compare `ms_per_present` / phases; leftover seed ≠ FlyCube bare — do not treat identical GPU work |

### Acceptance — atmosphere legacy

- [x] Warm legacy `ms_per_present` near leftover ~10 ms (Debug) — plan Task 3 (~10–14 ms)  
- [x] Timed frames report `rebuild_count=0`  
- [x] Visual: legacy BMP still passes landish / black-clear gate  

### Acceptance — world3d matrix (M1–M4)

- [ ] **M1** Cold phases in matrix / perf JSON (named cold upload / mesh / rebuild / record fields; discard_cold=1 warm primary)  
- [ ] **M2** Cache + upload: FlyCube **cold ≤ 300 ms** (vs ~1.8–2 s baseline; leftover ~90 ms peer)  
- [ ] **M3** Prep honesty: `prep_par_on` not slower than `prep_par_off` without cull; product default remains **off** until frustum cull  
- [ ] **M4** Non-bare budgets: document / gate full-atmosphere (non-`PERF_BARE`) warm+cold without stripping DEM  

---

## §Leftover scene3d per-object showcase（2026-10-02）

**Status:** active  
**Updated:** 2026-10-02  
**Plan checklist:** [`../plans/2026-10-02-scene3d-surface-base-modern-cpp.md`](../archive/plans/2026-10-02-scene3d-surface-base-modern-cpp.md)  
**Code:** `seed/map_to_scene.cc` (`seed_showcase_mode_into_scene`) · `host/stereo_hwnd_view.cc` · `legacy/app/shell/showcase/scene3d_showcase.cc` · harness `testing/tools/harness/legacy/legacy.scene3d.*`

### Why

Composite `china` showcase covers terrain + GeoObject + MapLabelBatch + northarray together. Cube / sphere / water / pointcloud render paths and northarray-only had no separately runnable BMP harness.

### Modes (`--scene3d-showcase <mode>` / `SCENE3D_SHOWCASE_MODE`)

| Mode | Seeds | Harness suite | score_id |
| --- | --- | --- | --- |
| `china` (default) | DEM + vectors + labels | `legacy.scene3d.china` (+ `.d3d` / browse) | `legacy_scene3d_china` |
| `terrain` | DEM underlay only | `legacy.scene3d.terrain` | `legacy_scene3d_china` |
| `cube` / `sphere` / `water` | single mesh at origin | `legacy.scene3d.<mode>` | `legacy_scene3d_mesh` |
| `pointcloud` | synthetic CSV → `PointCloud3d` | `legacy.scene3d.pointcloud` | `legacy_scene3d_mesh` |
| `northarray` | framing AABB only (HUD from `Scene::Setup`) | `legacy.scene3d.northarray` | `legacy_scene3d_mesh` |

`SmtSurfaceObject` (`surface_base`) is abstract — covered by `terrain` + `pointcloud` draw paths, not a separate suite.

### Acceptance

- [x] Modes selectable via argv/env; BMP leaf `legacy-scene3d-showcase-<mode>.bmp`
- [x] Per-object OpenGL harness suites under `testing/tools/harness/legacy/`
- [x] Suites green + visual review on captures (`cube`/`sphere`/`water`/`pointcloud`/`northarray`/`terrain`; `china` still green)

---

## §world3d Earth DEM / satellite cloud / atmosphere seams（2026-10-02）

**Status:** active  
**Updated:** 2026-10-02  
**Owning product face:** [`2026-09-13-plugin-host-design.md`](2026-09-13-plugin-host-design.md) §world3d True Earth  
**Plan:** [`../plans/2026-09-30-world3d-true-earth.md`](../plans/2026-09-30-world3d-true-earth.md) (P0b)

### Seam

| Capability | Render / content hook |
| --- | --- |
| DEM / terrain | `vista::set_sample_dem_path_override` → `find_sample_dem_path` → `terrain_mesh::rebuild_terrain_mesh` (honors override; does not force China box) |
| Atmosphere | `AtmosphereSession::{set_sky,set_ocean,set_cloud,set_fog}_enabled` + `seed_procedural` via `apply_china_scene3d_atmosphere` |
| Satellite cloud | `AtmosphereSession::load_fields("<tif>:cloud_cover")` → FieldStore `kCloudCover` → `CloudPass` cover modulation; missing file → procedural |

### Non-goals (this §)

- WGS84 spherical globe mesh / clipmap paging (still P2).
- Live meteorological imagery CDN.

---

## §src_render + vista parallel accelerate（终态 · 2026-10-02）

**Status:** active  
**Updated:** 2026-10-03 — `GPUSCENE_PREP_PARALLEL` **default off**（§Scene3d equal-profile / world3d M3；直到 frustum cull）；cross-link Map2d **P3**；既有 L0–L3 详设不变  
**Plan:** [`../plans/2026-10-02-src-render-vista-parallel-accelerate.md`](../plans/2026-10-02-src-render-vista-parallel-accelerate.md)（L0–L3）；GPU-process checklist → [`../plans/2026-09-27-gpu-rhi-accelerate.md`](../plans/2026-09-27-gpu-rhi-accelerate.md) Task 8  
**Equal-profile budgets:** Map2d [`../plans/2026-10-01-src-render-map2d-equal-profile-optimize.md`](../plans/2026-10-01-src-render-map2d-equal-profile-optimize.md) **P3** · Scene3d/world3d [`../plans/2026-10-01-src-render-scene3d-equal-profile-optimize.md`](../plans/2026-10-01-src-render-scene3d-equal-profile-optimize.md) **M1–M4**  
**Diagram (normative):** [`../diagrams/render-accelerate-topology.html`](../diagrams/render-accelerate-topology.html)（§2–§3 L0–L3 + 命名阶段 `stage_frame` → `build_layout_parallel` → `prep_cull_parallel` → `record_and_present`）  
**CPU boundary diagram:** [`gis-vista-architecture.html`](../diagrams/gis-vista-architecture.html)（MapFrame POD 穿墙 → content/effect/graph）  
**Present layering diagram:** [`map2d-present-frame.html`](../diagrams/map2d-present-frame.html)（§Map2d present；本 § 仍只锁并行机器）  
**Related diagrams:** [`ui-views-shell-architecture.html`](../diagrams/ui-views-shell-architecture.html) · [`legacy-render-architecture.html`](../diagrams/legacy-render-architecture.html)  
**Code:** `src/vista/**` · `src/content/browser/present/{map2d,scene3d}/**` · `src/vista/{map,map_pass,world,world_pass,atmosphere}/**` · `src/render/{rhi,graph,scene}/**` · `src/base/execution/**`  
**Reference (leftover as-built):** §rhi2d leftover tile-raster · §rhi3d leftover parallel frame  
**Visual rule:** `.cursor/rules/repo/design-html-diagrams.mdc`（技术架构浅色 · SVG-first）

### Why

Leftover proved Chromium-like **off-UI FrameJob + CPU prep×N + serial GPU submit** (D3D11 deferred optional). Product already has fragments（`emit_fills`/`emit_lines` `parallel_for`、`batches_via_parallel_for`、`prepare_for_present` StaticReuse、`graph::present` 单 CL）但缺一份**对接真实符号**的终态详设。Equal-profile §§ 管 **ms / StaticReuse 预算**；本 § 管 **线程 / 调度 / API / 热点路径 / env**。

### Core layers (L0–L3 · locked)

| Layer | Owner | May | Must not |
| --- | --- | --- | --- |
| **L0 UI** | Views HWND / Commit | stage FrameRequest + gen; shell DisplayList Commit; Submit | `join` layout / GPU workers; touch `rhi::Device` |
| **L1 Schedule** | `content` Map2d / Scene3d | gen / cancel / StaticReuse / phase clocks; wake workers | Own a second thread pool; issue HDC draws from the scheduler (software HDC stays on `paint_map_frame_gdi`, §Map2d present) |
| **L2 CPU** | `vista` (+ GpuScene prep helpers) | `parallel_for` tess / optional tile layout / frustum cull; TLS scratch | `#include` `render/rhi`; HWND; record CommandList |
| **L3 GPU** | Display thread · `render::graph` / `effect` | sole `rhi::Device`; sync → **one** CL → `present`; publish gen | Multi-thread Draw on shared GL; default multi-CL |

Sole pool: **`base::execution`** (`GlobalNThreadPoolExecutor` / `parallel_for` in `src/base/execution/parallel/for.h`).

---

### 1. Types & ownership（谁分配什么）

| Type | Header / owner | Allocates | Consumed by | Notes |
| --- | --- | --- | --- | --- |
| `vista::LayoutInput` | `vista/component/map/layout.h` · content fills | View + style* + tiles + `GlyphMetrics*`（非拥有） | `Layout::build` | 无 RHI；晕渲烘焙是 `bake_hillshade_slot`（§Map2d present）；content 持 RGBA 供 `load_raster` |
| `vista::LayerBatch` | same · `build_layer_batches`（`vista::detail` / `carto_filter`） | POD features（content 在边界把 `MapScene` 拷成 POD） | `Layout::build` | vista 不 `#include` `map_scene.h`；签名 `Layout::build(LayoutInput, vector<LayerBatch>)` 不变 |
| `vista::MapIR` | `vista/component/map/ir.h` · **cache 拥有** `cached_frame_` | `background_rgba` + `DrawItem[]`（`DrawBlend` 在 `vista/component/map/draw.h`，不 include `rhi.h`） | `vista::MapPass` / `paint_map_frame_gdi` | POD 穿 `gis ↛ rhi` 墙 |
| `vista::Layout` | `layout.cc` · 无状态 | TLS arena clear 于 build 开头 | — | 纯函数式 CPU |
| `content::Map2dFrameCache` | `map2d_frame_cache.h` | MapFrame、hillshade RGBA、fingerprint、`layout_scratch_` | `Map2dGpuPresent` / software | `mu_` 保护 prepare/rebuild |
| `PresentAction` | cache enum | — | present 路径分支 | `kRebuildFull` / `kInteractiveReuse` / `kSettleRebuild` / `kStaticReuse` |
| `vista::MapPass` | `vista/pass/map/pass.h` · present 拥有 `map2d_pass_` | GPU VB/IB/纹理 upload 缓存 | `graph::present` via `MapEffect` | `invalidate_uploaded` 仅 cold |
| `vista::WorldPass` | `vista/pass/world/pass.h` | `GpuMesh` / pipelines / instances | `record_draws` → graph opaque | remesh 在 Device 线程 |
| `render::graph::ViewInput` | `render/graph/frame_graph.h` | effect* 列表（非拥有） | `graph::present` | 1 camera · 1 CL |
| `render::rhi::Device` | Display / viewport | sole；create/destroy CL | `graph::present` | UI 永不持有写侧 |

**Gen / cancel（目标语义，对接 as-built）：**

- **Warm StaticReuse**：`prepare_for_present` → `kStaticReuse` 且 `last_present_ok_` → **跳过** Pass re-record + `graph::present`（china FPS 热路径）。
- **Cold**：`kRebuildFull` / `kSettleRebuild` / `!last_present_ok_` → `invalidate_uploaded` + 全量 `Pass::record`。
- **V2 gen**：引入单调 `layout_gen`（cache）与 optional `present_gen`；worker 完成时若 gen 过期则丢弃、不 publish；destroy 置 cancel bit，**HWND 不 join** layout future。

---

### 2. Call graph（warm StaticReuse vs cold）

#### Map2d — warm StaticReuse（目标 / as-built 已接近）

```
L0  Views Invalidate / Display mailbox Submit
      └─ never join
L1  Map2dPresenter::present_gpu
      └─ Map2dGpuPresent::present(device, w, h, shell, shell_gen)
           ├─ cache_->prepare_for_present → PresentAction::kStaticReuse
           ├─ reuse_action && last_present_ok_ && shell_stable
           │     → note_map2d_phase_gpu(0,0); return true   // 无 L2/L3
           └─ (else fall through cold-ish shell change)
```

**Files:** `map2d_presenter.cc` · `map2d_gpu_present.cc` · `map2d_frame_cache.cc` · `map2d_phase_profile.*`

#### Map2d — cold rebuild（layout + GPU）

```
Map2dGpuPresent::present
  └─ prepare_for_present → kRebuildFull | kSettleRebuild
       └─ Map2dFrameCache::rebuild_layout(cam, fp)          // L1 当前仍同步
            ├─ bake_hillshade_slot (vista terrain; content holds RGBA)
            ├─ async(visible_layer_batches) → get()         // 池任务
            │     └─ detail::batches_via_parallel_for       // map2d_batches.cc
            └─ Layout::build(in, batches)                   // L2 CPU
                 ├─ detail::emit_fills  → parallel_for      // fill.cc
                 ├─ detail::emit_lines  → parallel_for      // line.cc
                 ├─ emit_circles / heatmap / extrusion      // 目标：同 grain
                 └─ emit_symbols (+ LabelGrid)              // 串行 collision
  └─ present_frame → MapEffect(Pass, &cache_->frame(), …)
       └─ render::graph::present(device, ViewInput)         // L3
            ├─ device->create_command_list()
            ├─ record_effects → Pass::record (upload+draw)
            ├─ list->close(); device->execute; present
            └─ destroy_command_list
```

**Phase clocks（as-built）：** `note_map2d_phase_layout(layout_ms, hillshade_ms)` · `note_map2d_phase_gpu(upload_ms, present_ms)`（`Pass` 内 `last_pass_record_ms` ≈ upload/record）。目标补齐命名：`prep_ms`（3D）与 V2 异步 layout 的墙钟分离。

#### Scene3d — GPU present（`graph::present`，1 CL）

`Scene3dGpuPresent::present` 只组视口、look、大气开关、orbit 相机和 `render::graph::ViewInput`（Null 后端相机为空，避免误开 frustum），然后调用已有 `render::graph::present`。与 `Map2dGpuPresent::present_frame` 同一条缝：一次 CommandList。不把 shell 四边形放进 `kOverlay`。

```
Scene3dPresenter::present_gpu
  └─ Scene3dGpuPresent::present
       ├─ OceanPass::prepare_gpu          // 仅冷帧需要 ocean height
       ├─ GpuScene::sync_from             // 温帧禁止第二次 sync_from
       ├─ mark_meshes_dirty               // 仅冷帧；温帧不置 dirty
       └─ render::graph::present(ViewInput)
            ├─ pre_effect  → record_pre_opaque     // sky/depth；ocean 关
            ├─ OpaqueEffect → record_draws         // globe_on 时不放入 list
            │     └─ meshes_dirty_ → rebuild_meshes
            └─ post_effect → record_post_opaque    // 平坦海洋，然后 cloud/fog/sat
```

记录顺序：sky/depth → opaque DEM → ocean → cloud/fog。冷帧在 `graph::present` 之前置 `meshes_dirty_`，由 `record_draws` 里已有分支 rebuild（pre 的 depth 分配先发生）。温帧 `dem_gpu_synced_after_sky_` / `dem_gpu_synced_after_ocean_` 为真时不置 dirty，`rebuild_count=0`。不打开 `GPUSCENE_PREP_PARALLEL`。

Frustum cull 仍是 `vista/scene/frustum_aabb.cc` + `detail::frustum_cull_enabled()`（默认关；`SCENE3D_FRUSTUM_CULL=1`）。启用 cull 时 `prep_cull_meshes` 先走 `AabbOctree`（unibn 中心半径候选），再 `aabb_intersects_frustum`；失败则线性扫描。可见性标位可以 `parallel_for`，record 仍串行读标位。见 **§Vista scene unibn octree**。

---

### 3. Parallel grains（统一 job→merge）

| Grain | As-built | Target rule |
| --- | --- | --- |
| Fill / line tess | `emit_fills` / `emit_lines`：collect `FillJob`/`LineJob` → `parallel_for` → **按 layer_ord 有序 merge** | 保留；加 `VISTA_LAYOUT_PARALLEL=0` → 强制串行 |
| Batch collect | `batches_via_parallel_for` per visible layer → `merge_parts` | 保留；属 L1 数据准备，非 RHI |
| Circle / heatmap / extrusion / symbol | 大多串行 | **同一模式**：job 向量 → `parallel_for` 写 per-index scratch → 保序 merge |
| Labels / collision | `LabelGrid` 在 `emit_symbols` | **必须串行**（在 parallel emit 之后）；禁止 worker 写共享 grid |
| Optional tile layout (V4) | 无 | device-pixel AABB 切 Layout 子任务；仍产出 **一个** `MapFrame`；GPU compose 走 `vista/component/map`，软件出口仍是 `paint_map_frame_gdi`（§Map2d present） |
| GpuScene prep (V3) | `mesh_culled` 在 record 内串行 | `prep_cull_parallel`：每 mesh 写 `visible[i]`；`worker_hint` clamp 2–4 |
| Worker 禁令 | TLS `mesh_scratch` / arena OK | **禁止** `rhi::Device`、HWND、`CommandList`、共享 GL context |

常量：`kParallelTessMinGeoms` / `kParallelTessGrain` in `vista/component/map/layout/tess_grain.h`（今日 `MinGeoms=2`）。

统一伪模式（所有 L2 emitter）：

```cpp
// job collect (serial) → parallel_for body(i) → ordered merge (serial)
if (!vista_layout_parallel_enabled() || jobs.size() < kParallelTessMinGeoms) {
  // N=1 path
} else {
  base::execution::GlobalNThreadPoolExecutor executor;
  base::execution::parallel_for(executor, 0, jobs.size(), body, grain);
  merge_in_painter_order(...);
}
```

---

### 4. GPU accelerate path

| Topic | Lock |
| --- | --- |
| Upload batching | Cold：`Pass::record` / `GpuScene::rebuild_meshes` 在 **Display 线程** 批量 upload；Warm StaticReuse：**零 upload** |
| StaticReuse | Map2d：跳过 `graph::present`；Scene3d：equal-profile `rebuild_count=0` + 跳过 remesh |
| `graph::present` | **永远** `create_command_list` → `record_effects` → `close` → `execute` → `present` → destroy — **1 CL**（`frame_graph.cc`） |
| When NOT multi-CL | `record_ms` 未成为 equal-profile 热点；GL 共享 context；任何 leftover D3D11 deferred TLS |
| Effect slots | `kBeforeOpaque` / `kOpaque` / `kAfterOpaque` / `kOverlay` — 顺序固定；并行只发生在 slot **之前**的 CPU prep |
| FlyCube compute | `OceanGpuFields::record`（`vista/pass/atmosphere/ocean/gpu_fields.cc`）在 **同一 Device 线程**、可在 present CL 前/内提交 compute；**不是** worker 线程开 Device |
| Equal-profile | 本 § 不改预算数字；并行只服务于 `layout_ms` / `prep_ms` 下降，且不得破坏 warm present 上限 |

---

### 5. Env & fallbacks

| Env | Default | Effect | Scope |
| --- | --- | --- | --- |
| `VISTA_LAYOUT_PARALLEL` | on（jobs ≥ `kParallelTessMinGeoms`） | `=0` → tess N=1 | **product** · vista emitters |
| `GPUSCENE_PREP_PARALLEL` | **off**（default；直到 frustum cull 诚实） | `=1` → frustum prep N clamp 2–4；`=0` → N=1 | **product** · effect/scene prep · §Scene3d equal-profile M3 |
| `SCENE3D_FRUSTUM_CULL` | off（as-built） | `=1` 启用 AABB cull（并行 prep 的前提） | product · 与 equal-profile 联调 |
| `RHI2D_PARALLEL` / `RHI2D_TILE_RASTER` | leftover | **禁止** product 路径 `getenv` | leftover only |
| `RHI3D_FRAME_JOB` / `*_PREP_PARALLEL` / `*_D3D_DEFERRED` | leftover | **禁止** product 路径 | leftover only |

文档入口：`src/render/README.md`「Parallel / GPU (planned)」→ 本 § + 图。

---

### 6. Pseudocode / API sketches（对接现有类，不另起平行树）

命名 `snake_case`；下列为 **契约钩子**，优先落成现有方法的拆分/异步包装，而非新 DLL 面。

```cpp
// L1 — content/browser/present/map2d/frame/map2d_frame_cache.*
// stage: UI/Display 只投递；不 join。
struct FrameRequest {
  uint32_t width_px = 0, height_px = 0;
  uint64_t layout_gen = 0;  // V2
};

// Target split of today's synchronous prepare_for_present + rebuild_layout:
bool stage_frame(Map2dFrameCache* cache, const FrameRequest& req);
// worker (base::execution::async):
bool build_layout_parallel(Map2dFrameCache* cache, uint64_t layout_gen);
// publish only if gen matches; else drop.

// L2 — vista/component/map/layout/*  (already: emit_fills / emit_lines)
void build_layout_parallel_emitters(const LayoutInput& in,
                                    const std::vector<LayerBatch>& layers,
                                    MapFrame* out);  // == Layout::build body

// L2 — effect/scene (V3); CPU only
void prep_cull_parallel(const FrustumPlanes& planes,
                        std::span<const GpuScene::GpuMesh> meshes,
                        std::span<char> visible_out);

// L3 — content present_* + render/graph/frame_graph.cc
bool record_and_present(render::rhi::Device* device,
                        const render::graph::ViewInput& in);
// == render::graph::present; single CommandList; Display thread only.
```

**As-built 锚点（勿改名 unless rename blast）：**

- `Layout::build` · `emit_fills` · `emit_lines` · `Map2dFrameCache::rebuild_layout` / `prepare_for_present`
- `batches_via_parallel_for` · `Map2dGpuPresent::present` / `present_frame`
- `GpuScene::record_draws` · `detail::record_kind` · `render::graph::present`
- `base::execution::parallel_for`

---

### 7. Non-goals（硬）

- 不把 `Rhi2dTileGraphRunner` / 每线程 GDI HDC / TransparentBlt / ocean color-key 迁入 FlyCube
- 不引入 D3D11 deferred context TLS 作为产品默认；P3b 仅限 FlyCube multi-CL bundle
- 不在 `gis` / `render` 再开第二套线程池
- **`gis` ↛ `render/rhi` / HWND**；MapFrame 仅 POD 穿越
- N cameras / N CLs 每 `graph::present`（除可选 P3b）
- Leftover matrix 不作产品 A/B 真值

---

### As-built vs target（摘要）

| Piece | As-built | Target |
| --- | --- | --- |
| Fill / line `parallel_for` | Yes | + `VISTA_LAYOUT_PARALLEL` |
| Symbol / circle / heatmap / extrusion | Partial serial | Unify job→merge (V1) |
| `batches_via_parallel_for` | Yes | Keep under L1 |
| FrameJob UI-never-joins layout | Partial（present skip）；layout 仍同步 | `stage_frame` / async build (V2) |
| GpuScene frustum prep×N | Serial in `record_kind` | `prep_cull_parallel` (V3) |
| Tile layout grain | No | Optional V4 |
| Product env wire | Not wired | V0/V1 doc + code |

### Legacy → product mapping（locked）

| Leftover | Product |
| --- | --- |
| rhi2d FrameJob / Impl | L1+L3：`stage_frame` → build → `record_and_present`；L0 never join |
| tile / layer Raster×N | L2 tess + optional tile **layout**；compose = `vista/component/map` |
| rhi3d PrepRunner | `prep_cull_parallel` |
| D3D11 deferred | 默认 1 CL；禁 TLS deferred |

### Phases

| Phase | Deliverable | Status |
| --- | --- | --- |
| **V0** | Living 代码级 § + HTML 命名阶段 + README 指针 | **done（本修订）** |
| **V1** | 统一 emitters + `VISTA_LAYOUT_PARALLEL` | open |
| **V2** | `stage_frame` / async layout / gen cancel | open |
| **V3** | `prep_cull_parallel` + `GPUSCENE_PREP_PARALLEL` | open |
| **V4** | Optional viewport tile layout | open |
| **V5** | P3b multi-CL if `record_ms` hot | deferred |

### Relation to other §§

- **§rhi2d tile-raster** / **§rhi3d parallel frame**：leftover 语义参考 only。
- **§content present accelerate**：present 树 C/G/E 采纳（cache 分层、GPU 合成、Pipeline 重叠）；本 § 仍只锁 L0–L3 机器。
- **§Vista Map2d / §src_render Scene3d equal-profile**：ms + StaticReuse；Map2d **P3** 消费本 § 的 `VISTA_LAYOUT_PARALLEL`；本 § 供并行机器。
- **Views §compositor thread**：L0/L3 线程角色。
- **§GPU-process accelerate**：双拓扑 — **本 § = Topology A**（in-process L3 Display + `graph::present`）；**§GPU = Topology B**（`--type=gpu` hub + `FrameComposer`）。L0–L2 语义共享；L3 按模式 remap。**统一规范图** [`render-accelerate-topology.html`](../diagrams/render-accelerate-topology.html)；Checklist 见 §GPU / Task 8。

### Acceptance

- [x] Living § + plan + **统一规范图** [`render-accelerate-topology.html`](../diagrams/render-accelerate-topology.html)（L0–L3 + 命名流水线 + Topology B）
- [x] 代码级 Types / Call graph / Grains / GPU / Env / API sketches 落档
- [ ] Product env 在代码中接线 + `src/render/README.md` 已链（README 指针已加；**接线仍 open**）
- [ ] Map2d china：`LAYOUT_PARALLEL=0` 对照墙钟下降；warm StaticReuse 预算仍成立
- [ ] Scene3d：prep workers 不碰 RHI；warm `rebuild_count=0`
- [ ] Destroy / navigate：无 join hang；stale gen 永不 present
- [ ] Visual gates 不变（hillshade / labels / atmosphere legacy）

---

## §Scenic（product cut · content host）（2026-10-04）

**Status:** accepted  
**Updated:** 2026-10-04 — Scenic copy `geom.h`/`math.h` retired: Point/Rect/vectors are `namespace base` (`base/math`). Copy export macros `LEGACY_RENDER_*` → `SCENIC_IMPL_*` / `SCENIC_RHI2D_*` / `SCENIC_RENDER_{GL,D3D}_*`. `base/math/math.h` is an aggregator only (no `render`/`geo`/`scenic` namespace). Prior same day — Root split: public umbrellas stay `engine.h` / `scenic_export.h`; product stubs + `mem_frame.h` in `engine/`. Prior same day — Dissolved `scenic/render/detail`. **Lock:** `scene3d/` stays peer of `vista/scene`; **do not** sink into `rhi3d/impl/common`.  
**Diagram:** [`../diagrams/legacy-render-architecture.html`](../diagrams/legacy-render-architecture.html)（CPU Raster×N / Prep×N，GPU 仅 rhi3d；与 Vista 对照在图 §4） · [`../diagrams/vista-subdirectory-layers.html`](../diagrams/vista-subdirectory-layers.html)  
**As-built:** [`../../../src/scenic/README.md`](../../../src/scenic/README.md) · leftover strangler [`../../../src/legacy/render/README.md`](../../../src/legacy/render/README.md)

**Scenic** is the previous-generation map/scene **engine**, cut out of leftover so **`src/content` can host complete Scenic capability**. Vista remains the current-gen GPU / MapFrame path. Dual-run is **content-hosted** (Vista present vs Scenic engine), not leftover MFC HWND as the long-term host.

| Topic | Choice |
| --- | --- |
| Name | **Scenic** |
| Job | Same *job* as Vista: 2D map frame + 3D world present — **different engine** |
| Host | Env on content (`prefer_map2d_scenic`). `MAP2D_ENGINE=scenic` attaches Scenic rhi2d on the Views map HWND (`ScenicRhi2dHost` / `Renderer2d`). `scenic::Engine` DrawItem is fallback. Copy internals stay in `scenic_impl` + port DLLs; product ↛ `src/legacy` |
| Disk target | **`src/scenic`** — peer of `src/vista`. Not under `vista/`, not under `src/render/rhi/` |
| Disk layout | Root umbrellas **`engine.h`** + **`scenic_export.h`**. Product stubs in **`engine/`**. Copy shared **`render/{err,frame,scenic_impl_export}.h`** + `backend_dll.h`; Point/Rect/vectors from **`base/math`** (no scenic `geom.h`/`math.h`). **`render/{rhi2d,rhi3d}`**; **`scene3d/`**. No leftover dump at `scenic/detail/` or `scenic/render/detail/`. No forwarding headers at old root paths |
| Strangler disk | **`src/legacy/render`** until a real extract |
| DLL | `//src/scenic:scenic` **shared_library**, `dll_stem = scenic` → `scenic.dll` / `scenic_d.dll`. Export `SCENIC_EXPORT` / `SCENIC_EXPORTS`. **Not** in `src_all`. `assert_no_deps` leftover render **and** leftover GIS |
| Copy internals | `src/scenic/{render,scene3d}` opt-in `scenic_copy_all` (`scenic_impl`, `scenic_rhi2d_*`, `scenic_render_gl|d3d`). Map paint under `scenic/render/rhi2d/.../paint/{map,carto}`. **Must not** `#include "legacy/…"`. Product `gis::` / `vista::` only. **Not** in `src_all`. Distinct stems vs `legacy_render*` |
| Merge | **Do not** merge into `vista.dll` or `render.dll` |
| Ports | Scenic rhi2d/rhi3d are **not** `render::rhi::Device` / FlyCube peers |
| Direction | Product Scenic DLL ↛ leftover. Copy internals use product GIS/vista + `base/math` Point/Rect/vectors + rhi2d `Viewport` + scene3d tess. Map/scene paint takes `gis::style::ResolvedPaint` from `gis/style` (not a scenic-owned Style class). After extract, leftover **wraps** Scenic |
| Namespace | Public **`scenic`** only (detail `scenic::detail`). Directory `engine/` / `render/` / `scene3d/` do **not** create third public namespaces. Scenic-owned types drop `Smt` / `Rhi2d` / `Rhi3d` prefixes (`MapPainter`, `Object3d`, …). Errors are `scenic::detail::Err` (`kErrNone`, …) |
| Not Scenic | FlyCube `src/render/rhi`; leftover MFC HWND host |

**Vista mirror (directory):** Scenic 2D map paint stays under `scenic/render/rhi2d/.../paint/{map,carto}` (not a top-level `scenic/map` peer of `vista/component/map`); `scenic/scene3d` ↔ `vista/scene`; Scenic device backends stay under `scenic/render/` (Vista consumes FlyCube `src/render`).

**3D peer lock (do not invert):** `rhi3d/impl/common` is **only** GL+D3D FrameJob / CPU prep (`frame/{scheduler,prep_runner,frame_request}`). It is **not** the Scenic peer of `vista/scene`. Scene graph, HWND stereo (`stereo_hwnd_view` owns `Scene`+seed), and Object3d deferred partition (`d3d_deferred_objects.h`) stay in `scene3d/`. The D3D **context pool** stays in `rhi3d/impl/d3d/host/deferred_draw.*`. Moving the scene tree under `impl/common` would make device TUs `#include` `Object3d` / `Scene` (layer invert) and break the Vista mirror.

**完备功能 (content):** Vista+FlyCube is the default MapFrame dual exit. Scenic rhi2d is the equal-compare / opt-in map stack (`MAP2D_ENGINE=scenic`, `RHI2D_PORT`). `scenic::Engine` DrawItem remains fallback. Env: `SCENE3D_ENGINE=scenic`, `MAP2D_ENGINE=scenic`.

### Checklist (this umbrella — no dated plan)

- [x] Product façade `scenic::Engine` + Null + Map2d/Scene3d stubs + `scenic.dll` (`SCENIC_EXPORT`) + `scenic_engine_test`
- [x] Content presenters host Scenic rhi2d (`ScenicRhi2dHost` / `Renderer2d` / `gis::Map` / port DLLs) when `MAP2D_ENGINE=scenic`; DrawItem `scenic::Engine` is fallback
- [x] Compile isolation: `//src/scenic:scenic` GN-↛ leftover render (`assert_no_deps`); `ninja scenic` alone
- [x] Working copy `src/legacy/render` → `src/scenic` (cp, leftover frozen)
- [x] Subdir: merge `detail`+`rhi2d`+`rhi3d` → `scenic/render/`; map paint remains under `rhi2d/.../paint/{map,carto}` (not extracted to top-level `scenic/map`); `scene3d/scene/` hosts deferred helpers; no old-path forwarding headers
- [x] Lock scene3d **not** under `rhi3d/impl/common`; `impl/common/README.md` + §Scenic peer table
- [x] Strip scenic-owned `Smt*` type names; `base/math` Point/Rect + rhi2d viewport + scene3d `feature_mesh`; carto paint uses `gis::style::ResolvedPaint` / `fill_resolved_paint_from_ogr_feature` (no scenic `Style` POD / leftover dump dirs)
- [x] Dissolve `scenic/render/detail` (`frame.h` product names; Point/Rect in `namespace base`)
- [x] Split copy types: export macros in `scenic_impl_export.h`; STL/`ulong` there; one `scenic_impl.lib` autolink; shared `render/backend_dll.h`
- [x] Copy TUs: zero `#include "legacy/…"`; DEM/coord via `//src/vista`; OGR tess in `scene3d/primitive/feature/tess_*.cc`; map painter on product `MapLayer` / `OgrRasterLayer` / `ProviderTileLayer`
- [ ] Bind copy internals to `scenic::Engine` (façade ↔ `scenic_impl`) without MFC HWND in product TUs
- [ ] Extract 2D FrameJob / `PaintBackend` present path off leftover HWND dual-run
- [ ] Extract scene3d record/present without LoadLibrary leftover or scenic_impl HWND
- [ ] Leftover `src/legacy/render` adapters `#include "scenic/…"` (wrap; leftover stays frozen until then)
- [ ] Drop leftover DLL stems when MFC host is gone
- [x] Rename export macros `LEGACY_RENDER_*` → `SCENIC_IMPL_*` (binary stems can stay)

### Non-goals (this slice)

- Do **not** edit `src/legacy/**` (especially `src/legacy/render`) except when extracting a product seam first.
- Do not make `//src/scenic:scenic` GN-dep leftover render. Content `map_present` may GN-dep `scenic` + `scenic_impl` so the Views app can host the rhi2d stack.
- Do not dump `scenic` / `scenic_copy_all` into `src_all` / `//:all` / e2e / te.
- Do not put Scenic HWND `Init` on the product façade (`SessionDesc` is size-only).
- Do not export leftover `Smt_*` as the Scenic public API.
- Do **not** relocate `src/scenic/scene3d` into `render/rhi3d/impl/common` (or any rhi3d impl). 2D paint-under-rhi2d is **not** a precedent for 3D scene graph.

---

## §RHI2D public composition split（2026-10-04）

**Status:** accepted  
**Updated:** 2026-10-04 — **end state = rhi3d-shaped split interfaces**（`Device2d*` live in `render_device.h` + thin `RenderDevice2d`；`Map2dSession` bag **removed**；façade `device_api.h` still superseded）  
**Diagram:** [`../diagrams/rhi2d-device-api.html`](../diagrams/rhi2d-device-api.html)  
**Code:** public `{render_device,renderer}.h` · impl nested `MapRenderDevice::{Host,Interact,Present,Frame}`  
**As-built:** [`../../../src/scenic/README.md`](../../../src/scenic/README.md) · [`../../../src/scenic/render/rhi2d/impl/common/README.md`](../../../src/scenic/render/rhi2d/impl/common/README.md)

### Why this matches rhi3d

rhi3d: pure-virtual `Device*` in `render_device.h`; thin `RenderDevice3d` with `host()` / `frame()` / …; backends implement nested types. Callers hold `RenderDevice3d*` and use parts. **No session bag.**

2D uses the same thinking. Names are `Device2d*` so they do not collide with rhi3d `DeviceHost` / `DeviceFrame` in `scenic::detail`. Low-level Draw* / encoder / DIB / tiles / `eRDBufferLayer` stay **out** of `rhi2d/public` (3D parts include mesh/draw because that *is* the 3D product surface).

`scenic.dll` 产品面仍是 `scenic::Engine`（`Map2dEngine` 今日 `MemFrame`）。本 § 约束 copy 引擎在绑 Engine / HWND shell 时的 **rhi2d public**。

### Locked types（split interfaces）

| Type | Accessor | 外部用它做什么 | 3D analog |
| --- | --- | --- | --- |
| `Device2dHost` | `host()` | `init` HWND、`resize`、viewport / windowport / options、`on_timer` | `DeviceHost` |
| `Device2dInteract` | `interact()` | `preview_zoom_*`、`zoom_to_rect`、`lp_to_dp` | `DeviceTransform` / `xform()` |
| `Device2dPresent` | `present()` | `refresh`、`blit_to_dc`、`save_image`、published gen | `DeviceFrame::swap_buffers` |
| `Device2dFrame` | `frame()` | `schedule_redraw(map, urgent)` | none (2D FrameJob) |

命名空间 `scenic::detail`。方法 `snake_case`。无 `Map2dSession`。

### Public vs impl bar

**`rhi2d/public/` 允许**

- `render_device.h`：`Device2d*` 纯虚部件 + 薄 `RenderDevice2d`（四 accessors + 视口/pan-org 字段）。`CreateRenderDevice` LoadLibrary 工厂
- `renderer.h`：`CreateDevice` + `GetDevice()`（与 `Renderer3d` 同形）

**禁止出现在 public 头（留 `impl/` / 不导出）**

- `Draw*` / `RenderLayer` / `BeginRender` / `EndRender` / `eRDBufferLayer`
- `CommandEncoder` / `CommandBuffer` / `CartoDraw` / `MapPainter` overlay
- `OwnedSurface` / DIB pool / tile raster env / `LayerTreeHost` / `TileGraphRunner`
- `PresentController::stage_map_job` 等 debounce 细节
- `BufferImage` 编解码

Edit overlay 继续走 impl host encoder；需要对外时另开 overlay part（本波 **不** 提供）。

### Internal composition（`MapRenderDevice`）

已有 member：`PresentController present_`、`LayerTreeHost*`、`CartoDraw`、`MapPainter` overlay、四块 `OwnedSurface`。本波 nested `Host` / `Interact` / `Present` / `Frame` 绑 `this`，与 `GlRenderDevice::Host` 同形。leftover `Init` / `Draw*` 仍是 **impl 方法**（backend DLL），不是 public 虚表。

`present_controller()` 是 **impl** `PresentController`（debounce）。产品 blit 走 `present()` → `Device2dPresent`。

### Forbidden mixes（类型上）

| 禁止 | 为什么 |
| --- | --- |
| 用 `Device2dFrame` 同时 host-encode MAP/QUICK | 共享 DIB / 0xC0000409 |
| 用 `Device2dPresent` 当画笔 | blit 不持有 encoder |
| `preview_*` 当 tessellate | StretchBlt ≠ `schedule_redraw` |
| 产品 TU `#include "legacy/…"` | 产品↛leftover |

### Enforcement

1. 产品路径：`Renderer2d::GetDevice()` → `host()` / `interact()` / `present()` / `frame()`。
2. 删除 `map_session.h` / `Map2dSession` / `device_api.h`。
3. 新功能只加在 `Device2d*` 上，不恢复 god vtable。
4. 本 § + HTML（与 rhi3d 图互链）。

### Non-goals / follow-up

- **不做：** 改 leftover `src/legacy/render`；改 backend 体；把 `Map2dEngine` 接到 HWND（§Scenic 仍 open）；导出 overlay Draw*。
- **后续：** `Device2dPresent`/`Device2dFrame` 收走 `device_pass`/`PresentController` 的函数体（不再经 impl 方法转发）。

### Acceptance

- [x] Living § 锁 **rhi3d-shaped split**（`Device2d*` in `render_device.h` + thin device）+ 图
- [x] 删除 `Map2dSession` / `map_session.h`；nested `MapRenderDevice::{Host,…}`
- [ ] Engine / HWND shell 只走四部件
- [ ] leftover `Init`/`Draw*` 进一步收进 impl-only 头

---

## §Carto draw lattice seam（2026-10-04）

**Status:** accepted  
**Updated:** 2026-10-04  
**Diagram:** [`../diagrams/rhi2d-device-api.html`](../diagrams/rhi2d-device-api.html)（§4 制图 draw 输入）  
**Code:** `src/scenic/render/rhi2d/impl/common/paint/carto/draw/{lattice_view,draw_mesh,carto_draw}.*` · host `MapRenderDevice::DrawGrid`

### Why

`paint/carto/draw` is an immediate-mode carto rasterizer (OGR + device polylines + TIN). It must not `#include` plugin product types. Owning 2D XY lattice remains plugin `plugin::detail::OrthoLattice` (`gis/geo/grid` solvers stay on `geo::NodeField2d`). Draw consumes a **non-owning** view.

### Locked

| Topic | Choice |
| --- | --- |
| Draw input | `scenic::detail::LatticeView2d` (`nx`/`ny` + `sample_xy(ctx, col, row)`) |
| Conversion | Plugin / content / present bind the view **above** carto draw |
| TIN | Still `OGRTriangulatedSurface*` (OGR, not plugin) |
| GN | rhi2d device ports must **not** dep `//src/plugin/product/world3d:world3d_orthogrid_lattice` |
| Leftover | Frozen `src/legacy/render` may keep `OrthoLattice*` on `RenderDevice2d::DrawGrid` |

### Call chain

```
plugin OrthoLattice  --bind LatticeView2d-->  MapRenderDevice::DrawGrid
                                              --> CartoDraw::draw_grid
                                              --> GdiMeshDraw (LP→DP + GDI lines/nodes)
```

### Non-goals

- Do not move `OrthoLattice` into `gis.dll`.
- Do not edit leftover rhi2d.
- Do not teach carto draw `HexLattice` / analysis-writer types.

### Acceptance

- [x] carto/draw + MapRenderDevice `DrawGrid` take `LatticeView2d*`
- [x] no `plugin/…` include under `src/scenic/render/rhi2d`
- [ ] product plugin paint binds `LatticeView2d` when it next draws an ortho grid

---

## §rhi3d public API composition（2026-10-04）

**Status:** accepted  
**Updated:** 2026-10-04 — dropped `device_lanes.h` forwarding façade; `RenderDevice3d` **composes** `Device*` parts; GL/D3D nested types implement them.  
**Diagram:** [`../diagrams/rhi3d-public-api-lanes.html`](../diagrams/rhi3d-public-api-lanes.html)  
**Code:** `src/scenic/render/rhi3d/public/device/render_device.h` · impl nested `GlRenderDevice::{Host,Frame,Mesh,…}` / `D3dRenderDevice::…`  
**As-built:** [`../../../src/scenic/README.md`](../../../src/scenic/README.md)

`device_lanes.h` (non-virtual wrappers over one god vtable) was an intermediate. Callers and backends now use the **same** `DeviceHost` / `DeviceFrame` / `DeviceMesh` / … types. `RenderDevice3d` only exposes `host()` / `frame()` / `xform()` / `mesh()` / `client_array()` / `video()` / `fixed()` / `program()` / `textures()` / `overlay()`. rhi2d uses the same shape (`Device2d*` + `host()` / `interact()` / `present()` / `frame()`; see §RHI2D). Scenic `scenic_render_gl` / `scenic_render_d3d` **may change** this vtable (copy engine). Frozen leftover `src/legacy/render` is not edited.

### Locked

| Topic | Choice |
| --- | --- |
| Shape | **HAS-A.** Nested `GlRenderDevice::Mesh : DeviceMesh` holds `GlRenderDevice*` and owns the GL/D3D method bodies. |
| XOR | geometry: Mesh XOR ClientArray XOR Video; shading: Fixed XOR Program |
| Default | Mesh + Fixed (DEM) or Mesh + Program |
| Names | Part methods `snake_case` |
| Factory | `Create3DRenderDevice` still returns `RenderDevice3d*` |

### Non-goals

- Do not restore PascalCase god virtuals on `RenderDevice3d`.
- Do not edit leftover `src/legacy/render/rhi3d`.
- Do not merge into `render::rhi`.

### Acceptance

- [x] `Device*` parts + composed `RenderDevice3d` in `render_device.h`; `device_lanes.h` / `device_parts.h` gone
- [x] GL/D3D nested implementations; scene3d / camera / resource inlines / tests use parts
- [ ] P1 mix `DCHECK` in `begin_render`/`end_render`

---

## §Vista subdirectory tighten（2026-10-04）

**Status:** active  
**Diagram:** [`../diagrams/vista-subdirectory-layers.html`](../diagrams/vista-subdirectory-layers.html)（图已改为 **§Vista IR/Pass lanes** 的 IR/GPU 泳道。本 § 目录表里的 `frame/` / `scene/` 由该节取代）  
**As-built:** [`../src-layout.md`](../src-layout.md) · [`../../../src/vista/README.md`](../../../src/vista/README.md)  
**Precedent:** scene3d tighten [`../archive/plans/2026-10-01-scene3d-subdirectory-tighten.md`](../archive/plans/2026-10-01-scene3d-subdirectory-tighten.md) · atmosphere layout [`../archive/plans/2026-09-27-atmosphere-subdirectory-layout.md`](../archive/plans/2026-09-27-atmosphere-subdirectory-layout.md)  
**GIS MapFrame 泳道（输入侧）：** [`../diagrams/gis-vista-architecture.html`](../diagrams/gis-vista-architecture.html) · GDAL umbrella 不另开 dated spec。  
**Checklist:** 下列分阶即本 § 的可执行表（不新开 dated plan 只重述搬目录）。实现时每步可编译；scheme C，无转发头。

Vista 是视口所持的那一幅景象：正交 MapFrame 与透视 World 是同一层 DLL，不是第二颗行星，也不是 `render.dll`。上一代对照轨是 **Scenic**（产品 `src/scenic` / `scenic.dll`，宿主 `src/content`；leftover `src/legacy/render` **冻结**至切开完成）——**不**并进本 DLL。物理树是 **`src/vista/{frame,world,assets,mesh,terrain,domain,map,scene,atmosphere}`**，链 **`vista.dll`**。本 § 锁死责任层、依赖方向、目标子目录与收紧步骤。能力/并行仍见 §Map2d present、§src_render + vista parallel、§Atmosphere*。

### 现状诊断

| 事实 | 证据 |
| --- | --- |
| 产品真源 | `src/vista/**` + leftover `src/legacy/gis/vista` 编进 `//src/vista:vista` |
| 孤儿树 | **已删**（2026-10-04）：磁盘无 `src/gis/vista/`、`src/effect/`。CBM 可能仍有旧路径直到 `/auto-cbm-gen` |
| 模块 | CPU：`map`、`world`（节点图）、`assets`（模型 / 3D Tiles / 点云 codec）、`mesh`（CPU 三角化）、`terrain`（DEM，头文件无 `World`）、`atmosphere`（session + cpu_waves）。GPU：`map_pass`、`world_pass`、`atmosphere_pass` |
| 调用方 | `content` present（Map2d / Scene3d）、`app/views`、`plugin/product/world3d`、`gpu`、`legacy/render/scene3d`（leftover→product）。`gis/tile:tile_test` 与 `render/graph:frame_graph_test` **测试**可 deps `vista`；`gis.dll` / `graph_sources` 产品对象 **禁止** |
| 平面/过深 | **P2 已收**：无 `ingest/io/`、无 `pointcloud/buffer/`、无 `frame/detail/collision/` 子目录、CPU atmosphere 平铺。GPU `atmosphere/{ocean,cloud,…}` 保持 |
| God / 遗留气味 | `world.h` 仍前向 `Map` / `SmtLayer`；GPU 头 include guard 仍 `EFFECT_*`；CPU `*_sources` 仍挂 `//build:legacy`；部分 test 复制 `.cc` + `RENDER_EXPORTS`/`VISTA_EXPORTS` 避开 DLL 锁 |
| 产品↛leftover | `src/vista` 无 `#include "legacy/…"`（保持）。leftover adapters 可 deps `world_sources` |

### 目标分层（拥有 / 不拥有）

| 层 | 拥有 | 不拥有 |
| --- | --- | --- |
| `gis.dll` | Feature / MapLayer / Style / tile / geo kernels | MapFrame、World、RHI、HWND |
| vista **CPU** | Layout、World、assets、DomainSession、CPU atmosphere 场 | `render::rhi`、Views、FlyCube 类型 |
| vista **GPU** | Pass、GpuScene、AtmosphereFrame + 各 pass | 打开数据源、Style JSON 解析、chrome |
| `render.dll` | rhi Facade、`graph::present` / Effect **vtable** | 编译 `vista/component/map|scene|atmosphere` 源 |
| `content` present | MapScene→POD 边界、GDI/GPU 双出口、HWND host | 制图决策（留在 `Layout::build`） |
| leftover `legacy/gis/vista` | DemHeightField / dem→World / Y-up | 产品新 API；不得被 product TU include |

**依赖方向（锁死）**

```
leftover → vista → gis     OK
vista GPU → render::rhi    OK
content / app / plugin → vista    OK
gis ↛ vista
vista CPU source_set ↛ rhi
graph_sources ↛ vista map/scene/atmosphere 头
product ↛ leftover
```

同一 `vista.dll` 内：`frame_sources` → `map_sources`；`world_sources` → `assets_sources` + `terrain_sources`；`map_sources` → `mesh_sources` + `terrain_sources`；`scene_sources` → `world_sources` + `mesh_sources`。`assets` / `mesh` / `terrain` ↛ `world`。GPU atmosphere ↛ `vista::World`（场数据经 content/host 注入）。禁止 Qt。禁止把 leftover 引擎（**Scenic**）拉进本层。

### 目标子目录

模块身份停在 `src/vista/<module>`（嵌套上限：`src/<layer>/<module>`）。模块内只按**职责**再下一层；禁止 `ingest/io` 这类分类学再套。头与 `.cc` 同目录。Include：`"vista/<module>/…"`。**不要** `gis/vista/`、`effect/`、`render/scene/` 转发头。

| 目录 | 一句话职责 |
| --- | --- |
| `map/` | CPU 制图一次：`Layout::build` → `MapFrame` / `DrawItem`；hillshade bake 编排（U2 as-built） |
| `map/layout/` | 按几何类 emit（fill/line/point/symbol/raster…） |
| `map/detail/` | `carto_filter` ⊥ `batch_build`、`collision`、hillshade bake、MVT（无 RHI） |
| `map/` | 公开 IR / `Layout` / `place` / `multiply` |
| `world/` | 逻辑节点图 + AABB；`dem_seed` 把 DEM 写入 `kTerrain` |
| `world/pointcloud/` | 节点上的 chunk / LOD（缓冲与文件加载在 `assets/pointcloud`） |
| `assets/{model,tileset,pointcloud}/` | Assimp / 3D Tiles / LAS·LAZ·PDAL·文本。不依赖 `world` |
| `mesh/` | OGR/GIS → xyz+indices。公开 `"vista/mesh/tessellate.h"`。`map` 与 `world` 直接依赖。不依赖 `world` |
| `mesh/fill/` | 多边形 / ring fill tess（`fill_tess`） |
| `mesh/line/` | 描边 ribbon tess（`line_tess`；cap/join/dash） |
| `mesh/detail/` | `mesh_types` / append / scratch pools / process-trace |
| `terrain/{dem,process}/` | DEM 栅格、hillshade、land mask、bake cache。头文件不出现 `World` |
| `domain/` | `DomainSession` 缝 |
| `domain/atmosphere/` | CPU FieldStore / Environment / systems（文件平铺，去掉 `field/` `systems/`） |
| `frame/` + `frame/detail/` | 2D GPU `FramePass`（upload / encode） |
| `scene/` | GpuScene facade (`scene.h`) |
| `scene/index/` | unibn AABB octree |
| `scene/detail/` | tessellate / paint / upload / draw_pass / prep_cull |
| `scene/detail/mesh/` | envelope / kind tess / sync / tint / pipelines |
| `atmosphere/{frame,ocean,cloud,sky,fog,globe,common,detail}/` | GPU 大气（已落地，不合并进 `domain/`） |
| `legacy/gis/vista/` | leftover 适配，仍编进本 DLL |

公开命名空间两层：`vista` + `vista::detail`（CPU frame / World / GPU）；`vista::atmosphere`（CPU domain）。禁止公共 `vista::frame` / `effect::map`。新函数 `snake_case`。

### GN

| 目标 | 角色 |
| --- | --- |
| `//src/vista:vista` | `product_shared_library` `dll_stem=vista`；`VISTA_EXPORTS` |
| `//src/vista/<mod>:*_sources` | 编进 DLL 的 source_set |
| `//src/vista:vista_test_all` | 模块测试聚合 |
| `//src/legacy/gis/vista:vista_sources` | leftover 对象进本 DLL |

落地时补：

- `//src/gis:gis` `assert_no_deps = [ "//src/vista:vista" ]`（`tile_test` 除外）。
- CPU `frame_sources` / `world_sources` / `assets_sources` / `domain*`：`assert_no_deps` 指向 `//src/render:render`、`//src/legacy/render:*`、`//src/legacy/ui:*`、`//src/legacy/app:*`。
- `graph_sources` 保持「只 deps rhi」；不要为了测试把 map 头编进 `render.dll`。
- `src_all` 已拉 `//src/vista:vista`；不要把孤儿 `//src/gis/vista:*` / `//src/effect:*` 加回图。
- 逐步去掉产品 vista TU 上的 `//build:legacy`（与 leftover 适配 TU 分开）。

### 分阶收紧（每步可编译）

- [x] **P0 文档** living § + HTML + `src/vista/README.md` + `src-layout`。
- [x] **P1 删孤儿树** 磁盘已无 `src/gis/vista/**` / `src/effect/**`。产品 include 走 `"vista/…"`。leftover 仍用 `"legacy/gis/vista/…"`。无 shim。
- [x] **P2 模块内收紧**（scheme C，同目录改 include + `BUILD.gn` `sources`）

| 旧路径 | 新路径（已落地） |
| --- | --- |
| `world/pointcloud/ingest/io/{las,laz,pdal,text}_io.*` | `world/pointcloud/ingest/{las,laz,pdal,text}_io.*` |
| `world/pointcloud/buffer/point_cloud.*` | `world/pointcloud/point_cloud.*` |
| `frame/detail/collision/collision.*` | `frame/detail/collision.*` |
| `domain/atmosphere/field/*` | `domain/atmosphere/field_*`（同目录） |
| `domain/atmosphere/systems/*` | `domain/atmosphere/{cloud,ocean,environment,procedural,atmosphere_params}.*` |

`terrain/{dem,mesh,process}` 与 GPU `atmosphere/{ocean,…}` **不**再合并。

- [x] **P3 GN 墙** CPU `*_sources` + `//src/gis:gis` `assert_no_deps`。CPU `//build:legacy` **仍在**（P3 尾项，另步能编再摘）。
- [ ] **P4 测试不再复制源** `map_effect_test` / atmosphere `*_pass_test` / domain 单测仍 in-process `VISTA_EXPORTS`/`RENDER_EXPORTS`（LNK1168 债务）。
- [ ] **P5 机械清理（可选，可另 PR）** include guard `EFFECT_*` / `GIS_VISTA_*` → `VISTA_*`。产品 vista 公开命名空间已是 `vista` / `vista::atmosphere` / `vista::detail`（不再使用 `gis::World` / `gis::vista`）。

### 测试 / 调用方

| 面 | 影响 |
| --- | --- |
| `//src/vista:vista_test_all` | P2 改 sources 路径；P4 改 link |
| `tile_test` / `frame_graph_test` | 仅测试 deps；include 改 `vista/…` |
| `content` present Map2d/Scene3d | 头路径已是 `vista/`；P2 不碰 API |
| `app/views` / world3d 插件 | P2 改 `pointcloud/ingest/…` 与 `domain/atmosphere/…` |
| leftover scene3d | 继续 deps `//src/vista:vista` |
| CBM | 本 § 落地后由 `/auto-cbm-gen` 刷新；本次不 `index_repository` |

### 不做什么

- 不新开 `docs/superpowers/specs/YYYY-MM-DD-*-design.md` 描述这次目录。
- 不引 Qt；不把 leftover GDI/GL/`scene3d` 拉进 `vista` 产品 TU。
- 不把 `GpuScene` 搬回 `src/render/scene`；不把 MapFrame 搬回 `gis.dll`。
- 不打破 `src/<layer>/<module>` 模块上限（不发明 `src/vista/gpu/map`）。
- 不在 `gis/` 或 `effect/` 留转发头「兼容一季」。
- 不把 CPU `domain/atmosphere` 与 GPU `atmosphere/` 合成一个目录（CPU 场 ⊥ GPU pass 是硬墙）。
- 本 § 不改 equal-profile 数值预算、不改 FlyCube pipeline 集合。

### 风险

| 风险 | 缓解 |
| --- | --- |
| 磁盘双树导致改错文件 | P1 先删孤儿；只编 `//src/vista:vista` |
| P2 include 漏网 | 全仓 Grep 旧路径；world3d `pdal_io` 是已知热点 |
| P4 改链 DLL 触发 LNK1168 | 测前释放 `out/Debug/vista_d.dll`；失败则暂留 in-process 编译并在测试注释标明债务 |
| `assert_no_deps` 误伤 leftover 适配 | 墙加在 **CPU product** source_set，不要加在 `legacy/gis/vista:vista_sources` |
| 命名空间 | 产品 `src/vista` 公开 API 已统一为 `vista`（CPU domain 为 `vista::atmosphere`） |
| 嵌套过深复发 | 新文件必须落在上表目录；禁止再开 `io/` / `internal/` |

---

## §Vista logical/physical lanes（2026-10-04）

**Status:** accepted  
**Updated:** 2026-10-05 — 「逻辑 / 物理」词汇退役。目录以 **§Vista IR/Pass lanes** 为准。下表与 U0–U4 保留落地当时的用词。CPU 场与 GPU 大气现已分属 `vista/component/atmosphere` 与 `vista/pass/atmosphere`（两个 `source_set` 的编译墙保留；GPU 不得依赖 `session_sources`）。  
**Plan:** [`../plans/2026-10-02-src-render-vista-parallel-accelerate.md`](../plans/2026-10-02-src-render-vista-parallel-accelerate.md) Task 8  
**Diagram:** [`../diagrams/vista-subdirectory-layers.html`](../diagrams/vista-subdirectory-layers.html) · 上一代 [`../diagrams/legacy-render-architecture.html`](../diagrams/legacy-render-architecture.html)

逻辑对象在 CPU 上并行产出 IR。物理录制在设备线程上做 GPU 上传、计算和一条 CommandList。2D 与 3D 共用这组阶段。UI 在 `stage_frame` 之后返回，不 join。

| 角色 | 目标目录 | 磁盘现状 | 线程 |
| --- | --- | --- | --- |
| 逻辑 2D | `vista/component/map`（`Layout` → `MapFrame`） | `vista/component/map` | Emit 可并行；碰撞串行。CPU `source_set` 不依赖 `render` |
| 逻辑 3D | `vista/component/world` | `vista/component/world` | 细分可并行。不依赖 `render` |
| 逻辑场 | `vista/component/atmosphere/session` | `vista/component/atmosphere/session` | CPU。`session_sources`，`assert_no_deps` → `render`。GPU `atmosphere_sources` 不依赖它 |
| 物理 2D | `vista/frame`（`FramePass`） | `vista/frame` | GPU 设备线程 |
| 物理 3D | `vista/scene`（`GpuScene`） | `vista/scene` | cull 在 CPU；Upload / Record 在 GPU |
| 物理大气 | `vista/component/atmosphere` pass + FFT | `vista/component/atmosphere` | GPU。不依赖 `world` source_set |

阶段：`stage` → Emit×N → Resolve → Upload → Compute（海洋 FFT）→ Record（pre-opaque → 不透明 → post-opaque → 2D 叠加）→ `graph::present`。`VISTA_LAYOUT_PARALLEL=0` 把 2D Emit 收成 N=1。`GPUSCENE_PREP_PARALLEL` 默认关。

Scenic 对照（同一张上一代图）：`MapPainter` 与 `scene3d/Scene` 把逻辑和录制焊在一个类型里。rhi2d Raster×N 是 CPU 私有 HDC。rhi3d Prep×N 之后才是 GL Draw 或 D3D11 deferred。Scenic 的 `frame` 是 `cc/` 帧拍，Vista 的 `frame` 是物理 2D 录制。Scenic 设备在引擎内；Vista 的设备是 `src/render` FlyCube。

`domain/domain.h` 留下 `DomainSession`。其他域不进 `atmosphere/`。GPU 大气目标继续不依赖 `session_sources`（该集带 `land_mask`）。

### 两代取舍

Vista 把「算什么」和「画上去」拆开。Scenic 把它们焊在同一个类型里。当前代换来统一的 GPU 路径和可单测的 CPU 中间结果；上一代换来按脏区域光栅、少一次 IR 拷贝。

| | Vista（当前代） | Scenic（上一代） |
| --- | --- | --- |
| 加速落在哪 | CPU：Emit×N 写 `MapFrame` / 网格，工人不碰 Device。GPU：FlyCube 上 Upload、海洋 FFT、一条 CommandList | 2D：Raster×N，每人一块私有 HDC，汇合后 BitBlt。3D：Prep×N 只做视锥，然后 GL Draw 或 D3D11 deferred |
| 优点 | 2D 与 3D 共用同一套阶段和同一套设备。`MapFrame` 同时喂软件出口和 GPU 出口，制图决策只做一次。`gis` 不依赖 vista，CPU 目标编译期进不了 RHI。UI 在 `stage_frame` 后返回 | 脏矩形按瓦片或图层光栅，合成只搬脏块。LP→DP 紧挨 HDC，GDI 路径少一层网格 IR。GDI / GDI+ / Skia / GL / D3D 能挂在引擎里做同等对比。帧拍（代次、丢弃过期帧）已经跑过 |
| 缺点 | 冷帧先做完 CPU IR 再上传。热帧靠 DrawCache / 静帧复用，没有脏瓦片 BitBlt。GPU 录制仍是一条 CommandList。`prep` 并行默认关，视锥不准时开了更慢。磁盘上 2D 目录名仍和逻辑/物理相反，`GpuScene` 仍把同步、剔除、上传、录制放在一个类型里 | 逻辑和录制在 `MapPainter`、`Scene::Render` 里绑死，换 GPU 后端就要改遍历。2D 与 3D 是两套设备。工人持有 HDC，和 HWND 的生命周期绑在一起。D3D11 deferred 的线程局部上下文不能带到 FlyCube。上屏在引擎内部 |

分工：Vista 做默认的 2D/3D 产品路径（一次制图、一次 GPU 提交、大气计算着色器）。Scenic 留在同等对比和 GDI 脏区光栅上，用来核对画面，不作为 FlyCube 的第二条命令列表。

### 升级（U0–U4）

每一阶段单独可编译。scheme C，不留转发头。公开签名 `Layout::build` 与 `record` 保持；变的是路径、`Pass` 的类型名，以及 CPU/GPU 的 `source_set` 边界。

从 Scenic 只借语义：帧拍与过期代次丢弃（已在 content `stage_frame`）、剔除先于绘制（已在 `prep_cull`）、脏区只重做失效的一片。不借私有 HDC、D3D11 deferred 的线程局部上下文、`MapPainter` / `Scene::Render` 这种逻辑与录制焊在一起的类型、引擎内上屏、第二套设备。

| 阶段 | 做什么 | 补上的缺点 | 验收 |
| --- | --- | --- | --- |
| **U0** | 两代图、取舍、本表 | 文档与磁盘脱节 | 图与本 § 一致 |
| **U1** | 编译墙：`scene` 的细分 / `FrustumPlanes` / `prep_cull` 进 `scene_cpu_sources`（`assert_no_deps` → `render`）。`CameraMatrices` 提取留在 GPU 侧，fan-out 之前写成 POD。`atmosphere/ocean/cpu_waves` 进 `atmosphere_cpu_sources`。`map/place` 进不依赖 `render` 的 CPU 集 | `GpuScene` 把 CPU 和 GPU 编进同一个目标 | 工人翻译单元不含 `render/rhi`。`record` 行为不变。`prep` 仍默认关 |
| **U2** | 同一次改动、三步改名以免覆盖：`vista/component/map` → `vista/frame_pass`；`vista/frame` → `vista/component/map`；`vista/frame_pass` → `vista/frame`。`Pass` 改名为 `FramePass`。include、`BUILD.gn`、`content` / `app` 调用点一起改 | 2D 目录名和逻辑/物理相反 | `frame_sources` 依赖 `map_sources` + `render`。`map_sources` 不依赖 `render` |
| **U3** | `domain/atmosphere` → `atmosphere/session`。GN 目标改名为 `session_sources`。GPU `atmosphere_sources` 不依赖它。`domain/domain.h` 留下 | CPU 场和 GPU pass 分成两个顶层目录，读起来像两套大气 | 宿主仍投影 `float*` / `AtmosphereParams`。GPU 集不带上 `land_mask` |
| **U4** | 脏区 Emit：只重算 `cache_key` 失效的 `DrawItem` 切片（C1 已有键）。失效片在设备线程上由 `FramePass` 上传。合成仍是 GPU 录制 | 冷帧整帧 IR + 上传；热帧没有脏区粒度 | `VISTA_LAYOUT_PARALLEL=0` 时墙钟不回退。画面门（山体阴影、注记）不变。产品路径没有 HDC |

**U5** 不在本升级里打开。多 CommandList 仍是并行计划的 V5，默认保持一条。不把 D3D11 deferred 引进 FlyCube。

### Checklist

- [x] U0 两代图、优缺点、升级表
- [x] U1 CPU/GPU `source_set` 拆开（`map_sources` `assert_no_deps` → `render`；`place` 在 `map/`。`scene_cpu_sources`：tess / frustum / octree / prep_cull，`assert_no_deps` → `render`）
- [x] U2 `map` = 逻辑 2D，`frame` = 物理 2D，`Pass` → `FramePass`
- [x] U3 `atmosphere/session`，GPU 目标不依赖会话集
- [x] U4 脏区 Emit（`retained_slices` / `cache_key`）+ `FramePass::upload_keyed_slices`

下一波（空间脏区 / prep 波 / 批合并）：**§Vista map/frame scenic-peer**。

---

## §Vista scene unibn octree（2026-10-05）

**Status:** active  
**Updated:** 2026-10-05  
**Diagram:** [`../diagrams/vista-scene-octree.html`](../diagrams/vista-scene-octree.html)  
**Hung off:** §Vista logical/physical lanes · §src_render + vista parallel（prep honesty）· leftover SP4b vendor [`../plans/2026-09-28-scene3d-index-octree.md`](../plans/2026-09-28-scene3d-index-octree.md)  
**Code:** `src/vista/scene/index/aabb_octree.*` · `cull/prep_cull.*` · `GpuScene::{instance,mesh}_index_`  
**Vendor:** `//third_party:octree` → jbehley/octree `unibn::Octree`（MIT header-only；`Octree.hpp` 只进 `.cc`）

### Locked

| # | Choice |
| --- | --- |
| 1 | Product scene+mesh spatial index lives in **`vista/scene/index`**, not leftover `SmtSceneOctTree`. Same unibn package as SP4b. |
| 2 | Index **AABB centers** + stored extents. Radius query uses frustum **world AABB** half-diag + max mesh half-diag (conservative). Exact filter remains `aabb_intersects_frustum`. |
| 3 | `prep_cull_meshes` builds a frame-local `AabbOctree` from `GpuMesh` AABBs (including point-cloud chunk boxes). Not stored on `GpuScene` (DLL layout). |
| 4 | Render filter stays opt-in (`SCENE3D_FRUSTUM_CULL=1`). Invert failure → linear scan. Prep workers still never touch Device / CL. |
| 5 | Do **not** include `Octree.hpp` from public headers. Do **not** add a second octree vendor. GPU BVH remains Deferred. |

### Flow

```
World nodes ──sync_from──► GpuInstance[] ──rebuild_meshes──► GpuMesh[] + AABB
record_draws (cull on):
  extract planes + frustum_world_aabb
  prep_cull: frame-local AabbOctree (unibn centers) → candidates
  aabb_intersects_frustum → visible[]
  record_kind reads visible[] only
```

### Acceptance

- [x] `AabbOctree` adapter + `scene_gpu_test` unit query (near kept / far dropped)
- [x] `GpuScene` rebuilds both indexes; `prep_cull_meshes` prefers octree when sizes match
- [ ] Product DEM still default-cull-off until orbit AABB is honest

---

## §Vista scene subdirectory（2026-10-05）

**Status:** active  
**Updated:** 2026-10-05  
**Diagram:** [`../diagrams/vista-scene-octree.html`](../diagrams/vista-scene-octree.html)  
**Hung off:** §Vista subdirectory tighten · §Vista logical/physical lanes U1 · §Vista scene unibn octree  
**Code:** `src/vista/scene/**`

Locks the GpuScene god-type split. Public `GpuScene` stays the present seam (`sync_from` / `ensure_meshes` / `record_draws`). Facade / IR / upload / record TUs live at the **`vista/scene` root** (peer of `vista/component/map/layout.h`). Specialized subdirs: `cull/` `index/`. Header next to `.cc`. No forwarding shims. Do **not** add `scene/gpu/` — content already uses `gpu/` vs `software/` for present backends; CPU tess TUs must not sit under a GPU-named folder. GN still splits `scene_cpu_sources` vs `scene_sources`.

| Layer | Disk | `assert_no_deps` → `render` |
| --- | --- | --- |
| CPU IR | `{gpu_instance,sync,envelope,kind_tess,pipelines,tessellate,paint}` | yes (`scene_cpu_sources`) |
| CPU cull | `cull/{frustum_aabb,mesh_cull,prep_cull}`, `index/` | yes |
| GPU mesh | `{gpu_mesh,upload,tint,draw_pass}`, `cull/frustum_camera` | no (`scene_sources`) |
| Facade | `{scene,rebuild,scene_draw,opaque_effect}` | no |

`CameraMatrices` is converted to `FrustumPlanes` + `float view/proj[16]` in `frustum_camera` **before** prep fan-out. `prep_cull_meshes` takes `MeshCullItem` (AABB + index_count + has_buffers), never `Buffer*`.

### Acceptance

- [x] `scene_cpu_sources` does not dep `//src/render:render`
- [x] `GpuInstance` / `GpuMesh` are colocated types; `GpuScene::GpuMesh` is a `using`
- [x] Rebuild tessellates via `kind_tess`; record still one CommandList
- [ ] Product DEM still default-cull-off (same as octree §)

---

## §Vista map/frame scenic-peer（2026-10-05）

**Status:** accepted  
**Updated:** 2026-10-05 — M1–M4 as-built（`collect_visible` / emit / AABB tile 键 / coalesce / `layout_gen` abort）。  
**Diagram:** [`../diagrams/vista-map-frame-scenic-peer.html`](../diagrams/vista-map-frame-scenic-peer.html)  
**Hung off:** §Vista logical/physical lanes（U0–U4 as-built）· §RHI2D map-layer-feature paint strategy · §RHI2D cc frame-beat  
**Plan:** [`../plans/2026-10-02-src-render-vista-parallel-accelerate.md`](../plans/2026-10-02-src-render-vista-parallel-accelerate.md) Task 2 leftover tile grain + Task 8 follow-on  
**Code:** `src/vista/component/map`（CPU `Layout` / `MapIR`）· `src/vista/pass/map`（`MapPass`）· host `src/content/browser/present/map2d`

对照 Scenic `paint/map` 升级产品 2D 的 **map（逻辑）/ frame（物理）** 缝。U2 目录对调已落地。本 § 不再搬路径，只锁下一刀语义。

### As-built（不要再做 U2）

| 树 | 拥有 | 硬墙 |
| --- | --- | --- |
| `vista/component/map` | `Layout::build`、`emit_*`、`LabelGrid`、`place`、`SliceCache`、hillshade bake 编排 | `assert_no_deps` `//src/render:render` |
| `vista/pass/map` | `MapPass::record`、atlas、upload、encode、`DrawCache`、`upload_keyed_slices` | 设备线程；不进 `parallel_for` |
| `content` Map2d | `stage_frame`、`layout_gen`、`layer_slices_`、`retained_slices` 注入 | UI 不 join Emit / GPU |

公开签名保持：`Layout::build`、`FramePass::record`。禁止第三公共命名空间 `vista::frame`。

### 借 Scenic / 不借

| 借（语义） | 不借（实现） |
| --- | --- |
| Scheduler × Runner **组合不合并** → `stage_frame` × `parallel_for` 在 Layout 内 | 把 tess 池并进 `FramePass` 或合成 `MapPainter` |
| collect → prep → encode → submit | GDI `PreparedFeature` / `POINT` / `CommandBuffer` 拷进 `MapFrame` |
| 脏矩形只重做失效片 | 私有 HDC Raster×N、`TransparentBlt` |
| `FrameAbortFn` / publish-on-current-gen | `cc::Layer` GIS 列表；引擎内 HWND present |
| 同笔 coalesce（画家顺序） | D3D11 deferred TLS；第二条 FlyCube 设备 |

### 缺口（相对 Scenic；M1–M4 后仍开）

1. **AABB 键是整幅 envelope，不是 per-feature clip。** `kLayoutAabbTilePx=256` 混入 `style_slice_key`；平移跨 cell 整层 miss。Scenic `RasterTile` 仍更细。
2. **无 pack_geoms 波。** 无现成廉价 clip/simplify peer；emit 仍直接写 view-CRS `Vertex`。
3. **content 需把 `layout_gen` / `live_layout_gen` 注入并拒绝 publish 过期帧。** map 侧已在 emit 批边界停 tess。

### 分阶（M0–M4）

每一阶段单独可编译。scheme C。`VISTA_LAYOUT_PARALLEL=0` 时墙钟不回退。

| 阶段 | 做什么 | 对标 Scenic | 验收 |
| --- | --- | --- | --- |
| **M0** | 文档对齐磁盘（本 § + 图）。U1–U4 勾 as-built | — | README / lanes 表与 `map_sources` / `frame_sources` 一致 |
| **M1** | `Layout::build` 内拆 `collect_visible` → 可选 `pack_geoms` → `emit_*`。IR 仍是 view-CRS `Vertex` | `map_layer_draw` + `map_feature_prep`（无 GDI POINT） | `frame_test` / `map2d_pass_test`；画家 z 不变 |
| **M2** | `cache_key` 混入 viewport AABB tile id（device-pixel 网格）。miss 只 tess 该 tile×style。`FramePass` 仍按 key 上传 | `RasterTile` damage；合成仍 GPU | 平移：命中片 skip tess；视觉门（hillshade / labels） |
| **M3** | Resolve 后同 kind/rgba/blend/pixel_space 的相邻 `DrawItem` 并网格（保持 style 层序） | `map_draw_batch` same_stroke | GPU draw 次数下降；`kIncremental` hash 仍稳 |
| **M4** | emit 波读 `layout_gen`；过期则丢部分结果不 publish | `FrameAbortFn` | 快平移不发布旧 gen；无 HWND join |

**V5 / 多 CommandList** 不在本 §。默认 1 CL。

### 依赖方向

```
gis ↛ vista
map_sources ↛ render
frame_sources → map_sources + render
content → vista
scenic.dll 不编进 vista.dll
```

### Checklist

- [x] M0 图 + 本 § + U1–U4 as-built 纠偏
- [x] M1 collect / pack / emit 拆层（签名不变）
- [x] M2 AABB tile × slice key
- [x] M3 CPU DrawItem coalesce
- [x] M4 emit 中 `layout_gen` abort

### 不做什么

- 不把 `Rhi2dPainter` / leftover HDC 引进 `vista/pass/map`。
- 不把 `MapFrame` 搬回 `gis.dll`。
- 不新开 dated spec。
- 不改 Scenic 为 FlyCube 第二条 CL。

---

## §Vista map deep split（2026-10-05）

**Status:** active  
**Updated:** 2026-10-05  
**Diagram:** [`../diagrams/vista-map-frame-scenic-peer.html`](../diagrams/vista-map-frame-scenic-peer.html)（CPU 列补公开头 / emit 辅件；物理 `frame` 不变）  
**Hung off:** §Vista map/frame scenic-peer · §Vista subdirectory tighten  
**Code:** `src/vista/component/map/` · [`../../../src/vista/component/map/README.md`](../../../src/vista/component/map/README.md)

M1–M4 语义已落地。本 § 只拆 **责任层**，不改 `Layout::build` / `MapFrame` 字段 / 画家 z。禁止第三公共命名空间 `vista::map`。无转发头：`frame.h` 是模块根 **umbrella**（允许），类型真源在并列头。

### 目标分层

| 层 | 磁盘 | 拥有 | 不拥有 |
| --- | --- | --- | --- |
| 公开 IR | `view.h` `draw.h` | `View` / `MapFrame` / `DrawItem` | OGR 批、tess 并行开关 |
| 公开编排 | `layout.h` `batch.h` | `Layout` / `LayoutInput` / `LayerBatch` / `build_layer_batches` | GPU encode |
| 覆盖 | `multiply.h` | hillshade luma coverage | emit tess |
| 风格策略 | `detail/carto_filter.*` | scale / role / stem | `OGRGeometry` 构造 |
| POD→OGR | `detail/batch_build.cc` | `LayerBatchSet` 生命周期 | Raster 上传 |
| 阶段 | `layout/{collect,emit,coalesce}.*` | 可见层、派发、同笔合并 | 单几何 tess 细节 |
| 键 / 并行 / 走几何 / 网格 | `layout/{slice_key,tess_grain,geom_walk,mesh_emit}.*` | 各管一块 | 混进同一个 `geom_mesh` |
| 按类 emit | `layout/{fill,line,point,symbol,raster}.*` | 仍按几何类 | 再套 `internal/` |

### 依赖方向（本模块内）

```
layout.cc → collect → emit → {fill,line,…} → mesh_emit / geom_walk / slice_key
batch_build → carto_filter（策略）+ OGR
place / FramePass → MapFrame IR only
map_sources ↛ render
```

### 不做什么

- 不把 `geom_mesh.h` 留成 shim。
- 不把 multiply 留在 `detail/layout`（那是 emit 目录）。
- 不改 equal-profile 数字、不借 HDC / `MapPainter`。
- 不新开 dated spec。

### Acceptance

- [x] 公开类型拆头；`#include "vista/component/map/ir.h"` 仍编译
- [x] `geom_mesh` / `build_stages` 按职责拆走，无转发（2026-10-05 回归后 N0 再次删除 `geom_mesh`）
- [x] `carto_filter` ⊥ `batch_build`；`multiply` 升到 `map/`
- [x] 内部 TU 落 `map/detail/`（scheme C，无转发头）
- [x] `frame_test` / `map_effect_test` 绿（本变更编译墙）

---

## §Vista map/frame scenic damage（2026-10-05）

**Status:** active  
**Updated:** 2026-10-05 — N0–N3 落地（脏瓦片 splice / `pack_geoms` / tess job abort）。  
**Diagram:** [`../diagrams/vista-map-frame-scenic-peer.html`](../diagrams/vista-map-frame-scenic-peer.html)  
**Hung off:** §Vista map/frame scenic-peer（M1–M4 as-built）· §Vista map deep split · §RHI2D map-layer-feature paint strategy · §RHI2D cc frame-beat  
**Code:** `src/vista/component/map/layout/{geom_walk,mesh_emit,tess_grain,slice_key,pack,emit,coalesce}.*` · `src/vista/pass/map` `MapPass` · host `content/.../map2d`

M1–M4 的 collect / 层键 / CPU coalesce / `layout_gen` 层边界 abort 还在。N0 把走几何、网格和并行粒度拆回三份头。N1–N3 只借 Scenic 的脏瓦片、prep 剔除和帧内 abort，不借 HDC、`POINT`、`MapPainter`。

### 对照（N1–N3 as-built）

| Scenic | Vista 现在 | 缺口 |
| --- | --- | --- |
| `enumerate_viewport_tiles`：只重做与 damage 相交的瓦片，带 outset | `enumerate_layout_tiles`：世界格网 256px + 16px world skirt；`cache_key` = layer × (tx,ty,zoom) | 宿主 E-lane `build_map2d_layout` 仍可不传 `retained_slices`（同步 `rebuild_layout` 已传） |
| `prepare_one_feature`：包络拒绝、亚像素剔除、概览跳过内环，再投影 | `pack_geoms` 在 emit 前：视口+outset 外拒绝、`scale<12` 亚像素 3px、概览去内环。IR 仍是 view-CRS `Vertex` | 无 GDI `POINT`（有意） |
| `FrameAbortFn` 可在 prep / tile 内停 | `layout_gen_stale` 在 style 层、tile、以及 fill/line `parallel_for` job 边界读 `live_layout_gen` | 保持。UI 不 join |
| `map_draw_batch` 同笔合并后再 play | CPU `coalesce_draw_items` 只并相邻同笔（含同 `cache_key`） | 保持。不把非相邻同笔打乱画家序 |

公开签名不变：`Layout::build`、`FramePass::record`。`map_sources` 继续 `assert_no_deps` `//src/render:render`。禁止第三公共命名空间。

### 分阶（N0–N3）

每一阶段单独可编译。`VISTA_LAYOUT_PARALLEL=0` 时墙钟不回退（`vista_layout_parallel_enabled()` 走串行 tess）。

| 阶段 | 做什么 | 对标 Scenic | 验收 |
| --- | --- | --- | --- |
| **N0** | 删除 `geom_mesh`。`geom_walk` 只管 OGR 走几何；`mesh_emit` 只管 quad / tess / 圈扇 / 线选项；`tess_grain.h` 只放阈值。无转发头 | 不把 prep 和 play 焊进一个类型 | `map_sources` 列出这三份；`fill` / `line` / `point` / `symbol` 不再 include `geom_mesh` |
| **N1** | `cache_key` 变成 layer × **视口瓦片 id**。要素包络命中哪些瓦片就只重算那些瓦片；未跨过的列/行 splice `retained_slices`。合成仍是 `FramePass` keyed upload。skirt ≈ 16px world | `RasterTile` damage + outset；不搬私有 HDC | 平移一列：未动瓦片 skip tess；hillshade / 注记视觉门不变 |
| **N2** | emit 前 `pack_geoms`：视口+outset 外拒绝、亚像素剔除、概览缩放跳过内环。IR 仍是 view-CRS `Vertex` | `map_feature_prep`（无 GDI `POINT`） | 同等 china 画面；冷帧 tess 次数下降；`frame_test` 画家 z 不变 |
| **N3** | tess job 批边界读 `live_layout_gen`，过期则丢掉部分 `MapFrame`，宿主不 publish | `FrameAbortFn` 在 prep 波内 | 快平移不发布旧 gen；UI 不 join |

**不做：** 私有 HDC、`TransparentBlt`、`MapPainter`、D3D11 deferred、第二条 CommandList、第二套线程池、把 `MapFrame` 搬回 `gis.dll`。

### Checklist

- [x] N0 `geom_walk` / `mesh_emit` / `tess_grain`；无 `geom_mesh`
- [x] N1 视口瓦片 × slice，平移复用未脏瓦片
- [x] N2 `pack_geoms`（包络 / 亚像素 / 内环）
- [x] N3 tess job 内 `layout_gen` abort

---

## §Vista IR/Pass lanes（2026-10-05）

**Status:** active  
**Updated:** 2026-10-05  
**Hung off:** §Vista logical/physical lanes。该节与 §Vista subdirectory tighten 里的「逻辑 / 物理」词汇退役；`frame/`、`scene/` 不再是目标目录。目录以本节为准。  
**Diagram:** [`../diagrams/vista-subdirectory-layers.html`](../diagrams/vista-subdirectory-layers.html)  
**Code (target):** `src/vista/{component/{map,world,atmosphere},pass/{map,world,atmosphere},mesh,terrain,assets,domain}`。

停用 logical / physical。磁盘模块用同一对词：**IR**（CPU，`assert_no_deps` → `//src/render:render`）和 **Pass**（设备线程 Upload / Record，跑在 GPU 上）。公开命名空间仍是两层 `vista` / `vista::atmosphere`。不新增 `vista::component` 或 `vista::pass`。scheme C，不留转发头。Include guard 与路径一致（`VISTA_COMPONENT_…` / `VISTA_PASS_…`）。`frame/` 与 `scene/` 在目标布局中删除。

### 目录

| 目录 | 类型 | GN | 线程 |
| --- | --- | --- | --- |
| `vista/component/map` | `Layout` → `MapIR`（原 `MapFrame`）。伞头 `"vista/component/map/ir.h"`（原 `vista/component/map/frame.h`） | `map_sources` | CPU IR |
| `vista/pass/map` | `MapPass`（原 `vista/frame` 的 `FramePass`） | `map_pass_sources`（原 `frame_sources`）→ `map_sources` + `render` | GPU 设备线程 |
| `vista/component/world` | `World`；`Instance`（原 `GpuInstance`）；CPU sync / tess / cull / index | `world_sources` 依赖 `mesh`，`assert_no_deps` → `render` | CPU IR |
| `vista/pass/world` | `WorldPass`（原 `GpuScene`）。`sync_from` 与 `record_draws` 仍是本类型的方法 | `world_pass_sources`（原 `scene_sources`）→ `world_sources` + `render` | GPU 设备线程 |
| `vista/component/atmosphere` | CPU：根 `Environment` / `AtmosphereParams`；`field/` · `ocean/` · `cloud/`；`detail/math.h` | `session_sources` / `atmosphere_cpu_sources`，`assert_no_deps` → `render` | CPU IR |
| `vista/pass/atmosphere` | GPU：根 `AtmosphereFrame` / effects；`ocean`/`cloud`/`sky`/`fog`/`globe`；`detail/{mesh,raster,field_texture}` | `atmosphere_pass_sources` → `atmosphere_cpu_sources` + `render`；不得依赖 `session_sources` | GPU 设备线程 |
| `mesh` / `terrain` / `assets` / `domain` | 不改名，也不标成 IR 或 GPU | 不变 | 不变 |

不建 `map/gpu`、`world/gpu` 或 `atmosphere/gpu`。`content` 的 `present/map2d`、`present/scene3d` 目录不动。

### 文件归属

进 `world/` 的 CPU 翻译单元（原 `scene_cpu_sources`）：`gpu_instance`（类型 `Instance`）、`sync`、`tessellate`、`kind_tess`、`paint`、`pipelines`、`envelope`、`cull/`（不含 `frustum_camera`）、`index/`。

进 `world_pass/` 的 GPU 翻译单元：根 `WorldPass` / `gpu_mesh` / `opaque_effect`；`detail/{upload,tint,rebuild,draw,record}`；`cull/frustum_camera`。`GpuMesh` 留在 `world_pass`（它持有 Device buffer）。细则见 **§Vista world_pass deep split**。

进 `map_pass/` 的是原 `frame_sources`：根 `pass` / `map_effect` / glyph；`detail/{atlas,encode,upload}`。`frame/detail/layout` 是已经活在 `vista/component/map/layout/` 的孤儿，删除，不带进 `map_pass`。

进 `atmosphere/` 的 CPU：根 `Environment` / `AtmosphereParams`；`field/`（store / ingest / procedural）；`ocean/`（`OceanSystem` + `cpu_waves`）；`cloud/`（`CloudSystem`）；`detail/math.h`（无 Device）。进 `atmosphere_pass/` 的 GPU：根 `AtmosphereFrame` / effects；`cloud` / `fog` / `sky` / `globe` / `ocean`；`detail/{mesh,raster,field_texture}`。宿主把场投影成 pass 参数；GPU 目标不 `#include` 场头。

### 调用顺序

公开签名的调用顺序保持。`content` 的 scene3d GPU present 持有一个长期 `WorldPass`，先 `sync_from` 再 `record_draws`。不把 `sync_from` 改成返回一份 IR 值再交给另一个对象。

环境变量字面量 `GPUSCENE_PREP_PARALLEL`、`VISTA_LAYOUT_PARALLEL` 保持。它们驱动的是 `WorldPass` prep 与 `Layout` emit。

测试可执行文件 `output_name`（`map2d_pass_test`、`scene_gpu_test`、`unified_draw_test`）保持。GN 目标名随目录改。

Scenic `scene3d/` 对照的是 `vista/component/world`（节点图）加 `vista/pass/world`（`WorldPass`），不要下沉进 `rhi3d/impl/common`。

### 不做什么

- 不把 `WorldPass::sync_from` 改成返回值，不改 content 里对象的持有方式。
- 不新增 `vista::atmosphere_pass` 命名空间；公开仍是 `vista` / `vista::atmosphere`。
- 不把 `mesh` / `terrain` / `assets` 标成 IR 或 GPU。
- 不改 `SMT_*` 环境变量字面量，不改测试 exe 的 `output_name`。
- 不新增第三层命名空间。
- 不新开 dated spec。

---

## §Vista world_pass deep split（2026-10-05）

**Status:** active  
**Updated:** 2026-10-05  
**Hung off:** §Vista IR/Pass lanes · §Vista map deep split（对等：根公开 + `detail/`）  
**Diagram:** [`../diagrams/vista-subdirectory-layers.html`](../diagrams/vista-subdirectory-layers.html)（§4 目录表）  
**Code:** `src/vista/pass/world/` · [`../../../src/vista/pass/world/README.md`](../../../src/vista/pass/world/README.md)

对标 `map_pass`：公开类型留在模块根，设备线程辅件按职责进 `detail/`。禁止第三公共命名空间 `vista::world_pass`。无转发头。`sync_from` / `record_draws` 仍是 `WorldPass` 的方法（不拆成返回 IR 的第二类型）。

### 目标分层

| 层 | 磁盘 | 拥有 | 不拥有 |
| --- | --- | --- | --- |
| 公开 facade | `pass.h` / `pass.cc` | `WorldPass`：sync、pipelines、view/tint/depth setters | per-kind encode 细节 |
| 公开 mesh | `gpu_mesh.h` | `GpuMesh` POD + `as_cull_item` | tess / upload 实现 |
| 公开 graph 适配 | `opaque_effect.*` | `kOpaque` `Effect` | `pass.h` 进 `render.dll` |
| 上传 | `detail/upload.*` | VB/IB/纹理、lit 开关 | 风格 RGBA 映射 |
| 着色 | `detail/tint.*` | paint / 点云 / DEM 默认 tint 写到 `GpuMesh` | RHI create |
| 重建 | `detail/rebuild.cc` | `rebuild_meshes`（CPU tess → upload） | 相机绑定 |
| 按类录制 | `detail/draw.*` | `record_kind` / `mesh_culled` | frustum 平面提取 |
| 编排录制 | `detail/record.cc` | `record_draws` / `record`（相机、prep_cull、按 kind 派发） | mesh rebuild 本体 |
| 视锥相机 | `cull/frustum_camera.*` | `CameraMatrices` → `FrustumPlanes` | AABB 相交（在 `world/cull`） |

### 依赖方向（本模块内）

```
pass.cc → detail/upload（pipelines / lit 探测）
detail/rebuild → world/{kind_tess,envelope} + detail/{tint,upload}
detail/record → detail/draw + cull/frustum_camera + world/cull/prep_cull
detail/tint → world/paint（rgba_from_resolved_paint）
opaque_effect → pass.h
world_pass_sources → world_sources + render
```

### Paint API 归属

`rgba_from_resolved_paint` 声明与实现在 **`vista/component/world/paint.h` / `paint.cc`**（CPU IR 侧）。不再挂在 `world_pass/pass.h`。GPU tint 只消费该函数。

### 不做什么

- 不把 `sync_from` 改成返回值，不改 content 持有方式。
- 不新增 `vista::world_pass` 命名空间。
- 不留 `draw_pass.h` / `scene_draw.cc` 转发头（`scene_*` 词已退役）。
- 不改 `GPUSCENE_PREP_PARALLEL` 字面量，不改 `scene_gpu_test` / `unified_draw_test` 的 `output_name`。
- 不新开 dated spec。

### Acceptance

- [x] 根公开三件套 + `detail/` 五件套 + `cull/`；GN `world_pass_sources` 列出新路径
- [x] `rgba_from_resolved_paint` 仅从 `world/paint.h` 导出
- [x] 外部仍 `#include "vista/pass/world/pass.h"`；无转发
- [ ] `scene_gpu_test` / `unified_draw_test` 绿（本变更编译墙）

---

## RHI2D map-layer-feature paint strategy

**Status:** accepted  
**Updated:** 2026-10-04  
**Diagram:** [`../diagrams/rhi2d-paint-map-strategy.html`](../diagrams/rhi2d-paint-map-strategy.html)

`paint/map/` is the **map / layer / feature drawing strategy**. It sits
**beside** `paint/carto/` and **above** carto primitives; it **consumes**
`cc/` (FrameJob gen, `LayerTreeImpl` damage, `TileGraphRunner`) and does
not merge into either tree.

### Why not re-layer into carto or cc

| Tree | Owns | Must not grow |
| --- | --- | --- |
| `paint/carto/` | Frame context, command encode, Draw* / style / xform | `gis::Map` walk, layer kind dispatch, parallel grain |
| `cc/` | Commit/activate, scheduler, raster tile rects, worker pool | OGR feature prep, carto style, GIS layer list |
| `paint/map/` | Visible-layer collect, CPU prep, ordered encode, execute/submit | Backend GDI vs Skia (stays `paint/backend/`) |

Moving strategy into carto would mix GIS document walk with primitive
draw. Moving into cc would give the compositor a second GIS layer list
(forbidden: GIS layers stay on `gis::Map`).

### Split (locked)

```
gis::Map
  → map_painter (Rhi2dPainter)   map dimension: seal + orchestrate
      → map_layer_draw           collect / prep wave / OGR·raster·tile
          → map_feature_prep     CPU LP→DP
          → map_draw_batch       encode via CartoDraw
          → map_feature_draw     unprepared fallback
      → map_submit               execute serial|tile|layer + publish front
paint/carto                      primitives only
cc                               schedule + tile graph
```

Host / LayerTreeHost keep the `MapPainter` type name as the map-frame
seam. Overlay Host painter still uses the same class with scheduler null.

### Non-goals

- No `cc::Layer` GIS list
- No carto include of `gis::Map`
- No leftover `src/legacy/` edits
- No public third namespace under `scenic`

---

## §RHI2D cc frame-beat upgrade（2026-10-04）

**Status:** accepted  
**Updated:** 2026-10-04 — P1 publish-on-current-gen + runner join-after-exit; P2 `run_cpu_prep_jobs` phased on TileGraphRunner（P3 still open）  
**Diagram:** [`../diagrams/rhi2d-paint-map-strategy.html`](../diagrams/rhi2d-paint-map-strategy.html)  
**Related:** §rhi2d Chromium-cc compose · §rhi2d leftover tile-raster · §RHI2D map-layer-feature paint strategy · §RHI2D public composition split · §Map2d present  
**Code (product only):** `src/scenic/render/rhi2d/impl/common/{host,cc,paint,surface}` · P3 `src/content/browser/present/map2d` · façade `src/scenic`  
**Freeze:** do **not** edit `src/legacy/`（`.cursor/rules/repo/legacy-freeze.mdc`）。Product TUs must not `#include "legacy/…"`。

`cc/` is Chromium-style **frame beat** (commit / activate / FrameJob generation), **not** a pixel compositor. Incremental strangler: leftover map2d semantics stay in Scenic `impl/common`, then align with product `content` present. Do **not** fold `MapScheduler` into `TileGraphRunner`.

### Goal / non-goal

| 目标 | 非目标 |
| --- | --- |
| 锁死 host / cc / paint / surface / content present 边界 | 把 Scheduler 与 Runner **合成一类** |
| 收口接缝：`CommandBuffer` IR + `RasterTile` rects + FrameJob gen + `paint_fn` | 让 `cc/` 认识 GIS 要素 / `gis::Map` 图层表 |
| 分阶段把 leftover 帧拍语义对齐 Scenic façade 与 `Map2dFrameCache` | 在 `src/legacy/` 上迭代；产品 `#include "legacy/…"` |
| 可选：prep fan-out 仍走 `TileGraphRunner::run_jobs`，与 Scheduler 分离 | 第二条 FrameJob；Blink `cc::Layer` 列表；把 `run_chunked_prep_pipeline` 并入 Scheduler |

### Target layers

| Axis | Dir | Owns | Must not own |
| --- | --- | --- | --- |
| HWND / ABI | `host/` | `MapRenderDevice`、`PresentController`（debounce + present-on-gen）、preview、published HBITMAP | FrameJob 执行、GIS walk |
| Frame beat | `cc/` | `LayerTreeHost`（sole map `Painter` + `back_buf_`）、`LayerTreeImpl` pending/active gen+damage、`MapScheduler`、`TileGraphRunner`、`RasterTile` | OGR prep、carto style、HWND Timer |
| Paint | `paint/` | `map/` prep→encode→submit；`carto/{encode,draw,frame,style}`；`backend/` | FrameJob coalesce / HWND-leak shutdown |
| Buffer | `surface/` | DIB pool、composer blit、HWND present helper | GIS layer list |
| Product present | `content/browser/present/map2d` | `Map2dPresenter`、`Map2dFrameCache`、`PresentAction`、vista `MapFrame` 双出口 | leftover `PaintBackend` / `CommandBuffer` GDI IR |

GIS layers stay on `gis::Map` and are walked only by `paint/map`.

### Seam APIs

```
PresentController.stage/submit
  → LayerTreeHost.stage_frame | submit_frame | paint_map_sync
      → MapScheduler.submit  (job_gen bump; PaintFn)
          → paint_fn = LayerTreeHost::on_worker_tick
              → LayerTreeImpl.activate(gen, RenderContext)
              → MapPainter.render_map
                  → paint/map: feature prep (`run_cpu_prep_jobs` / Runner prep phase)
                  → CartoDraw / CommandEncoder → CommandBuffer
                  → map_submit: TileGraphRunner.run_jobs(Raster×N)
                  → surface composer blit dirty tile centers
              → MapScheduler.mark_published(gen) if still current
  → PresentController arm present → HWND blit
```

| Seam | Owner | Contract |
| --- | --- | --- |
| `MapScheduler::PaintFn` | `cc/` → `LayerTreeHost` | 一帧激活 + 调 `MapPainter`；HWND 永不 `join` FrameJob |
| `CommandBuffer` | `paint/carto/encode` | 与 GIS 无关的 IR；tile/layer `execute` / `execute_tile` |
| `RasterTile` | `cc/raster_tile` | 视口格网 rect + outset；`cc` 不持有 feature |
| `TileGraphRunner::run_jobs` | `cc/` 被 `map_submit` 调用 | FrameJob **内** barrier；pull-index；idle 后才 `join` |
| `run_cpu_prep_jobs` | `paint/map/map_submit` | CPU LP→DP on **same** process-wide Runner, **prep phase**; barrier before encode |
| `run_chunked_prep_pipeline` | `paint/map/prep_pipeline.h` | Fallback if Runner is in raster phase; **not** Scheduler |

### Scheduler × Runner（compose, do not fold）

Call graph（已落地，保持分离）：

```
MapScheduler::{submit,paint_loop}     // size-1 NThreadPoolExecutor；coalesce/gen/HWND-leak
  └─ paint_fn_
       └─ MapPainter::render_map
            └─ map_submit / execute_map_pass
                 └─ TileGraphRunner::run_jobs   // resident workers；join after idle
```

| | `MapScheduler` | `TileGraphRunner` |
| --- | --- | --- |
| Role | 串行 FrameJob / Impl | 帧内 Raster×N（tile 或 layer） |
| Lifetime | 可 leak size-1 executor（HWND 线程不安全 join） | `shutdown` 必须 `join` workers |
| Cancel | `job_gen` + `cancel_frame_` | `request_cancel` / pull 停工 |
| Caller | `LayerTreeHost` | `paint/map` 在 `paint_fn` barrier 内 |

### paint/map vs cc

Prep **does not leave** `paint/map`. `cc` sees only IR + tile rects + gen. Overlay Host painter may use `MapPainter` with scheduler null — still not a second compositor.

### Phases

**P0 — 文档 / 边界锁死（本 §）**

| 改哪些 | 验收 | 风险 |
| --- | --- | --- |
| 本活规格 `§` + [`rhi2d-paint-map-strategy.html`](../diagrams/rhi2d-paint-map-strategy.html)；`impl/common/README.md` 措辞 | 四轴 + compose-not-merge 与代码头注释一致；无 `src/legacy/` diff | 旧 § 仍写 leftover 路径 — 以本 § + Scenic 树为准 |

**P1 — 接口收口 / 去重复生命周期** — landed（Scenic `impl/common`）

| 改哪些 | 验收 | 风险 |
| --- | --- | --- |
| `src/scenic/render/rhi2d/impl/common`：Host overlay `MapPainter` ≠ full-map；LTH `share_from` front；`publish_map_front` 拒绝 `should_abort`；`shutdown_static_raster_runners` 仅在 FrameJob `has_exited` | `scenic_gdi_cc_test` / `scenic_tile_raster_test` | HWND-leak vs join 顺序错 → UAF |

**P2 — prep 走 Runner fan-out** — landed（`paint/map` only）

| 改哪些 | 验收 | 风险 |
| --- | --- | --- |
| `run_cpu_prep_jobs`：process-wide `TileGraphRunner` prep 相位；失败则 Pipeline fallback；**不**并入 `MapScheduler` | 同测试；prep 与 raster 分波 | 争用 pool — 相位 CAS 禁止 prep∥HDC execute |

**P3 — 对齐 content / façade**

| 改哪些 | 验收 | 风险 |
| --- | --- | --- |
| `src/content/browser/present/map2d`（`Map2dFrameCache` / `PresentAction`）消费 **vista `MapFrame`/`DrawItem`**；Scenic 公开面不泄漏 `CommandBuffer`/GDI tile blit 语义 | §Map2d present 双出口仍成立；产品不 include leftover；Views HWND 走 content present | 把 leftover IR 复制进 content = 第二条制图；只复用 **帧拍语义**（commit/gen/retire），不复用 GDI HDC |

### Do not

- Fold `MapScheduler` + `TileGraphRunner`
- GIS features / `gis::Map` walk inside `cc/`
- Edit `src/legacy/`
- Second FrameJob lane
- Chromium `cc::Layer` GIS list
- Merge `run_chunked_prep_pipeline` into `MapScheduler`
- Product → leftover GN / include

---

## Folded topics (2026-09-28 merge B)

Former hot specs are under `archive/specs/` (`superseded`). **Revise this file** (append `§`) for new requirements in this topic. Do not create a new `YYYY-MM-DD-*-design.md`.

| Former hot spec | Section / note |
| --- | --- |
| [`../archive/specs/2026-09-13-model-render-compute-design.md`](../archive/specs/2026-09-13-model-render-compute-design.md) | §Model / render / compute umbrella (folded) |
| [`../archive/specs/2026-09-14-render-math-refactor-design.md`](../archive/specs/2026-09-14-render-math-refactor-design.md) | §Render math (folded) |
| [`../archive/specs/2026-09-14-render-skia-canvas-design.md`](../archive/specs/2026-09-14-render-skia-canvas-design.md) | §Skia canvas chrome (folded) |
| [`../archive/specs/2026-09-19-atmosphere-ocean-cloud-design.md`](../archive/specs/2026-09-19-atmosphere-ocean-cloud-design.md) | §Atmosphere / ocean / cloud + weather domain (folded) |
| [`../archive/specs/2026-09-27-map2d-frame-design.md`](../archive/specs/2026-09-27-map2d-frame-design.md) | §Map2d present (layering) · frame-graph CPU frame row |

