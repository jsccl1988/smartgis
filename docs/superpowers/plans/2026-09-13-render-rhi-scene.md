<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Unified RHI + dual scene Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Land a FlyCube-shaped `render::rhi` (DX12 + Vulkan create paths), move logical models/scenes into `sdb`, and sync a GPU scene that records 2D and 3D into one command list.

**Architecture:** Public Facade in `src/render/rhi` (FlyCube private). CPU assets and World in `src/sdb/{model,scene}`. `src/render/scene` is the GPU cache. Leftover GDI/GL/`SmtRenderDevice` keep HWND present.

**Tech Stack:** C++20, GN/Ninja (`build.bat`), FlyCube (optional), Assimp, tinygltf, existing `SmtMap` / `SmtLog` leftover.

## Global Constraints

- Work on `master` only. Do not create branches or worktrees.
- Do not `git commit` unless the user explicitly asks.
- Copyright: `Copyright (c) 2026 The Mogu Authors.` on every new engineering file; bump year on touched Mogu headers.
- Public namespaces: two levels (`render::rhi`, `sdb::model`, `sdb::scene`, `render::scene`). Deeper = `detail` or anonymous.
- New functions `snake_case`. Types PascalCase.
- Comments in English. Class purpose in one or two sentences.
- No Qt. No D3D9/D3DX. No Cesium Native / OSG / Filament.
- No FlyCube / Assimp / tinygltf includes in public `src/` headers.
- Product C++20. Do not force `cc_std` onto third_party CMake.
- Output only under repo-root `out/`. Tests use `testing/test.gni` `test()` + `expect`/`main` like `sde_gdal_test`.
- `SmtRenderDevice::Init` + `BindRhiPresent` must keep compiling.
- Exact APIs: copy from `docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md`.

## File map

| Path | Responsibility |
| --- | --- |
| `src/render/rhi/rhi.h` | Facade types |
| `src/render/rhi/rhi.cc` | `create_device` / `preferred_gpu_backend` |
| `src/render/rhi/null_rhi.cc` | Null device + stub lists |
| `src/render/rhi/gdi_rhi.cc` | Leftover HWND present |
| `src/render/rhi/gl_rhi.cc` | Leftover HWND present |
| `src/render/rhi/flycube_rhi.cc` | DX12/Vulkan stub (real FlyCube when `smt_has_flycube`) |
| `src/render/rhi/rhi_test.cc` | Null + backend identity |
| `src/sdb/model/model.h` `.cc` | CPU mesh + cube + `load_file` |
| `src/sdb/model/tileset.h` `.cc` | tileset.json + `select_tiles` |
| `src/sdb/model/model_test.cc` | cube + parser + SSE |
| `src/sdb/scene/scene.h` `.cc` | World |
| `src/sdb/scene/scene_test.cc` | generation + AABB |
| `src/render/scene/scene.h` `.cc` | GpuScene |
| `src/render/scene/scene_test.cc` | sync + record |
| `BUILD.gn` / module BUILD.gn | wire tests into `test_all` |
| `third_party/manifest.json` | flycube, assimp, tinygltf pins |
| docs listed in the spec | layout accuracy |

Tasks 1–3 have **no shared files** and may run in parallel. Task 4 depends on 1 and 3. Task 5 is docs + manifest (can parallel with 1–3 except `src/README.md` / `src-layout.md` if those are also edited elsewhere — do docs last).

---

### Task 1: RHI Facade + null / leftover / FlyCube stub

**Files:**
- Modify: `src/render/rhi/rhi.h`
- Modify: `src/render/rhi/rhi.cc`
- Modify: `src/render/rhi/null_rhi.cc`
- Modify: `src/render/rhi/gdi_rhi.cc`
- Modify: `src/render/rhi/gl_rhi.cc`
- Create: `src/render/rhi/flycube_rhi.cc`
- Create: `src/render/rhi/rhi_test.cc`
- Modify: `src/render/BUILD.gn`
- Modify: `BUILD.gn` (`test_all` add `//src/render:rhi_test`)

**Interfaces:**
- Consumes: existing `BindRhiPresent` → `create_device(kGdi)`
- Produces: spec `Device` / `CommandList` / `create_device` / `preferred_gpu_backend`

- [ ] **Step 1: Write `rhi.h` to the spec surface** (full file in spec). Include `<cstdint>`. Put `StubCommandList` in the header as a concrete recorder used by null/gdi/gl:

```cpp
class StubCommandList : public CommandList {
 public:
  uint32_t draw_indexed_calls = 0;
  bool closed = false;
  bool pass_open = false;
  void set_viewport(float, float, float, float, float, float) override {}
  void begin_render_pass(const RenderPassDesc&) override { pass_open = true; }
  void end_render_pass() override { pass_open = false; }
  void draw_indexed(uint32_t, uint32_t, uint32_t, int32_t, uint32_t) override {
    ++draw_indexed_calls;
  }
  void close() override { closed = true; }
};
```

- [ ] **Step 2: Null / GDI / GL / FlyCube devices all implement `create_command_list` / `destroy_command_list` / `execute`**. `execute` on stub lists returns `list != nullptr && static_cast<StubCommandList*>(list)->closed`. FlyCube stub: `initialize` returns false; `backend()` returns the requested `kDx12` or `kVulkan`. `preferred_gpu_backend()` returns `kDx12` on `_WIN32`.

- [ ] **Step 3: `rhi_test.cc`** — `expect` helpers like `sde_gdal_test`. Cases: null init; draw_indexed count 1 after record/execute; preferred is kDx12; kDx12 and kVulkan pointers non-null.

- [ ] **Step 4: Wire `test("rhi_test")` in `src/render/BUILD.gn`** with `include_dirs += [ "//src" ]`, `deps = []` (rhi is in `:render` — link `:render` plus its existing deps). Add to `//:test_all`.

- [ ] **Step 5: Run `.\build.bat te`**. Expected: `rhi_test` PASS. `BindRhiPresent` still compiles inside `SmtRender`.

---

### Task 2: `sdb::model` + 3D Tiles parser

**Files:**
- Create: `src/sdb/model/model.h`, `model.cc`, `tileset.h`, `tileset.cc`, `BUILD.gn`, `model_test.cc`
- Modify: `src/sdb/BUILD.gn` (add `//src/sdb/model:model` to `group("sdb")`)
- Modify: `BUILD.gn` (`test_all` add `//src/sdb/model:model_test`)

**Interfaces:**
- Consumes: none of Task 1
- Produces: `load_unit_cube`, `load_file`, `parse_tileset_json`, `select_tiles`

- [ ] **Step 1: Unit cube** — 8 positions (24 floats), 12 triangles (36 indices), name `"cube"`.

- [ ] **Step 2: `load_file`** — if `path` is `"cube"` (or equals `"cube"` with no directory), call `load_unit_cube` and return true. Otherwise return false (Assimp not linked).

- [ ] **Step 3: `parse_tileset_json`** — scan for `"root"` object. Read `geometricError` as double (default 0). `refine` REPLACE/ADD. `content.uri` string. `boundingVolume.box` 12 numbers: center (c0,c1,c2) + half-axes; AABB is center ± |ax|+|bx|+|cx| on each axis. `boundingVolume.region` 6 numbers (radians/meters): use as minx,miny,minz,maxx,maxy,maxz directly. Recurse `children` arrays. Reject if no root.

- [ ] **Step 4: `select_tiles`** as spec (REPLACE omits parent when a child is selected).

- [ ] **Step 5: Tests** — cube size; missing file false; JSON with root + one child; `max_sse` 1e9 selects only root; `max_sse` 0 with child geometricError 0 still terminates; REPLACE does not include parent when child selected.

---

### Task 3: `sdb::scene::World`

**Files:**
- Create: `src/sdb/scene/scene.h`, `scene.cc`, `BUILD.gn`, `scene_test.cc`
- Modify: `src/sdb/BUILD.gn`
- Modify: `BUILD.gn` (`test_all`)

**Interfaces:**
- Consumes: `SmtMap` / `SmtLayer` from `//src/sdb/map:gis` for `attach_map` only
- Produces: `World` as spec

- [x] **Step 1: Implement World** with `std::vector<Node>`, `next_id_` starting at 1, `generation_` starting at 1. `add_node` appends and increments generation. `remove_node` erases and increments. `query_aabb` inclusive overlap: `!(a.max < b.min || b.max < a.min)` per axis.

- [x] **Step 2: `attach_map`** — remove existing kVectorLayer/kRasterLayer nodes, then for each layer `GetLayerCount` / iterate `SmtMap` (use `MoveFirst`/`GetNext` or index API if present). If the map API is iterator-only, walk it. Name = `GetLayerName` if available, else `"layer"`. Kind: raster if class name / type says raster, else vector. Envelope: `GetEnvelope` / `GetRect` if those exist; else zeros.

- [x] **Step 3: Tests without a real map** — add two overlapping/non-overlapping nodes; query; remove; generation; `attach_map(nullptr)` does not bump generation.

---

### Task 4: `render::scene::GpuScene`

**Files:**
- Create: `src/render/scene/scene.h`, `scene.cc`, `BUILD.gn`, `scene_test.cc`
- Modify: `src/render/BUILD.gn` (`render_all` += `//src/render/scene:scene`)
- Modify: `BUILD.gn`

**Interfaces:**
- Consumes: `render::rhi` (Task 1), `sdb::scene::World` (Task 3)
- Produces: `GpuScene::sync_from` / `record`

- [ ] **Step 1: `sync_from`** copies all world nodes into `GpuInstance` when generation differs.

- [ ] **Step 2: `record`** uses `RenderPassDesc` clear (0,0.2,0.4,1), viewport 0,0,width,height,0,1; one `draw_indexed(3,1,0,0,0)` per instance; close. Return false if `list` is null or width/height is 0.

- [ ] **Step 3: Test** — World with one kVectorLayer and one kModel; after sync instance_count==2; record on null device list; second sync same generation keeps count.

---

### Task 5: Manifest pins + docs

**Files:**
- Modify: `third_party/manifest.json` — append flycube, assimp, tinygltf (GitHub URLs; `install_skip` true until cmake is wired)
- Modify: `docs/build/src-layout.md`, `src/README.md`, `README.md`, `docs/README.md` per spec

- [ ] **Step 1:** Pins with notes “smartgis-owned; fetch later; GN stubs compile without source”.
- [ ] **Step 2:** Docs: RHI backends DX12/Vulkan; model/scene locations; leftover scene3d/model3d. Root README **最后更新** 2026-09-13.

---

### Task 6: Leftover 2D/3D record + raster texture bind (`src/render`)

Original Task 3 (`sdb::scene::World`) is landed. Remaining product gap under `src/render`: leftover GDI/GL must record through `render::rhi`, and raster/tile quads must bind an uploaded texture when pixels exist.

**Files:**
- Modify: `src/render/rhi/rhi.h`, `rhi_test.cc`
- Modify: `src/render/scene/scene.h`, `scene.cc`, `unified_draw_test.cc`
- Create: `src/legacy/render/bridge/leftover_record.h`, `leftover_record.cc`, `leftover_record_test.cc`
- Modify: leftover GDI `RenderMap` and leftover GL `DrawIndexedPrimitives`
- Modify: `src/render/BUILD.gn`, `src/render/scene/BUILD.gn`, leftover GDI/GL BUILD.gn, root `BUILD.gn` / `build.bat`

- [x] **Step 1:** RHI `Texture` + `create_texture` / `upload_texture` / `bind_texture`. Null-path counters on `StubCommandList`.
- [x] **Step 2:** `GpuScene` uploads raster/tile pixels when `TessMesh::has_image` and `GetRasterNoClone` / tile buf has data; solid quad if empty. `record_draws` does not close the list.
- [x] **Step 3:** `LeftoverRecorder` records World/map 2D then leftover VB/IB 3D on one Device/list. GDI / GDI-simple `RenderMap` and GL `DrawIndexedPrimitives` call it. `leftover_record_test` + existing render tests.

---

### Task 7: FlyCube present pin + 2D style/camera + model/tileset record (2026-09-13 GIS loop)

**Progress (landed):**

- [x] `LeftoverRecorder::ensure_device` prefers `preferred_gpu_backend()` **when HWND is set** (DX12), else null; `set_native_window` + GDI/GDI-simple/thread pass `m_hWnd`.
- [x] `bind_rhi_present` creates preferred GPU first, GDI leftover fallback.
- [x] RHI `CommandList::set_solid_color` + FlyCube `ColorCB` solid PS; default brush cyan via `GpuScene::set_solid_color_from_colorref`.
- [x] `GpuScene::set_view_ortho` / map envelope from `SmtMap::get_envelope` in `record_map` (zoom/camera seam).
- [x] `GpuInstance` syncs `model` / `tileset`; `rebuild_meshes` uploads `ModelAsset` via `flatten_meshes`; tileset draws AABB bridge (`tessellate_aabb`) until content decode feeds meshes.
- [x] **MapViewport**: `try_flycube_device()` before LoadLibrary GDI; `SMT_PREFER_GDI_DEVICE=1` opt-out for leftover DLL.
- [x] **Layer style brush**: `record_map` resolves first `MapLayer::style_name` via `SmtStyleManager` into GpuScene default solid (cyan fallback). Per-layer Node brush + per-feature `Feature::style()` during tessellate still TODO (avoided style DLL in sdb/scene).
- [x] **Tileset content**: `visible_uris` → `decode_content_file` → flatten into GpuScene mesh; AABB fallback; `scene_gpu_test` fixture (null backend).
- [x] **FlyCube test stability**: `rhi_test` null path green; skips HWND GPU unless `SMT_RUN_FLYCUBE_GPU=1`; ColorCB BindingSet cached; `NullDevice` destroy_* leaks stubs (FlyCube-linked CRT hang); `scene_gpu_test` / `leftover_record_test` / `unified_draw_test` default null-only.

Depth/blend Facade growth deferred.

---

## Self-review

- Spec RHI surface → Task 1
- Assimp seam + cube + 3D Tiles → Task 2
- World + AABB + attach_map → Task 3
- Dual scene GPU → Task 4
- FlyCube not leaked → Task 1 stub + Task 5 pin
- Leftover BindRhiPresent → Task 1 / Task 7 preferred present
- Docs → Task 5
- No placeholders left in APIs
- Types: `Backend::kDx12`, `NodeKind`, `GpuInstance.node_id` consistent
