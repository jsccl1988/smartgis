<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Unified RenderDevice RHI (FlyCube) + dual scene (GIS World / GPU Scene)

**Date:** 2026-09-13  
**Status:** accepted  
**Related:** 三层伞状深度设计（模型 / 渲染 / 计算 + leftover 映射）见 [`2026-09-13-model-render-compute-design.md`](2026-09-13-model-render-compute-design.md)。产品语言 C++23。FlyCube 源码用本机 `C:\Dev\src\open\topic\graphic-engine`，不要 GitHub fetch。  
**Scope:** one implementation plan. Expand `render::rhi` to a modern engine-shaped RHI with FlyCube as the GPU backend (DX12 and Vulkan on Windows). Move logical model I/O and scene/spatial management out of `render/{model3d,scene3d}` into `sdb`. Keep a GPU-resident scene in `render/scene`.

## Goal

One `RenderDevice` path draws **2D maps and 3D worlds** through the same command-list RHI. GPU work is FlyCube (DirectX 12 and Vulkan). Logical models and the GIS world live in `sdb`; `render` only owns GPU resources and a synced render scene. Leftover `SmtRenderDevice` / GDI / GL stay as adapters so MFC views keep compiling.

## Non-goals

- Do not vendor Cesium Native, OpenSceneGraph, Filament, Diligent, bgfx, or a second GDAL/GEOS.
- Do not leak FlyCube, Assimp, or tinygltf types in public headers under `src/`.
- Do not treat Skia as the map RHI (Skia remains chrome paint in `render/skia`).
- Do not revive D3D9 / D3DX (`src/render/d3d` was deleted).
- Do not rewrite leftover `Smt_*` ABI or merge DLLs. `SmtRenderDevice::Init(HWND)` remains the MFC present seam.
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
| Leftover devices | `SmtRenderDevice` + GDI/GL wrap the Facade present; they do not grow a second 3D engine |

## Architecture

```
content::MapView / gpu process
        |
        v
render::rhi::Device  (Facade: Device / CommandList / Resource)
        |
        +-- FlyCube  (DX12 | Vulkan)     default GPU
        +-- Null                         tests
        +-- GDI / GL leftover            SmtRenderDevice::Init
        |
        v
render::scene::GpuScene   <----sync generation----  sdb::scene::World
        |                                              |
        | GPU mesh / texture / instance                +-- SmtMap layers (vector/raster)
        | frustum / GPU occupancy                      +-- sdb::model::ModelAsset (Assimp)
        | record CommandList                           +-- sdb::model::Tileset (3D Tiles)
                                                       +-- AABB query / pick
```

Dependency direction is one-way: `sdb` does not include `render/rhi`. `render/scene` depends on `sdb/scene` + `render/rhi`. `content` and `gpu` talk to `render::rhi` and `content::MapView` only.

## Tree and namespaces

Nesting stays `src/<layer>/<module>`. Public C++ is two levels; helpers go in `detail` or an anonymous namespace.

| Tree | Namespace | GN | Role |
| --- | --- | --- | --- |
| `src/render/rhi/` | `render::rhi` | part of `//src/render:render` | Facade + backends |
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
- `BindRhiPresent(HWND)` continues to create the GDI leftover device. New code (`gpu`, Views map viewport) uses `preferred_gpu_backend()`.
- FlyCube headers appear only in `flycube_rhi.cc`. Mapping: `Device` → FlyCube `Device` + `Swapchain`; `CommandList` → FlyCube `CommandList`; `execute` → `CommandQueue::ExecuteCommandLists`; `present` → `Swapchain::Present`.

Public RHI now includes `Buffer` + `create_buffer` / `upload` / `bind_vertex_buffer` / `bind_index_buffer`, `Texture` + `create_texture` / `upload_texture` / `bind_texture` for raster/tile quads, and `bind_camera` / `CameraMatrices`. FlyCube types stay out of `rhi.h`. When `SMT_HAS_FLYCUBE` is on (Debug compiles `/MDd` FlyCube into `out/flycube`; Release links the MD prebuilt), initialized devices upload to FlyCube heaps, bind view/proj constants, sample uploaded textures in a DX12 pipeline, and can clear/present a swapchain. Null-path tests stay CPU stubs (`bind_texture` / `bind_camera` counters). Optional: `rhi_test` initializes DX12 on a hidden HWND and skips (does not fail) when the machine has no adapter. Leftover GDI `RenderMap` and leftover GL `DrawIndexedPrimitives` record through `render::scene::leftover_session()`, a process-wide `LeftoverRecorder` owned by `SmtRender` (`smt_leftover_session` export) so GDI / GDI-simple / GL / SmtRender share one Device + CommandList.

## Model (`sdb::model`)

CPU-only. No GPU types, no `render` includes.

**Assimp (standalone files).** `load_file(const char* path, ModelAsset& out)` uses Assimp when `smt_has_assimp` is true. Supported product formats: OBJ, FBX, DAE, glTF/GLB. On failure, return false and leave `out` empty. When Assimp is not linked, `load_file` returns false except for the built-in name `"cube"` which fills a unit cube (`load_unit_cube`).

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
  void attach_map(const Smt_GIS::SmtMap* map);

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
- `attach_map` replaces all `kVectorLayer` / `kRasterLayer` nodes with one node per `SmtMap` layer. Envelope comes from `SmtLayer::GetEnvelope` when present; otherwise a zero box. Terrain/pointcloud leftover engines are attached later as `kTerrain` / `kPointCloud` name handles; v1 does not port `SmtScene` octree code.
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
- Camera: `CommandList::bind_camera` takes `CameraMatrices` (column-major view + proj). `GpuScene` binds ortho for raster/vector (world AABB) and perspective for 3D models. `LeftoverRecorder` binds perspective before leftover VB/IB. Null records the bind; FlyCube uploads a constant buffer so present is not identity-only.

## Data flow

1. App/content opens `SmtMap` and/or `load_file` / `parse_tileset_json`.
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
- GN args (in `build/smartgis.gni`): `smt_has_flycube` / `smt_has_assimp` / `smt_has_tinygltf`, default false until the source dir exists. Stub TUs always compile.
- C++23 (`cc_std`). FlyCube / Assimp keep their own CMake dialect; do not force `cc_std` onto those CMake trees unless they already inherit it.
- `src/render/d3d` was deleted (D3D9 / D3DX). Do not resurrect.

## Docs to update in the same change

- `docs/build/src-layout.md` — RHI backends; `sdb/model`, `sdb/scene`; `render/scene` GPU cache; leftover `scene3d`/`model3d`.
- `src/README.md` — RHI v1 paragraph.
- Root `README.md` — one line that map/3D GPU is FlyCube RHI; refresh **最后更新**.
- `docs/README.md` — link this spec.

## Optional GPU Track A (MapLibre Native)

`src/gpu` can select a 2D basemap backend **below** `content/public`:

- Default **Track B**: existing demo / FlyCube `GpuScene` (2D+3D same frame). `kScene3d` stays here.
- Optional **Track A**: `SMT_MAP_BACKEND=a` paints style background + one XYZ raster via `TileProvider` into `PresentTarget` (shared texture / DIB). Chrome still only blits `Latest()`.
- GN `smt_enable_maplibre` (default **false**) compiles a header probe for a local `third_party/.src/maplibre-native` pin. This tree does **not** link `mln::Map` until that pin provides a Windows lib. Do not include mln/mbgl from `app/` or `content/public`.

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
- `SmtRenderDevice::Init` still compiles and still calls `BindRhiPresent`.
