<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Unified RenderDevice RHI (FlyCube) + dual scene (GIS World / GPU Scene)

**Date:** 2026-09-13  
**Status:** accepted  
**Updated:** 2026-10-02 — §src_render + vista parallel accelerate（代码级详设） · §Atmosphere Google-Earth globe stack · §rhi3d leftover parallel frame · §rhi2d leftover tile-raster · §src_render Map2d/Scene3d equal-profile · …  
**Related:** model/compute · atmosphere · map2d folded into this file (§Folded topics); legacy present SP2 in [`2026-09-19-legacy-deep-abstraction-umbrella-design.md`](2026-09-19-legacy-deep-abstraction-umbrella-design.md)；Views shell [`2026-09-27-views-desktop-shell-design.md`](2026-09-27-views-desktop-shell-design.md)；as-built [`../../../src/render/README.md`](../../../src/render/README.md)、[`../../../src/gpu/README.md`](../../../src/gpu/README.md)；RHI subdir landed [`../archive/plans/2026-09-27-rhi-subdirectory-split.md`](../archive/plans/2026-09-27-rhi-subdirectory-split.md)。  
**Plans:** RHI scene [`../plans/2026-09-13-render-rhi-scene.md`](../plans/2026-09-13-render-rhi-scene.md) · frame graph [`../plans/2026-09-27-render-frame-graph.md`](../plans/2026-09-27-render-frame-graph.md) · gpu accelerate [`../plans/2026-09-27-gpu-rhi-accelerate.md`](../plans/2026-09-27-gpu-rhi-accelerate.md) · P0 [`../plans/2026-09-20-rhi-3d-capability-p0.md`](../plans/2026-09-20-rhi-3d-capability-p0.md) · suite/bench [`../plans/2026-09-28-render-rhi-suite-bench.md`](../plans/2026-09-28-render-rhi-suite-bench.md) · **Map2d hillshade + line casing** [`../plans/2026-09-30-map2d-hillshade-line-casing.md`](../plans/2026-09-30-map2d-hillshade-line-casing.md) · **world3d pointcloud LAS** [`../plans/2026-09-30-world3d-pointcloud-las.md`](../plans/2026-09-30-world3d-pointcloud-las.md) · **D3D leftover capability** [`../plans/2026-09-29-d3d-leftover-capability.md`](../plans/2026-09-29-d3d-leftover-capability.md) · **GL leftover capability** [`../plans/2026-09-29-gl-leftover-capability.md`](../plans/2026-09-29-gl-leftover-capability.md) · GDI leftover worker [`../plans/2026-09-29-gdi-leftover-worker.md`](../plans/2026-09-29-gdi-leftover-worker.md) · GDI carto math [`../plans/2026-09-29-gdi-carto-base-math.md`](../plans/2026-09-29-gdi-carto-base-math.md) · GDI layout/compose [`../plans/2026-09-29-gdi-layout-device-compose.md`](../plans/2026-09-29-gdi-layout-device-compose.md) · GDI profile [`../plans/2026-09-29-gdi-leftover-profile.md`](../plans/2026-09-29-gdi-leftover-profile.md) · **GDI internal RHI** [`../plans/2026-09-29-gdi-internal-rhi-reshape.md`](../plans/2026-09-29-gdi-internal-rhi-reshape.md) · **Legacy Pipeline+Arena** [`../plans/2026-09-30-legacy-render-pipeline-arena.md`](../plans/2026-09-30-legacy-render-pipeline-arena.md) · **rhi2d Chromium-cc** [`../plans/2026-10-01-rhi2d-chromium-cc-compose.md`](../plans/2026-10-01-rhi2d-chromium-cc-compose.md) · **rhi2d leftover tile-raster** [`../plans/2026-10-01-rhi2d-leftover-tile-raster.md`](../plans/2026-10-01-rhi2d-leftover-tile-raster.md) · **rhi3d leftover parallel frame** [`../plans/2026-10-01-rhi3d-parallel-frame.md`](../plans/2026-10-01-rhi3d-parallel-frame.md) · **rhi3d Eigen frustum** [`../plans/2026-10-01-rhi3d-eigen-base-math.md`](../plans/2026-10-01-rhi3d-eigen-base-math.md) · **src_render + vista parallel accelerate** [`../plans/2026-10-02-src-render-vista-parallel-accelerate.md`](../plans/2026-10-02-src-render-vista-parallel-accelerate.md)。  
**Scope:** Living RHI + dual scene + in-process frame graph + GPU-process compose. FlyCube DX12/Vulkan. Logical world in `gis`/`sdb`; GPU cache in `render/scene`. Do **not** open new dated RHI/layout twins — revise sections below.

## Goal

One `RenderDevice` path draws **2D maps and 3D worlds** through the same command-list RHI. GPU work is FlyCube (DirectX 12 and Vulkan). Logical models and the GIS world live in `sdb`; `render` only owns GPU resources and a synced render scene. Leftover `SmtRenderDevice` / GDI / GL stay as adapters so MFC views keep compiling.

## Non-goals

- Do not vendor Cesium Native, OpenSceneGraph, Filament, Diligent, bgfx, or a second GDAL/GEOS.
- Do not leak FlyCube, Assimp, or tinygltf types in public headers under `src/`.
- Do not treat Skia as the **FlyCube** map RHI (`render::rhi`). Leftover `SmtSkiaRenderDevice` (`legacy_rhi2d_skia.dll`, CPU `SkCanvas` / bootstrap) is an `SmtRenderDevice` paint lane only; chrome Skia stays in `ui/gfx`.
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

Public RHI now includes `Buffer` + `create_buffer` / `upload` / `bind_vertex_buffer` / `bind_index_buffer`, `Texture` + `create_texture` / `upload_texture` / `bind_texture` for raster/tile quads, and `bind_camera` / `CameraMatrices`. FlyCube types stay out of `rhi.h`. When `SMT_HAS_FLYCUBE` is on (Debug compiles `/MDd` FlyCube into `out/flycube`; Release links the MD prebuilt), initialized devices upload to FlyCube heaps, bind view/proj constants, sample uploaded textures in a DX12 pipeline, and can clear/present a swapchain. Null-path tests stay CPU stubs (`bind_texture` / `bind_camera` counters). Optional: `rhi_test` initializes DX12 on a hidden HWND and skips (does not fail) when the machine has no adapter. Leftover GDI/GL/D3D present stays on BitBlt / SwapBuffers / D3D11 Present — the process-wide `leftover_session` / `LeftoverRecorder` bridge was removed.

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
- Camera: `CommandList::bind_camera` takes `CameraMatrices` (column-major view + proj). `GpuScene` binds ortho for raster/vector (world AABB) and perspective for 3D models. Null records the bind; FlyCube uploads a constant buffer so present is not identity-only.

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

## Optional GPU tile basemap (StyleDocument; not MapLibre Native)

`src/gpu` can select a 2D basemap backend **below** `content/public`
(as-built: [`../../../src/gpu/README.md`](../../../src/gpu/README.md)):

- Default **direct** (`ContentSource::kDirect`): demo grid / Scene3d DEM underlay
  via `raster/direct` → `CompositorFrame` → `SoftwareRenderer` → `OutputSurface`.
- Optional **tile** (`SMT_MAP_BACKEND=a|track_a|maplibre` or `view.backend.maplibre`):
  StyleDocument + XYZ mosaic via `raster/tile` into the same compositor path
  (shared DXGI / DIB). Chrome still only presents the shared surface. Wire name
  `maplibre` means tile, **not** MapLibre Native.
- MapLibre Native product pin / `smt_enable_maplibre` / `maplibre_link` were
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
  `SmartGisViews --map2d-showcase=align` opens china_city + loads
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
- `SmtRenderDevice::Init` still compiles and still calls `BindRhiPresent`.

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
| Map / atmosphere GPU passes | `src/effect/map`, `src/effect/atmosphere` (`effect::*`); **not** compiled into `render.dll` deps of those passes |
| CPU frame | `gis::vista` (`MapFrame`); see map2d-frame living |

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

## §GPU-process accelerate（merged 2026-09-28）

Compose/present for `--type=gpu` (`src/gpu`) only. Shell / browser / Views never final-compose. Multi-adapter first-class: `GpuDeviceHub` + `AdapterId`. IR stays `CompositorFrame` / `DrawQuad`. Layout of `src/gpu` is **landed** as-built in `src/gpu/README.md`. In-process `render::graph::present` (Views) is a **different** path from this section. Plan checklist: [`../plans/2026-09-27-gpu-rhi-accelerate.md`](../plans/2026-09-27-gpu-rhi-accelerate.md). Archive: [`../archive/specs/2026-09-27-gpu-rhi-accelerate-design.md`](../archive/specs/2026-09-27-gpu-rhi-accelerate-design.md).

### A+C locked decisions (2026-09-28)

| Topic | Choice |
| --- | --- |
| Default `ComposeBackend` | **`kRhi`**. Unset / empty / unknown `SMT_GPU_COMPOSE` → `kRhi`. Explicit `SMT_GPU_COMPOSE=software` is the escape hatch (case-insensitive). Test override APIs remain. |
| Sticky fallback | On hard RHI / present failure for adapter A: sticky **software** for **that adapter only** (`GpuDeviceHub` sticky bit); other adapters keep RHI when healthy. |
| Topology | **One** `--type=gpu` process × **N** adapter device slots. Not N gpu processes. **No** single-frame multi-GPU split / cross-adapter mosaic in one compose. |
| Shell role | Shell consumes NT shared handles / DIB only — **never** blends the final frame. |
| Monitor affinity | `AttachSurfaceBody` / `ResizeSurfaceBody` carry `monitor_luid_low` / `monitor_luid_high` (DXGI adapter LUID of the output’s monitor). Optional `adapter_hint` (`0xffffffff` = unset). `gpu_main` binds / rebinds the `OutputSurface` via `GpuDeviceHub` (`prefer_adapter_for_monitor` / `rebind_surface_to_monitor` by LUID). **`HMONITOR` is not sent over IPC** (shell-local only when resolving LUID). |

### Legacy GDI buffer → compositor IR（2026-09-29）

**Updated:** 2026-10-01 — surface/ thinned to dib/ + composer/ (no blend/frame twin dirs; no Rhi2dComposer class).

src/legacy/render/rhi2d/impl/common/surface/ is map2d **compose + present**, not a generic byte arena.

| Locked | Choice |
| --- | --- |
| Public ABI | Rhi2dSurface + Rhi2dOwnedSurface + pool helpers; no SmtRenderBuf; POINT scratch uses ase::tls_allocate / 	ls_deallocate (no SmtBufPool) |
| Layout | surface/dib/ (DIB + pool + owned lifecycle) · surface/composer/ (soft blit · HWND present · buf→buf · IR submit) |
| Phase 1 | In-process: DIBSection BGRA + gpu::detail::blend_render_pass via narrow //src/gpu:compositor_cpu_blend (no full gpu_backend) |
| HWND present | detail::present_to_hwnd → GDI BitBlt / TransparentBlt / StretchBlt |
| buf→buf compose | detail::blit_owned_to / lit_surfaces → color-key→alpha then src-over; **stretch via nearest-neighbor** then blend |
| Blit mode | Rhi2dBlitMode::{kOpaque,kColorKey} (replaces eSwapType / GdiBlitMode) |
| Surface pool | 
hi2d_surface_pool() reuses DIB by size; draw TUs allocate POINT scratch from TLS arena |
| Phase 2 | make_compositor_frame + submit_surface; C ABI SmtRhi2dSetBgraSubmit; GPU process binds → OutputSurface. Shell still must not final-compose. |
| Phase 3 | GPU leftover_gdi_bgra_upload builds CompositorFrame → make_frame_composer (RHI/software) → draw_frame; NN-scale when sizes differ. Test: gdi_compose_test. |

### Phases

| Phase | Scope |
| --- | --- |
| **P0** | Default compose = RHI; `SMT_GPU_COMPOSE=software` escape; sticky per-adapter software fallback; docs/tests match. |
| **P1** | LUID fields on Attach/Resize IPC; shell fills LUID; `gpu_main` rebinds via hub (not primary-only forever). |
| **P2** | TDR / device-lost → sticky software + generation bump wired from recovery path; as-built docs / dual-adapter notes. |

---

## §P0 3D capability（merged 2026-09-28）

Lit solid path, style→3D albedo, CPU frustum, GPU smoke — checklist in [`../plans/2026-09-20-rhi-3d-capability-p0.md`](../plans/2026-09-20-rhi-3d-capability-p0.md). Pipeline entry is via §Generic pipeline (`Pipeline*` + constants), not `PipelineId::{kOcean,kCloud}`. Archive: [`../archive/specs/2026-09-20-rhi-3d-capability-p0-design.md`](../archive/specs/2026-09-20-rhi-3d-capability-p0-design.md).

---

## §Content `present/scene3d` slim（2026-09-28）

Align with `present/map2d`: thin facade + capability dirs; no pure-forward Presenter API.

| Path | Role |
| --- | --- |
| `scene3d_presenter.*` | bind / `present_gpu` / `paint*` / accessors only |
| `atmosphere/` | `AtmosphereSession` (Environment + pass prepare + M3 hooks) |
| `frame/` | `OrbitGeoFrame` + `rebuild_terrain_mesh` |
| `gpu/` / `software/` | FlyCube present / GDI paint |
| `policy/` | `prefer_scene3d_flycube` / `force_content_mapview_3d` |
| `stereo/` | `Scene3dStereoSession` (legacy_render LoadLibrary) |

Callers use `atmosphere_session()` / `gpu()` / `software()`. Deleted Presenter forwards: atmosphere toggles, wireframe, `paint_engine_logo` static. Geo frame filled once via `ensure_geo_frame()` before prepare; DEM mesh rebuild lives in `frame/terrain_mesh`. As-built: [`../../../src/content/browser/present/README.md`](../../../src/content/browser/present/README.md).

---

## §RHI suite / coverage / bench（2026-09-28）

**Status:** accepted  
**Plan:** [`../plans/2026-09-28-render-rhi-suite-bench.md`](../plans/2026-09-28-render-rhi-suite-bench.md)

Headless functional matrix + performance benches for `render::rhi` / `graph`, plus Debug Console harness entry. Matches industry RHI sample surfaces (Device / Resource / Pipeline / CommandList / Present / Compute / Frame graph) without vendoring a second RHI.

| Locked | Choice |
| --- | --- |
| Layout | Shared scenarios in `src/render/testing/` (`render::detail`); product DLL does **not** depend on testing |
| Functional | `rhi_suite_test` (Null) → `test_shell` / `test_all`; keep existing `rhi_test` GPU smoke (`SMT_RUN_FLYCUBE_GPU=1`) |
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

After paint dedupe, leftover still had dual scheduler vocabulary (`GdiUiController` vs worker lane) plus Hungarian `m_smt*` members and paint type names that did not match Chromium/cc. Worker lane is now `Rhi2dFrameWorker` + `detail::Rhi2dFrameScheduler`. `SmtRenderDevice` Draw* ABI stays.

### Locked choices

| Topic | Choice |
| --- | --- |
| Lexicon | Chromium/cc-style names inside `impl/gdi/core` |
| Directories | Keep `host/` · `paint/` · `worker/` · `surface/` |
| ABI | `SmtRenderDevice` virtuals + CreateDevice string **unchanged** |
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
| ABI | `SmtRhi2dRenderDevice` / CreateDevice / `SmtRenderDevice` virtuals **unchanged** |

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
| SIMD | Same as render-math: `smt_render_math_simd` default **off** (scalar) |
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
**Plan:** [`../plans/2026-10-01-rhi2d-eigen-base-math.md`](../plans/2026-10-01-rhi2d-eigen-base-math.md)  
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
| SIMD default | Remains **off** (`smt_render_math_simd`) |
| ABI | `SmtRenderDevice` LPToDP/DPToLP signatures unchanged |

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
**Plan:** [`../plans/2026-10-01-rhi2d-lpdp-hotpath-tune.md`](../plans/2026-10-01-rhi2d-lpdp-hotpath-tune.md)  
**Code:** `base/math/simd.*` · `rhi2d/.../style/xform.*` · `draw_ogr` linear_ring · prep play (`map_draw_batch`)  
**Extends:** §rhi2d Eigen / base/math deepen

### Why

Dense line/polygon paths already use `transform_xy_batch`. Remaining cost: `draw_linear_ring` still per-vertex `c_->lp_to_dp`; each `Rhi2dCartoDrawXform::lp_to_dp` rebuilds `LpToDp2`; batch SIMD is still a scalar loop. Do **not** force 2D through 4×4 `Matrix`.

### Locked choices

| Topic | Choice |
| --- | --- |
| P0 | `draw_linear_ring` → pack + `transform_xy_batch` (same as line_string) |
| P1 | Frame-level `LpToDp2` cache on `Rhi2dCartoDrawXform` (invalidate on context / port fingerprint change) |
| P2 | AVX2 fill of `transform_xy_batch` when `smt_render_math_simd`; default **off**; ≡ scalar `LONG+0.5+flip_y` |
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

`base::trace::process_trace` + Views `RenderTracePanel` already cover map2d/scene3d. **GDI leftover** (`impl/gdi`) had zero `BASE_TRACE_EVENT`. Legacy SmartGis.exe needs a thin MFC panel that streams **text log lines** (not Views HWND host, not Chrome Trace UI dependency) so dual-track paint can be decomposed before optimizing.

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

- `SMT_TRACE=1` or UI Record captures GDI spans; default off stays cheap.
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

Industry look pass on `src/effect/atmosphere` without a second atmosphere tree. Living umbrella only — no new dated design twin.

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

Three-layer product face on `src/effect/atmosphere` (no second engine):

| Layer | Pass | Role |
| --- | --- | --- |
| A Earth DEM | `globe/GlobePass` | UV sphere + height displace + equirect albedo |
| B Sat cloud | `globe/SatCloudPass` | Transparent shell slightly above earth |
| C Atmosphere | existing `SkyPass` (+ optional fog) | Far-field sky / scattering backdrop |

### Data entry

| Asset | Path (preferred) | Fallback |
| --- | --- | --- |
| Global DEM | `out/data/global_dem.tif` via `gis::find_sample_global_dem_path` / override | `china_dem.tif` is default `find_sample_dem_path` (China seed must not remask) |
| Global terrain albedo | `out/data/global_terrain.tif` (GeoTIFF; GDAL here is GTiff-only — use `build_globe_terrain.py --download-blue-marble`) | Hypsometric bake from DEM |
| Sat cloud | `out/data/sat_cloud.tif` / `global_cloud.tif` | Procedural cover stub |
| China overlay / imagery | `china_rs.tif` via `find_sample_imagery_path` (after global_*) | Hypsometric bake from DEM |

### Wiring

- `AtmosphereSession::{set_globe_enabled,set_sat_cloud_enabled}` + `prepare_globe` / `prepare_sat_clouds`.
- Globe present path skips flat DEM `rebuild_local_mesh` / `GpuScene::sync_from` / opaque DEM (avoids Debug STL AV).
- `prepare_for_present` on globe: globe + sat cloud + sky/fog only (no flat ocean/cloud prepare).
- `prepare_globe` uses `find_sample_global_dem_path` + `find_sample_global_imagery_path` (full-sphere when global_* present); default product DEM stays `china_dem` via `find_sample_dem_path` so leftover China remask does not punch plains. Generate samples: `py -3 testing/data/build_globe_terrain.py`.
- After growing `AtmosphereSession` (embed `GlobePass`/`SatCloudPass`), rebuild all TUs that include the header (`scene3d_presenter.obj` etc.) — stale layout caused post-DEM AV.
- `AtmosphereFrame` pre: sky → globe; post: sat cloud (globe path skips flat ocean / volumetric cloud deck).
- Showcase: `--atmosphere-showcase=globe` (`testing/tools/harness/atmosphere/atmosphere.globe/`).
- Unit: `globe_pass_test` (Null RHI). Null showcase verified PASS (`SMT_ATMOSPHERE_SHOWCASE_GPU=0`).

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
| ABI | **May break** `SmtRenderDevice` / callers (sync MFC/`xview`/tool) |
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
| ABI | `SmtRenderDevice` / CreateDevice / class names **unchanged** |
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
**Plan:** [`../plans/2026-09-29-gdi-flatten-player.md`](../plans/2026-09-29-gdi-flatten-player.md)  
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
- **No** drive-by rename of public `SmtGL*` types or factory strings.
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
**Plan:** [`../plans/2026-10-01-rhi2d-chromium-cc-compose.md`](../plans/2026-10-01-rhi2d-chromium-cc-compose.md)  
**Code:** `src/legacy/render/rhi2d/impl/common/cc/` + `host/` · `paint/` · `surface/` · port `impl/{gdi,gdiplus,skia}/`

### Why

Align leftover map2d host with Chromium **cc** (commit / activate / draw) so dual-track scheduling, retire, and compose IR read like the industry compositor — without merging into `gpu/` or copying Chromium sources.

### Locked choices

| Topic | Choice |
| --- | --- |
| Approach | **`impl/common/cc/`** owns frame beat (`LayerTreeHost` · `Scheduler` · `LayerTreeImpl` gen/damage) |
| Pipeline | PresentController → Host stage/submit → activate → (tile raster / Painter) → submit_surface / present_to_hwnd |
| Threading | **Landed baseline:** serial `NThreadPoolExecutor(1)` FrameJob. **Next:** §rhi2d leftover tile-raster replaces that executor with Impl + TileGraphRunner (HWND still never `join`s) |
| ABI | `SmtRenderDevice` / LoadLibrary / three DLL stems **frozen** |
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
**Updated:** 2026-10-02 — Diagram SVG（legacy/render 全宽分层 + rhi2d 流水线）  
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
| Mode switch | `SMT_RHI2D_PARALLEL=serial\|tile\|layer` (default **tile**). Legacy `SMT_RHI2D_TILE_RASTER=0` → serial when PARALLEL unset |
| Grain A (scheme 1) | **Viewport device-pixel grid** (default **256×256**, adaptive up to 1024 to keep ≤~4 tiles unless `SMT_RHI2D_TILE_SIZE` set) |
| Grain B | **Per GIS layer** encode → parallel `execute` → ocean **color-key** compose (TransparentBlt; preserves z-order cartography) |
| Ports | Shared encode IR; tile/layer execute uses **thread-private** HDC (all ports) |
| Thread model | **H2 slim:** FrameJob (`NThreadPoolExecutor(1)`) = Impl; **N Raster** via `Rhi2dTileGraphRunner` |
| Runner | Slim **TaskGraphRunner**: fixed workers **pull** job indices (no per-job pool PostTask) |
| Paint split (tile) | **Serial encode** → **parallel `execute_tile` + AABB cull** on **partial damage**; full-damage falls back to serial (`SMT_RHI2D_TILE_FORCE=1` to force). Prefer **layer** grain for full china frames |
| Paint split (layer) | **Serial encode per layer** → **parallel `execute`** → ocean clear + z-order ocean color-key |
| Damage | Tile mode: intersect damage with tiles; blit only dirty centers |
| Skirt | Tile **outset 16px**; compose writes center only |
| Labels | In command buffer for v1; dedicated serial label pass deferred |
| Timing | `SMT_RHI2D_PARALLEL_LOG=1` prints `execute_ms` to stderr |
| ABI | `SmtRenderDevice` / Create·Destroy / three stems **frozen** |

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
| `paint/map/map_painter.*` | Mode dispatch: serial / tile / layer |
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
- [x] `SMT_RHI2D_PARALLEL=serial\|tile\|layer` (+ legacy `TILE_RASTER=0`)
- [x] `legacy_rhi2d_{gdi,gdiplus,skia}` build green with tile/layer path
- [ ] Pan/zoom smoke: HWND never joins; stale gen does not publish
- [ ] Measurable wall-clock compare on mid-size PLP (`PARALLEL_LOG` + serial/tile/layer)

---

## §rhi2d modern C++ / base harden（2026-10-01）

**Status:** active  
**Plan:** [`../plans/2026-10-01-rhi2d-modern-cpp-base-harden.md`](../plans/2026-10-01-rhi2d-modern-cpp-base-harden.md)  
**Code:** `src/legacy/render/rhi2d/` (`detail/renderer.cpp`, `impl/common/`, `impl/{gdi,gdiplus,skia}/`, `public/` drive-by)

### Why

Mirror §rhi3d harden for leftover 2D: loader still uses `MessageBox`, `Release` does not `FreeLibrary`, tree still has C-era `NULL` / C-casts / raw `POINT[]` scratch. Paint hot path should use modern C++ + `base` allocators without changing `SmtRenderDevice` ABI or multi-DLL LoadLibrary shape.

### Decisions

| Topic | Choice |
| --- | --- |
| Depth | **B**: loader harden + full-tree modern C++ + paint hot-path base |
| Boundary | **Full tree**: `common` + `gdi`/`gdiplus`/`skia` ports (+ stale `impl/gdi` mirrors when present) |
| ABI | **`SmtRenderDevice` / Create*/Destroy* exports frozen**; `public/` only `NULL`→`nullptr`-class edits |
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
**Plan:** [`../plans/2026-10-01-rhi3d-camera-modern-cpp.md`](../plans/2026-10-01-rhi3d-camera-modern-cpp.md)  
**Code:** `src/legacy/render/rhi3d/public/camera/`

| Topic | Choice |
| --- | --- |
| Type names | Keep `Smt*` (`SmtPerspCamera`, …) |
| Methods | **`snake_case`** (breaking); `const&` inputs |
| Combined | Merge into `SmtPerspCamera`; **delete** `combined_camera.h` |
| Factory | `make_view3d_camera` → `std::unique_ptr<SmtPerspCamera>` |
| Layout | Decl `camera.h` + body `camera.cc`; shared orbit in `detail` |
| Math | Reuse `base/math` `Vector3` |
| Non-goals | No FlyCube camera; no `SmtScene` ownership rewrite |

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
**Updated:** 2026-10-01 — `paint/` → `backend/` · `carto/{encode,draw,frame,style}` · `map/` (Painter) · GDI worker→LayerTreeHost · mini expression `interpolate` / `match` (+ `step` / `case` / `coalesce`) · heatmap v1  
**Plan:** [`../plans/2026-09-30-map2d-hillshade-line-casing.md`](../plans/2026-09-30-map2d-hillshade-line-casing.md)

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
| Hillshade | Own DEM → slope/aspect → shade RGBA underlay into `MapFrame` / `effect/map`; Style `type: hillshade` paint constants only in v1 |
| Line casing | Dual Style layers (`road-casing` then `road`) — already in `default_carto_style_json`; fix zoom + contrast + align Style; **no** copy of MapLibre SDF line shaders |
| DEM source | Reuse `gis::DemRaster` / china_dem samples; no new terrain stack |
| Align gate | Extend `style_align.json` + `maplibre_align` / china loops; optional Native still is reference only |
| Implementation ban | No `#include` mln/mbgl from `app/` / `content/public` / `gpu/` / `effect/map`; no port of hillshade_prepare / line SDF buckets |
| Expression | Own mini eval in `gis/present/style/expression.*` — **not** a MapLibre expression VM |

### Deliverables

1. **Line casing (fast):** Lower overview `minzoom` so casing+fill readable on china frame; cream-safe casing/fill colors; `style_align.json` gains casing layer; layout draw order = Style layer order (casing before fill).
2. **Hillshade v1:** `paint_resolve` fills hillshade constants into `ResolvedPaint` (or small companion struct); layout emits one screen-aligned shaded raster `DrawItem` from DEM when a hillshade layer is present and DEM is bound; software + GPU textured path both show it under vectors.
3. **Product DEM bind:** `Map2dFrameCache` loads `china_dem` via `find_sample_dem_path` when Style has `hillshade` (soft-fail if missing); bakes `shade_dem_rgba` into `LayoutInput.hillshade_tiles` + `load_raster` for GDI/GPU.
4. **Gates:** `map2d_china_loop` / align stills show road casing contrast; soft hillshade variance metric when DEM underlay signal is present.
5. **Mini expression (landed):** expand paint/layout constant eval used by line/fill/circle/symbol — see §Mini expression below.

### §Mini expression — interpolate / match（2026-09-30）

**Status:** landed (mini subset expansion)  
**Code:** `src/gis/present/style/expression.{h,cc}` · gate `style_test`

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
- Atmosphere rewrite; leftover GDI carto rewrite beyond present compose.

### Success

- China overview: roads read as dual-stroke (casing darker, fill lighter), not cream scribble.
- With DEM: hillshade underlay visible behind land/water vectors when Style enables it.
- Fill-extrusion Style layers emit wall+roof DrawItems in unit tests.
- Heatmap Style layers emit circle splat DrawItems when zoom matches; `style_test` / `map2d_test` cover paint resolve + emit.
- Mini expression: `style_test` covers interpolate/match (and step/case/coalesce) for line/fill/circle/symbol paints.
- Along-line labels survive midpoint blockers via slot retry; short paths drop oversize text (`frame_test`).
- `build.bat debug` + map2d china / align gates green; no mln product dep.

---

## §Point cloud GPU draw（2026-09-30）

**Status:** active  
**Updated:** 2026-09-30  
**Plugin side:** [`2026-09-13-plugin-host-design.md`](2026-09-13-plugin-host-design.md) §world3d pointcloud LAS  
**Plan:** [`../plans/2026-09-30-world3d-pointcloud-las.md`](../plans/2026-09-30-world3d-pointcloud-las.md)

### Locked

| # | Choice |
| --- | --- |
| 1 | `gis::NodeKind::kPointCloud` carries CPU `point_positions` (xyz float) + optional `point_rgba`. |
| 2 | P0 draw: `tessellate_point_cloud` → tiny triangles uploaded like other lit kinds; `GpuScene::record_draws` must `record_kind(..., kPointCloud, ...)`. |
| 3 | P1+: chunk AABB cull; P2 octree/LOD — no native POINTLIST pipeline required for P0. |
| 4 | Do not route default Views path through leftover `Smt3DPointCloud`. |

---

## §rhi3d leftover parallel frame（2026-10-01）

**Status:** active  
**Updated:** 2026-10-02 — Diagram SVG（legacy/render rhi3d P1–P3 流水线）  
**Plan:** [`../plans/2026-10-01-rhi3d-parallel-frame.md`](../plans/2026-10-01-rhi3d-parallel-frame.md)  
**Diagram:** [`../diagrams/legacy-render-architecture.html`](../diagrams/legacy-render-architecture.html)（浅色 SVG：FrameJob → Prep×N → D3D deferred → publish）  
**Code:** `src/legacy/render/rhi3d/impl/common/frame/` + `scene3d/scene/stereo_hwnd_view.*` (+ P2 prep in `scene3d/scene/`)

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
| Fallback | `SMT_RHI3D_FRAME_JOB=0` → sync present; `SMT_RHI3D_PREP_PARALLEL=0` → prep N=1; `SMT_RHI3D_D3D_DEFERRED=0` → serial D3D draw |
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
| `scene3d/scene/stereo_hwnd_view.*` | Owns scheduler; present/capture contracts |
| `scene3d/scene/scene.*` · `index/octree.*` | P2 cull + P3 deferred Render |

### Non-goals

- Shared GL context multi-thread Draw; 3D viewport tile-raster RT
- FlyCube / `render::graph` “one CommandList” contract change
- Merge leftover into `src/gpu`
- Perfect transparency order across deferred slots (opaque-first leftover OK)

### Acceptance

- [x] `Rhi3dFrameScheduler` + `frame_scheduler_test` green
- [x] FrameJob on: present returns without blocking full Update/Render; HWND destroy no join hang
- [x] `SMT_RHI3D_FRAME_JOB=0` matches prior sync present behavior
- [x] capture/blit wait published gen (bounded); destroy cancels + shutdown
- [x] P2: N≥2 prep workers when enabled; CPU cull before draw
- [x] P3: D3D deferred begin/bind/finish (`d3d_texture_test`); `SMT_RHI3D_D3D_DEFERRED=0` serial fallback
- [x] `legacy_render` / `legacy_render_d3d` green
- [x] Module README documents FrameJob + prep + deferred env (gl/d3d)

---

## §rhi3d Eigen / base/math frustum（2026-10-01）

**Status:** active  
**Updated:** 2026-10-01  
**Plan:** [`../plans/2026-10-01-rhi3d-eigen-base-math.md`](../plans/2026-10-01-rhi3d-eigen-base-math.md)  
**Code:** `src/base/math/{frustum,matrix4}.*` · `rhi3d/public/device/{base,render_device}.h` · `rhi3d/impl/{gl,d3d}/paint/*` · `scene3d/{index/octree,scene/scene,primitive/surface/pointcloud}.*`

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

## §src_render Map2d equal-profile optimize（2026-10-01）

**Status:** active  
**Updated:** 2026-10-01 — baseline from leftover×port + Views china 1280×720 matrix  
**Plan:** [`../plans/2026-10-01-src-render-map2d-equal-profile-optimize.md`](../plans/2026-10-01-src-render-map2d-equal-profile-optimize.md)  
**Code:** `content/browser/present/map2d/**`, `effect/map/**`, `render/{rhi,graph}/**`; harness `testing/tools/harness/map2d/run_parallel_port_matrix.py`

### Why

Equal-profile matrix shows leftover IR `execute_ms` ≈ **50 ms** while Views `export_ms` ≈ **244 ms** and cold `present_gpu_ms` ≈ **1521 ms**. Richness (hillshade / MapFrame / FlyCube) is intentional — but cold path and export clearing wipe reuse. Need a **phased** cut so product 2D is competitive **without** deleting carto.

### Profile (locked)

| Axis | Value |
| --- | --- |
| Extent | China mainland `[80,16]–[128,52]` (same as leftover / showcase) |
| Viewport | **1280×720** (`SMT_MAP2D_SHOWCASE_W/H`) |
| Sample | `china_city` + product Style (hillshade when DEM present) |
| Leftover metric | `SMT_RHI2D_PARALLEL_LOG` → `execute_ms` (command-buffer replay) |
| src_render metrics | `export_ms` (software paint+BMP); `present_gpu_ms` (FlyCube present); **phase clocks** (Task 1) |

### Decisions

| Topic | Choice |
| --- | --- |
| Compare fairness | Report **phase** clocks; never claim leftover execute ≡ export |
| Product north star | **Warm** GPU StaticReuse present ≤ ~80 ms; software `paint_ms` ≤ ~100 ms |
| Cold first frame | Separate budget (layout + hillshade + upload); shrink via caches, not by skipping richness |
| Hillshade | Keep underlay; **cache** shade RGBA + overview DEM downsample |
| export_bmp | Default stays faithful (`clear_present_cache`); optional reuse env for bench only |
| Parallel grain | Prefer layout prep parallel + GPU reuse; do **not** copy leftover opaque layer wipe |

### Workstreams (see plan checkboxes)

1. **Phase clocks + warm/cold split** in showcase / matrix CSV  
2. **Cold present diet** — device reuse; avoid timed invalidate; Pass upload keep  
3. **Software export** — paint vs IO split; hillshade cache; optional cache blit for bench  
4. **Layout** — rebuild vs StaticReuse; overview LOD; optional parallel prep  
5. **GPU upload** — batch; hillshade single texture; warm upload ≈ 0  

### Acceptance

- Matrix still writes leftover + `src_render-china` BMPs at 1280×720  
- Warm `present_gpu_ms` ≤ **80**; `paint_ms` ≤ **100**; cold first present ≤ **400** after layout warm (Debug, china frame)  
- Visual: hillshade + city labels remain on inspect PNG  

### Non-goals

- Delete hillshade / MapFrame to match leftover IR-only ms  
- MapLibre Native shader port  
- Replacing leftover matrix (it stays A/B)

---

## §src_render Scene3d equal-profile optimize（2026-10-01）

**Status:** active  
**Updated:** 2026-10-01 — pin every-frame `GpuScene::rebuild_meshes` on atmosphere-showcase=legacy  
**Plan:** [`../plans/2026-10-01-src-render-scene3d-equal-profile-optimize.md`](../plans/2026-10-01-src-render-scene3d-equal-profile-optimize.md)  
**Code:** `content/browser/present/scene3d/**`, `effect/scene/**`; harness `--atmosphere-showcase=legacy`

### Why

Equal-profile vs leftover scene3d (same 640×480 HWND): warm `ms_per_present` was **~160–444 ms** Debug while leftover sits near **~10 ms**. Root cause: `Scene3dGpuPresent::present` forced `mark_meshes_dirty` whenever ocean/sky was on, and `rebuild_local_mesh` cleared DEM buffers so terrain cache never hit — full tessellate+upload every frame.

### Profile (locked)

| Axis | Value |
| --- | --- |
| Mode | `--atmosphere-showcase=legacy` (`kLegacyStereo`, ocean on, sky off) |
| Viewport | 640×480 (`kAtmosphereShowcaseW/H`) |
| Metric | warm `ms_per_present` (+ phase clocks) |
| Target | warm ≈ leftover **~10 ms** order; `rebuild_count=0` on timed frames |

### Decisions

| Topic | Choice |
| --- | --- |
| Ocean/sky remesh | Force DEM GPU remesh **once** after first height/depth alloc; warm in-place height upload keeps StaticReuse |
| DEM cache | Trim overlay fold; do **not** `clear()` local xyz before `rebuild_terrain_mesh` |
| Overlays | Re-attach only when DEM rebuilt or overlay dirty |
| Phase clocks | `mesh_ms` / `sync_ms` / `rebuild_ms` / `ocean_prep_ms` / `record_ms` / `present_swap_ms` |

### Acceptance

- [ ] Warm legacy `ms_per_present` near leftover ~10 ms (Debug)  
- [ ] Timed frames report `rebuild_count=0`  
- [ ] Visual: legacy BMP still passes landish / black-clear gate  

---

## §Leftover scene3d per-object showcase（2026-10-02）

**Status:** active  
**Updated:** 2026-10-02  
**Plan checklist:** [`../plans/2026-10-02-scene3d-surface-base-modern-cpp.md`](../plans/2026-10-02-scene3d-surface-base-modern-cpp.md)  
**Code:** `seed/map_to_scene.cc` (`seed_showcase_mode_into_scene`) · `scene/stereo_hwnd_view.cc` · `legacy/app/shell/showcase/scene3d.cc` · harness `testing/tools/harness/legacy/legacy.scene3d.*`

### Why

Composite `china` showcase covers terrain + GeoObject + MapLabelBatch + northarray together. Cube / sphere / water / pointcloud render paths and northarray-only had no separately runnable BMP harness.

### Modes (`--scene3d-showcase <mode>` / `SMT_SCENE3D_SHOWCASE_MODE`)

| Mode | Seeds | Harness suite | score_id |
| --- | --- | --- | --- |
| `china` (default) | DEM + vectors + labels | `legacy.scene3d.china` (+ `.d3d` / browse) | `legacy_scene3d_china` |
| `terrain` | DEM underlay only | `legacy.scene3d.terrain` | `legacy_scene3d_china` |
| `cube` / `sphere` / `water` | single mesh at origin | `legacy.scene3d.<mode>` | `legacy_scene3d_mesh` |
| `pointcloud` | synthetic CSV → `Smt3DPointCloud` | `legacy.scene3d.pointcloud` | `legacy_scene3d_mesh` |
| `northarray` | framing AABB only (HUD from `SmtScene::Setup`) | `legacy.scene3d.northarray` | `legacy_scene3d_mesh` |

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
| DEM / terrain | `gis::set_sample_dem_path_override` → `find_sample_dem_path` → `terrain_mesh::rebuild_terrain_mesh` (honors override; does not force China box) |
| Atmosphere | `AtmosphereSession::{set_sky,set_ocean,set_cloud,set_fog}_enabled` + `seed_procedural` via `apply_china_scene3d_atmosphere` |
| Satellite cloud | `AtmosphereSession::load_fields("<tif>:cloud_cover")` → FieldStore `kCloudCover` → `CloudPass` cover modulation; missing file → procedural |

### Non-goals (this §)

- WGS84 spherical globe mesh / clipmap paging (still P2).
- Live meteorological imagery CDN.

---

## §src_render + vista parallel accelerate（终态 · 2026-10-02）

**Status:** active  
**Updated:** 2026-10-02 — **代码级详设**（Types / Call graph / Grains / GPU path / Env / API sketches）；SVG 图与 as-built 函数名对齐  
**Plan:** [`../plans/2026-10-02-src-render-vista-parallel-accelerate.md`](../plans/2026-10-02-src-render-vista-parallel-accelerate.md)  
**Diagram:** [`../diagrams/src-render-vista-parallel-accelerate.html`](../diagrams/src-render-vista-parallel-accelerate.html)（浅色 SVG：L0–L3 + **命名阶段** `stage_frame` → `build_layout_parallel` → `prep_cull_parallel` → `record_and_present`）  
**CPU boundary diagram:** [`gis-vista-architecture.html`](../diagrams/gis-vista-architecture.html)（MapFrame POD 穿墙 → content/effect/graph）  
**Related diagrams:** [`ui-views-shell-architecture.html`](../diagrams/ui-views-shell-architecture.html) · [`legacy-render-architecture.html`](../diagrams/legacy-render-architecture.html)  
**Code:** `src/gis/vista/**` · `src/content/browser/present/{map2d,scene3d}/**` · `src/effect/{map,scene,atmosphere}/**` · `src/render/{rhi,graph,scene}/**` · `src/base/execution/**`  
**Reference (leftover as-built):** §rhi2d leftover tile-raster · §rhi3d leftover parallel frame  
**Visual rule:** `.cursor/rules/repo/design-html-diagrams.mdc`（技术架构浅色 · SVG-first）

### Why

Leftover proved Chromium-like **off-UI FrameJob + CPU prep×N + serial GPU submit** (D3D11 deferred optional). Product already has fragments（`emit_fills`/`emit_lines` `parallel_for`、`batches_via_parallel_for`、`prepare_for_present` StaticReuse、`graph::present` 单 CL）但缺一份**对接真实符号**的终态详设。Equal-profile §§ 管 **ms / StaticReuse 预算**；本 § 管 **线程 / 调度 / API / 热点路径 / env**。

### Core layers (L0–L3 · locked)

| Layer | Owner | May | Must not |
| --- | --- | --- | --- |
| **L0 UI** | Views HWND / Commit | stage FrameRequest + gen; shell DisplayList Commit; Submit | `join` layout / GPU workers; touch `rhi::Device` |
| **L1 Schedule** | `content` Map2d / Scene3d | gen / cancel / StaticReuse / phase clocks; wake workers | Own a second thread pool; draw with GDI HDC on product path |
| **L2 CPU** | `gis::vista` (+ GpuScene prep helpers) | `parallel_for` tess / optional tile layout / frustum cull; TLS scratch | `#include` `render/rhi`; HWND; record CommandList |
| **L3 GPU** | Display thread · `render::graph` / `effect` | sole `rhi::Device`; sync → **one** CL → `present`; publish gen | Multi-thread Draw on shared GL; default multi-CL |

Sole pool: **`base::execution`** (`GlobalNThreadPoolExecutor` / `parallel_for` in `src/base/execution/parallel/for.h`).

---

### 1. Types & ownership（谁分配什么）

| Type | Header / owner | Allocates | Consumed by | Notes |
| --- | --- | --- | --- | --- |
| `gis::vista::LayoutInput` | `gis/vista/frame/frame.h` · content fills | View + style* + tiles + `GlyphMetrics*`（非拥有） | `Layout::build` | 无 RHI；hillshade slots 由 cache bake |
| `gis::vista::LayerBatch` | same · content `map2d_batches` | `geoms*` / attrs（指向 MapScene 层，不拷贝 OGR） | `Layout::build` emitters | source_layer 匹配 |
| `gis::vista::MapFrame` | same · **cache 拥有** `cached_frame_` | `DrawItem[]` 顶点/索引堆 | `effect::map::Pass` / software painter | POD 穿 `gis ↛ rhi` 墙 |
| `gis::vista::Layout` | `layout.cc` · 无状态 | TLS arena clear 于 build 开头 | — | 纯函数式 CPU |
| `content::Map2dFrameCache` | `map2d_frame_cache.h` | MapFrame、hillshade RGBA、fingerprint、`layout_scratch_` | `Map2dGpuPresent` / software | `mu_` 保护 prepare/rebuild |
| `PresentAction` | cache enum | — | present 路径分支 | `kRebuildFull` / `kInteractiveReuse` / `kSettleRebuild` / `kStaticReuse` |
| `effect::map::Pass` | `effect/map/pass.h` · present 拥有 `map2d_pass_` | GPU VB/IB/纹理 upload 缓存 | `graph::present` via `MapEffect` | `invalidate_uploaded` 仅 cold |
| `effect::scene::GpuScene` | `effect/scene/scene.h` | `GpuMesh` / pipelines / instances | `record_draws` → graph opaque | remesh 在 Device 线程 |
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
            ├─ hillshade bake / cache (optional)
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

#### Scene3d — GPU present（as-built 骨架 → V3 prep）

```
Scene3dPresenter::present_gpu
  └─ Scene3dGpuPresent::present(device, …)                 // scene3d_gpu_present.cc
       ├─ [L2 target] prep_cull_parallel(instances, frustum)
       │     // workers: AABB only; 禁止 Device / CommandList
       ├─ AtmosphereFrame / OceanGpuFields::record (compute 可嵌套同 CL 前)
       └─ graph::present(ViewInput{ before, opaque GpuScene, after, overlay })
            └─ GpuScene::record_draws(device, list, …)      // scene_draw.cc
                 ├─ rebuild_meshes if dirty                 // Device 线程
                 └─ detail::record_kind(…, cull_frustum)    // 今日串行 cull
```

**Frustum as-built：** `effect/scene/frustum_aabb.cc` + `detail::frustum_cull_enabled()`（默认关；`SMT_SCENE3D_FRUSTUM_CULL=1`）。V3 在 **启用 cull 时** 把可见性标位的计算换成 `parallel_for`，record 仍串行读标位。

---

### 3. Parallel grains（统一 job→merge）

| Grain | As-built | Target rule |
| --- | --- | --- |
| Fill / line tess | `emit_fills` / `emit_lines`：collect `FillJob`/`LineJob` → `parallel_for` → **按 layer_ord 有序 merge** | 保留；加 `SMT_VISTA_LAYOUT_PARALLEL=0` → 强制串行 |
| Batch collect | `batches_via_parallel_for` per visible layer → `merge_parts` | 保留；属 L1 数据准备，非 RHI |
| Circle / heatmap / extrusion / symbol | 大多串行 | **同一模式**：job 向量 → `parallel_for` 写 per-index scratch → 保序 merge |
| Labels / collision | `LabelGrid` 在 `emit_symbols` | **必须串行**（在 parallel emit 之后）；禁止 worker 写共享 grid |
| Optional tile layout (V4) | 无 | device-pixel AABB 切 Layout 子任务；仍产出 **一个** `MapFrame`；compose 只走 `effect/map` |
| GpuScene prep (V3) | `mesh_culled` 在 record 内串行 | `prep_cull_parallel`：每 mesh 写 `visible[i]`；`worker_hint` clamp 2–4 |
| Worker 禁令 | TLS `mesh_scratch` / arena OK | **禁止** `rhi::Device`、HWND、`CommandList`、共享 GL context |

常量：`kParallelTessMinGeoms` / `kParallelTessGrain` in `gis/vista/frame/detail/layout/geom_mesh.h`（今日 `MinGeoms=2`）。

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
| FlyCube compute | `OceanGpuFields::record`（`effect/atmosphere/ocean/gpu_fields.cc`）在 **同一 Device 线程**、可在 present CL 前/内提交 compute；**不是** worker 线程开 Device |
| Equal-profile | 本 § 不改预算数字；并行只服务于 `layout_ms` / `prep_ms` 下降，且不得破坏 warm present 上限 |

---

### 5. Env & fallbacks

| Env | Default | Effect | Scope |
| --- | --- | --- | --- |
| `SMT_VISTA_LAYOUT_PARALLEL` | on（jobs ≥ `kParallelTessMinGeoms`） | `=0` → tess N=1 | **product** · vista emitters |
| `SMT_GPUSCENE_PREP_PARALLEL` | on（clamp 2–4） | `=0` → frustum prep N=1 | **product** · effect/scene prep |
| `SMT_SCENE3D_FRUSTUM_CULL` | off（as-built） | `=1` 启用 AABB cull（并行 prep 的前提） | product · 与 equal-profile 联调 |
| `SMT_RHI2D_PARALLEL` / `SMT_RHI2D_TILE_RASTER` | leftover | **禁止** product 路径 `getenv` | leftover only |
| `SMT_RHI3D_FRAME_JOB` / `*_PREP_PARALLEL` / `*_D3D_DEFERRED` | leftover | **禁止** product 路径 | leftover only |

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

// L2 — gis/vista/frame/detail/layout/*  (already: emit_fills / emit_lines)
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
| Fill / line `parallel_for` | Yes | + `SMT_VISTA_LAYOUT_PARALLEL` |
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
| tile / layer Raster×N | L2 tess + optional tile **layout**；compose = `effect/map` |
| rhi3d PrepRunner | `prep_cull_parallel` |
| D3D11 deferred | 默认 1 CL；禁 TLS deferred |

### Phases

| Phase | Deliverable | Status |
| --- | --- | --- |
| **V0** | Living 代码级 § + HTML 命名阶段 + README 指针 | **done（本修订）** |
| **V1** | 统一 emitters + `SMT_VISTA_LAYOUT_PARALLEL` | open |
| **V2** | `stage_frame` / async layout / gen cancel | open |
| **V3** | `prep_cull_parallel` + `SMT_GPUSCENE_PREP_PARALLEL` | open |
| **V4** | Optional viewport tile layout | open |
| **V5** | P3b multi-CL if `record_ms` hot | deferred |

### Relation to other §§

- **§rhi2d tile-raster** / **§rhi3d parallel frame**：leftover 语义参考 only。
- **§src_render Map2d / Scene3d equal-profile**：ms + StaticReuse；本 § 供并行机器。
- **Views §compositor thread**：L0/L3 线程角色。
- **§GPU-process accelerate**：进程外 compose 另册；in-process = 本 §。

### Acceptance

- [x] Living § + plan + SVG 图（L0–L3 + 命名流水线）
- [x] 代码级 Types / Call graph / Grains / GPU / Env / API sketches 落档
- [ ] Product env 在代码中接线 + `src/render/README.md` 已链（README 指针已加；**接线仍 open**）
- [ ] Map2d china：`LAYOUT_PARALLEL=0` 对照墙钟下降；warm StaticReuse 预算仍成立
- [ ] Scene3d：prep workers 不碰 RHI；warm `rebuild_count=0`
- [ ] Destroy / navigate：无 join hang；stale gen 永不 present
- [ ] Visual gates 不变（hillshade / labels / atmosphere legacy）

---

## Folded topics (2026-09-28 merge B)

Former hot specs are under `archive/specs/` (`superseded`). **Revise this file** (append `§`) for new requirements in this topic. Do not create a new `YYYY-MM-DD-*-design.md`.

| Former hot spec | Section / note |
| --- | --- |
| [`../archive/specs/2026-09-13-model-render-compute-design.md`](../archive/specs/2026-09-13-model-render-compute-design.md) | §Model / render / compute umbrella (folded) |
| [`../archive/specs/2026-09-14-render-math-refactor-design.md`](../archive/specs/2026-09-14-render-math-refactor-design.md) | §Render math (folded) |
| [`../archive/specs/2026-09-14-render-skia-canvas-design.md`](../archive/specs/2026-09-14-render-skia-canvas-design.md) | §Skia canvas chrome (folded) |
| [`../archive/specs/2026-09-19-atmosphere-ocean-cloud-design.md`](../archive/specs/2026-09-19-atmosphere-ocean-cloud-design.md) | §Atmosphere / ocean / cloud + weather domain (folded) |
| [`../archive/specs/2026-09-27-map2d-frame-design.md`](../archive/specs/2026-09-27-map2d-frame-design.md) | §Map2d CPU frame + Views present (folded) |

