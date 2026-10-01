<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/legacy/render` subdirectory layout + GDI dual-run (MapLibre parity)


> **Status: superseded** (2026-09-28 merge). Merged into umbrella §SP2 (legacy render layout / dual-run). Do not revise here except mechanical link fixes.

**Date:** 2026-09-27  
**Status:** accepted  
**Scope:** Dual-run policy for leftover `legacy_render` while Views / `map2d` / RHI remain the endgame; per-top-level subdirectory layout under `src/legacy/render`; GDI ↔ MapLibre-style capability parity goals; delete `gdi_simple`.  
**Landing note:** P1 (`gdi_simple` deleted + CreateDevice alias) and P2 (subdir split; `gdi/gdiaux/` for Windows `aux` reservation) landed 2026-09-27. Layout choice revised same day: **colocated `.h`+`.cpp`** (abandon headers-only-at-top). Same day follow-up: `render3d/` renamed to `rhi/`; thin tops `model3d/` / `terrain/` / `pointcloud/` absorbed into `scene3d/` (single `scene3d_sources`). **P2.4:** `scene3d/` re-layered — `scene/` `primitive/` `feature/` `surface/` `dem/` `bridge/` `test/`. **P2.5:** `rhi/` deep B′; top `gl/` → `rhi/impl/gl/` (sibling of `public/`). **P2.6:** abstract modules under `rhi/public/`; strip `gl_` file basename prefix under `impl/gl/` (types/`SmtGL*` unchanged; GN `test("gl_map_paint_test")` label kept). **P2.7:** `rhi/public/` re-layered to industry RHI seams — `device/` `resource/` `shader/` `texture/` `state/` `camera/` (collapse former `buffer/`/`gpu/`/`material/`); `impl/gl/` unchanged internals. **P2.8:** `rhi/impl/d3d/` D3D11 leftover (`SmtD3DRenderDevice` / `CreateD3DRenderDevice`). **P2.9 (2026-09-28):** tops `bridge/` + `gdi/` collapsed — 2D public + GDI → `rhi2d/`; `leftover_*` → `rhi3d/public/bridge/` ([rhi2d layout](2026-09-28-legacy-rhi2d-layout-design.md)). **P2.10 (2026-09-28):** leftover 3D top `rhi/` → `rhi3d/` (sibling of `rhi2d/`). Tops = `rhi2d/` `rhi3d/` `scene3d/`. P3 Must audit filled; shared road-width extract and build gates remain open in the plan.  
**Plan:** [`../plans/2026-09-27-legacy-render-subdirectory-dual-run.md`](../plans/2026-09-27-legacy-render-subdirectory-dual-run.md)  
**Related:**

| Topic | Doc | Relation |
| --- | --- | --- |
| Leftover split (landed path) | [`2026-09-13-render-legacy-split-design.md`](2026-09-13-render-legacy-split-design.md) | **accepted** — tops already under `legacy/render`; this spec refines *inside* each top |
| Present / paint facade | [`2026-09-19-legacy-render-present-facade-design.md`](2026-09-19-legacy-render-present-facade-design.md) | **active** — HWND present + Null record; dual-run present rules stay |
| GDI+ carto / MapLibre subset | [`2026-09-19-leftover-gdiplus-carto-design.md`](2026-09-19-leftover-gdiplus-carto-design.md) | **active** — AA / roads / labels already on main GDI |
| Views 2D → RHI | [`2026-09-27-views-2d-map-rhi-design.md`](2026-09-27-views-2d-map-rhi-design.md) | **active** — modern default present path |
| CPU MapFrame | [`2026-09-27-map2d-frame-design.md`](2026-09-27-map2d-frame-design.md) | **active** — `gis::map2d::Layout` → `render::map2d::Pass` |
| GPU process MapLibre pin | [`../archive/specs/2026-09-27-maplibre-out-of-gpu-design.md`](../archive/specs/2026-09-27-maplibre-out-of-gpu-design.md) | **superseded** — Native pin deleted (deferred reconsider); not GDI |
| Peer layout | [`2026-09-27-tool-subdirectory-layout-design.md`](2026-09-27-tool-subdirectory-layout-design.md), [`2026-09-27-atmosphere-subdirectory-layout-design.md`](2026-09-27-atmosphere-subdirectory-layout-design.md) | Same idea: responsibility dirs, stable aggregate GN |
| Scene3d / DEM | [`2026-09-19-leftover-scene3d-dem-unify-design.md`](2026-09-19-leftover-scene3d-dem-unify-design.md) | Layout of `scene3d/` must not fight DEM unify |

**As-built pointers (update when landing):** [`../../../src/legacy/render/README.md`](../../../src/legacy/render/README.md), [`../../../src/legacy/render/rhi2d/impl/gdi/README.md`](../../../src/legacy/render/rhi2d/impl/gdi/README.md), [`../../../src/legacy/render/rhi3d/BUILD.gn`](../../../src/legacy/render/rhi3d/BUILD.gn), [`../../build/src-layout.md`](../../build/src-layout.md). Layout of former tops `bridge/` + `gdi/` → see [`2026-09-28-legacy-rhi2d-layout-design.md`](2026-09-28-legacy-rhi2d-layout-design.md).

---

## 1. Goal / Non-goals

### 1.1 Goal

1. **Dual-run for a while:** keep `//src/legacy/render:legacy_render` (`dll_stem = legacy_render`) loadable for MFC / leftover hosts alongside the modern Views path (`gis::map2d` + `render::map2d::Pass` / `GpuScene` + `render::rhi`). Legacy is a **strangler**, not the destination.
2. **Subdirectory refactor inside each top-level** under `src/legacy/render` so fat modules (`gdi`, `rhi`, `scene3d`; GL under `rhi/impl/gl`) match peer layout norms (directory = responsibility; nesting cap `legacy/render/<top>/<module>/`).
3. **GDI capability alignment** with the MapLibre-style stack used by modern 2D (StyleDocument layers, painter order, AA, road casing/fill, label collision / along-line, raster tiles) — expressed as a must / should / later matrix, not a full Style Spec port.
4. **Delete `src/legacy/render/gdi_simple`** (sources, GN, exports, stale LoadLibrary names). One GDI device remains: `SmtRhi2dRenderDevice`.

### 1.2 Non-goals

- **Do not** make GDI the Views default present path (Views 2D stays RHI / map2d; GDI is fallback / leftover HWND owner).
- **Do not** vendor a second MapLibre-native renderer into `legacy/render`, or pull `mbgl` / `mln` into the `legacy_render` DLL.
- **Do not** introduce Qt, revive D3D9, or deepen nesting past `legacy/render/<top>/<module>/…`.
- **Do not** rewrite GDI/GL/scene3d algorithms in the layout phase — layout + delete + parity wiring only where specified.
- **Do not** reverse the dependency rule: `src/render` / `src/gis` must not gain new deps on `legacy/render`.
- **Do not** change `dll_stem` (`legacy_render`) or break `bind_rhi_present` / `smt_leftover_session` export names in this workstream (present-facade owns those semantics).
- **Do not** commit or branch unless the user asks; work stays on **master**.

---

## 2. Inventory (2026-09-27, CBM + tree)

Top-level under `src/legacy/render/` (10 folders + root `BUILD.gn`):

| Top | Role today | Scale (approx files) | Dual-run note |
| --- | --- | --- | --- |
| `bridge/` | `SmtRenderDevice` / `SmtRenderer`, `leftover_*` → RHI Null / GpuScene | ~13 | Strangler seam; keep stable |
| `gdi/` | Main 2D GDI(+); `map_carto2d`; shared `gdi_common_sources` | ~26 | Dual-run 2D pixels for MFC HWND |
| `gdi_simple/` | Second device + shim headers → `gdi` common | ~15 | **Delete** (this spec) |
| `gl/` | *(absorbed)* → `rhi/impl/gl/` | — | No longer a top under `legacy/render` |
| `rhi/` | Abstract 3D API + OpenGL/D3D11 impl under `impl/gl` + `impl/d3d` (was `render3d/` + top `gl/`) | ~90+ | Shared by scene; not modern `src/render/rhi` |
| `scene3d/` | `SmtScene`, DEM, map↔scene bridge; drawables under `primitive/` `feature/` `surface/` (was tops `model3d/` / `terrain/` / `pointcloud/`) | ~40 | Parallel to modern `GpuScene` / World |

**GN:** root `smt_shared_library("legacy_render")` deps all `*_sources` including `gdi_simple:render_gdi_simple_sources`. Optional; not in `src_all`.

**Callers of note:**

- `SmtRenderer::CreateDevice` loads `legacy_render[_d].dll`; supports `"SmtRhi2dRenderDevice"` (`CreateRenderDevice`) and `"SmtGdiSimpleRenderDevice"` (`CreateGdiSimpleRenderDevice`).
- Product default in `SmtApp::Init`: `str2DRenderDeviceName = "SmtRhi2dRenderDevice"` — **simple is not the default**.
- `MapViewport::try_local_device` still lists obsolete stems `render_gdi_simple_d.dll` / `render_gdi_d.dll` / … before modern RHI paths — stale dual-run residue; clean when deleting simple.
- Shared aux/bufpool/renderbuf already live once in `gdi:gdi_common_sources`; `gdi_simple` headers mostly `#include` those.

**Modern path (contrast):**

```
StyleDocument + features + viewport
        → gis::map2d::Layout (CPU MapFrame)
        → render::map2d::Pass (RHI record)
        → render::rhi::Device present (Views HWND)

GPU process: no MapLibre Native pin (removed; tile path is StyleDocument only)
```

Legacy GDI still owns BitBlt present on its `Init(HWND)`; after paint, best-effort `leftover_record_map_frame` (present-facade).

---

## 3. Approaches considered

| Approach | Idea | Pros | Cons |
| --- | --- | --- | --- |
| **A. Docs only** | Keep flat tops; only write parity matrix | Zero churn | Fat `.cpp` stay unnavigable; simple keeps rotting |
| **B. Headers-at-top + impl subdirs** (P2 initial) | Public `.h` stay at `legacy/render/<top>/…`; move `.cpp` into responsibility subdirs; delete `gdi_simple` | Smaller include churn during P2 | Headers and bodies split; harder to navigate |
| **B′. Colocated `.h`+`.cpp`** (**chosen**) | Same responsibility subdirs as B, but each public/internal header lives **next to** its `.cpp`/`.cc`; includes become `legacy/render/<top>/<module>/file.h` | Directory = unit; matches “统一目录” request | Include-path + GN source path churn for all callers |
| **C. Scheme C break-all includes** (superseded by B′) | Force every include under `<module>/` without pairing discipline | Strict directory=API | Same blast as B′ without the pairing rule |

**Recommendation: B′ (colocated).** P2 landed B first; revise to B′ so fat tops use one folder per unit (`device/gdi_renderdevice.h` + `.cpp`). Thin leftover geo objects live under `scene3d/{primitive,feature,surface}/` (not separate tops under `legacy/render`). `bridge/` stays flat. Header-only companions without a paired TU may stay at the top root or move with their module (prefer module when clearly owned). Prefer **delete `gdi_simple` before** deep GDI layout so we do not move dead code (done in P1).

**Irreversible fork (default locked):** deleting `gdi_simple` with a same-change string alias (`"SmtGdiSimpleRenderDevice"` → `CreateRenderDevice`) is the default. Keeping the directory as forever-shim is rejected.

---

## 4. Locked decisions

| # | Decision |
| --- | --- |
| 1 | Dual-run: `legacy_render` remains opt-in DLL; Views endgame stays map2d/RHI/Skia chrome. |
| 2 | Layout **B′ colocated `.h`+`.cpp`** inside fat tops; thin tops may stay flat. |
| 3 | Nesting cap: `src/legacy/render/<top>/<module>/…` — no deeper public trees. Includes use the module path. |
| 4 | Aggregate GN label stays `//src/legacy/render:legacy_render`; per-top `*_sources` source_sets. |
| 5 | **Delete** `gdi_simple/` (all sources, RC, icons, BUILD, `RENDER_GDI_SIMPLE_EXPORTS`). |
| 6 | Compat: `CreateDevice("SmtGdiSimpleRenderDevice")` **aliases** to `CreateRenderDevice` in the same change; drop `CreateGdiSimpleRenderDevice` export. |
| 7 | Clean stale `render_gdi_simple*.dll` / `render_gdi*.dll` names from `MapViewport::try_local_device` (and any cutover docs) in the delete change. |
| 8 | GDI parity work targets **main GDI + `map_carto2d`**; modern gaps close on `gis::map2d` / `render::map2d` when both must improve. |
| 9 | No Qt; no `src/render` → legacy deps; English comments on touched code. |
| 10 | Git: **master** only; no feature branch for this workstream. |
| 11 | OpenGL leftover is **`rhi/impl/gl/`** (sibling of `public/`, not a `legacy/render/gl` top). Abstract rhi modules under `rhi/public/`: `device/` `resource/` `shader/` `texture/` `state/` `camera/`. No old-path shims. |

---

## 5. Dual-run policy

```
┌─────────────────────────────────────────────────────────────┐
│ Views / SmartGisViews (endgame)                             │
│  chrome: ui/gfx                                        │
│  map 2D: map2d Layout → Pass / GpuScene → rhi present       │
│  fallback: SMT_FORCE_* → ContentMapView / GDI overlay       │
└─────────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────────┐
│ Leftover MFC / xview / SmartGis.exe (strangler, dual-run)   │
│  SmtRhi2dRenderDevice::Init(HWND) → BitBlt present            │
│  bind_rhi_present → leftover_session Null record            │
│  optional GL / scene3d on their Init HWND                   │
└─────────────────────────────────────────────────────────────┘
```

**Rules:**

1. **Present ownership:** GDI/GL keep exclusive present on the HWND they `Init`. Views FlyCube uses a **separate** HWND (present-facade).
2. **Feature policy during dual-run:** new 2D cartographic behavior lands first (or jointly) on `gis::map2d` + StyleDocument; GDI gets a **parity backport** only when leftover hosts still need it (must-row in §7).
3. **Exit criteria (later workstream):** leftover 2D hosts gone or permanently on RHI; then `legacy_render` GDI can shrink. This spec does **not** schedule full DLL deletion.
4. **Tests:** keep `map_carto2d_test` / `gdi_map_paint_test` green for dual-run; Views tests stay on map2d/RHI.

---

## 6. Target subdirectory layout (**B′ colocated**)

Public includes are `legacy/render/<top>/<module>/<header>.h` when the paired TU lives under `<module>/`. Thin / root-paired units keep `legacy/render/<top>/<header>.h`. Windows reserved name: use `gdiaux/` not `aux/`.

### 6.1 `bridge/` (light)

```
bridge/
  BUILD.gn
  renderdevice.h|cpp
  renderer.h|cpp
  leftover_session.*
  leftover_record.*
  leftover_mesh.*
  *_test.cc
  # optional later: session/ record/ mesh/ if files grow
```

Stay flat for now; only split if a file crosses ~1k lines with a clean seam.

### 6.2 `gdi/` (after `gdi_simple` gone)

```
gdi/
  BUILD.gn
  README.md
  resource.h / gdi_render_device.rc   # RC companion stays at top
  device/      gdi_renderdevice.h|cpp
  thread/      gdi_renderthread.h|cpp (+ _draw.cpp)
  buffer/      gdi_renderbuf.*, gdi_bufpool.*
  gdiaux/      gdi_aux_api.*, gdi_gdiplus.*   # not aux/ (Windows reserved)
  carto/       map_carto2d.h|cc (+ carto test)
  test/        gdi_map_paint_test.cc
```

`gdi_common_sources` / `render_gdi_sources` labels stay; `sources =` and include paths use module dirs.

### 6.3 `gdi_simple/` — **removed**

No replacement directory. No shim tree under `gdi/simple/`.

### 6.4 `gl/` — **absorbed into `rhi/impl/gl/`**

No separate top under `legacy/render`. Former `legacy/render/gl/**` lives at `legacy/render/rhi3d/impl/gl/**` (internal `device/` `caps/` `ext/` `buffer/` `text/` `test/` preserved; `gl_` file basename prefix stripped — e.g. `prerequisites.h`, `device/3drenderdevice.cpp`; `SmtGL*` types unchanged). Includes: `legacy/render/rhi3d/impl/gl/...`. No forwarding shim at old `legacy/render/gl/...` or pre-`public/` paths. **D3D11** leftover impl lives at `rhi3d/impl/d3d/` (`SmtD3DRenderDevice`, export `CreateD3DRenderDevice`, API string `"Direct3D"`). Chosen over D3D12 / resurrecting D3D9+D3DX; maps to leftover enum slot `RA_D3D09`. Not modern `src/render/rhi`.

GN: `//src/legacy/render/gl:render_gl_sources` → `//src/legacy/render/rhi3d/impl/gl:gl_sources` (compat group `:render_gl_sources` / parent `:gl_sources` / `:gl` forward to `legacy_render` or the source_set). D3D: `//src/legacy/render/rhi3d/impl/d3d:d3d_sources`. Test: `//src/legacy/render/rhi3d/impl/gl:gl_map_paint_test` (source `test/map_paint_test.cc`; GN target label kept).

### 6.5 `rhi/` (was `render3d/` + top `gl/`)

Abstract leftover 3D API lives under **`rhi/public/<module>/`**; backends live under **`rhi/impl/gl/`** (OpenGL) and **`rhi/impl/d3d/`** (D3D11), siblings of `public/`. Module names follow industry RHI public-API seams (Vulkan/D3D12/Metal conceptual layering) rather than a leftover `gpu/` catch-all.

| Module | Industry rationale | Sources |
| --- | --- | --- |
| `device/` | Device + caps + defs + factory/renderer entry (VkDevice / ID3D12Device) | `3drenderdevice.h`, `3drenderer.h|cpp`, `3ddevicecaps.h`, `3drenderdefs.h`, `base.h` |
| `resource/` | Buffer / GPU memory resources (VkBuffer / ID3D12Resource) | `vertexbuffer.h`, `indexbuffer.h`, `renderbuffer.h`, `videobuffer.h|cpp` |
| `shader/` | Shader modules + linked programs / managers (VkShaderModule / PSO shaders) | `shader.h|cpp`, `shadermanager.h|cpp`, `program.h|cpp`, `programmanager.h|cpp` |
| `texture/` | Images + views + framebuffers / managers (VkImage / VkFramebuffer) | `texture.h|cpp`, `texturemanager.h|cpp`, `framebuffer.h|cpp` |
| `state/` | Pipeline / blend / depth state + material / light / color | `states.h`, `statesmanager.h|cpp`, `3dmaterial.cpp`, `color.cpp`, `light.cpp` |
| `camera/` | View / projection / frustum (GIS leftover; keep name; flat under `public/camera/`, no subtype subdirs) | `camera.h` (base/ortho/persp/fps/arbv + factory), `combinedcamera.h|cpp`, per-type `*.cpp`, `frustum.cpp` (stub; `SmtFrustum` stays header-only in `device/base.h`) |

Rejected: keeping vague `gpu/`; nesting `impl/` under `public/`; empty placeholder folders (`pipeline/`, `pass/`). Nesting stays `legacy/render/rhi3d/public/<module>/file` — no third public directory level. No old-path shim headers. `Smt_*` / export / `dll_stem` unchanged.

```
rhi/
  BUILD.gn                 # rhi_sources (abstract); groups :rhi :gl :gl_sources
  public/
    device/     3drenderdevice.h 3drenderer.h|cpp 3ddevicecaps.h 3drenderdefs.h base.h
    resource/   indexbuffer.h vertexbuffer.h renderbuffer.h videobuffer.h|cpp
    shader/     shader|shadermanager|program|programmanager .h+.cpp
    texture/    texture|texturemanager|framebuffer .h+.cpp
    state/      states.h statesmanager.h|cpp + 3dmaterial/color/light .cpp
    camera/     camera.h combinedcamera.h|cpp + per-type *.cpp frustum.cpp
  impl/
    gl/         # entire former legacy/render/gl/ tree (see §6.4)
      BUILD.gn  # gl_sources + gl_map_paint_test
      prerequisites.h
      device/ caps/ ext/ buffer/ text/ test/
    d3d/        # D3D11 leftover (SmtD3DRenderDevice); no D3DX
      BUILD.gn  # d3d_sources
      prerequisites.h
      device/ caps/ buffer/ ext/
```

**Include migration (old → new):**

| Old path prefix | New path prefix |
| --- | --- |
| `…/rhi/public/buffer/` | `…/rhi/public/resource/` |
| `…/rhi/public/gpu/{shader,shadermanager,program,programmanager}.*` | `…/rhi/public/shader/…` |
| `…/rhi/public/gpu/{texture,texturemanager,framebuffer}.*` | `…/rhi/public/texture/…` |
| `…/rhi/public/gpu/videobuffer.*` | `…/rhi/public/resource/…` |
| `…/rhi/public/material/` | `…/rhi/public/state/` |
| `…/rhi/public/device/states.h` | `…/rhi/public/state/states.h` |
| `…/rhi/public/impl/gl/` (brief nested layout) | `…/rhi/impl/gl/` |

Include prefix: `legacy/render/rhi3d/public/<module>/...` for abstract API; `legacy/render/rhi3d/impl/gl/...` for GL; `legacy/render/rhi3d/impl/d3d/...` for D3D11. Distinct from modern `src/render/rhi/`.

### 6.6 `scene3d/` (P2.4 responsibility layers)

**Chosen scheme (CBM seams):** split former catch-all `object/` and peer `terrain/`/`pointcloud/` by *what the TU is*, and rename `map_bridge/` → `bridge/`. Rejected: a single `content/` bag (would only rename the mess). Nesting stays `legacy/render/scene3d/<module>/file` — no third public directory level. No old-path shim headers. `Smt_*` / export / `dll_stem` unchanged.

| Module | Responsibility | Sources |
| --- | --- | --- |
| `scene/` | Scene graph + spatial index + `Smt3DObject` base | `bl3d_scene*`, `bl3d_*octtree*`, `bl3d_object.h`, `bl3d_bas_struct.h` |
| `primitive/` | Geometric drawable primitives | `cube`, `sphere`, `water`, `northarray` |
| `feature/` | OGR / style feature wrappers on `Smt3DObject` | `2dgeoobject`, `3dgeoobject` |
| `surface/` | Large surface / sample content | `terrain`, `pointcloud` |
| `dem/` | Height field + stereo HWND / terrain view | `dem_height_field`, `dem_to_world`, `stereo_*` |
| `bridge/` | Map ↔ leftover scene / World adapters (was `map_bridge/`) | `map_to_scene`, `map_label_batch`, `scene_to_world` |
| `test/` | DEM / stereo unit tests | `dem_stereo_test.cc` |

```
scene3d/
  BUILD.gn
  README.md
  scene/       bl3d_*.h + octtree .cpp (incl. header-only bl3d_object / bas_struct)
  primitive/   cube.* sphere.* water.* northarray.*
  feature/     2dgeoobject.* 3dgeoobject.*
  surface/     terrain.* pointcloud.*
  dem/         dem_height_field.* dem_to_world.* stereo_*
  bridge/      map_to_scene.* map_label_batch.* scene_to_world.*
  test/        dem_stereo_test.cc
```

**Include migration (old → new):**

| Old path prefix | New path prefix |
| --- | --- |
| `…/scene3d/object/{cube,sphere,water,northarray}.*` | `…/scene3d/primitive/…` |
| `…/scene3d/object/{2d,3d}geoobject.*` | `…/scene3d/feature/…` |
| `…/scene3d/terrain/` | `…/scene3d/surface/` |
| `…/scene3d/pointcloud/` | `…/scene3d/surface/` |
| `…/scene3d/map_bridge/` | `…/scene3d/bridge/` |
| `…/scene3d/scene/` / `dem/` / `test/` | unchanged |

Align with leftover-scene3d-dem-unify: path-only moves; do not rename types / export names. Includes: `legacy/render/scene3d/<module>/<file>.h`.

### 6.7 Thin tops — absorbed

Former `model3d/`, `terrain/`, `pointcloud/` tops are **gone**; sources live under `scene3d/{primitive,feature,surface}/` and compile in `scene3d_sources`.

### 6.8 Root after land

```
src/legacy/render/
  BUILD.gn                 # legacy_render DLL; no gdi_simple dep; no RENDER_GDI_SIMPLE_EXPORTS
  README.md                # dual-run + layout pointer (new)
  bridge/ gdi/ rhi/ scene3d/
  # gdi_simple/ GONE; gl/ GONE (into rhi/impl/gl/); model3d/ terrain/ pointcloud/ GONE
  #   → scene3d/{primitive,feature,surface}/
```

---

## 7. GDI ↔ MapLibre-style capability parity

Reference stacks:

- **Legacy:** `SmtRhi2dRenderDevice` + `map_carto2d` + GDI+ (leftover-gdiplus-carto).
- **Modern:** StyleDocument + `gis::map2d::Layout` / `MapFrame` + `render::map2d::Pass` (no MapLibre Native pin).

| Capability | Must (dual-run) | Should | Later |
| --- | --- | --- | --- |
| Painter order (fill → line → symbol; road casing before fill) | GDI + map2d agree on default carto order | — | — |
| GDI+ AA lines/polygons/text | GDI (already in carto design) | map2d/RHI MSAA / SDF text quality | Full MLN text |
| Road casing + fill from class/kind | GDI table + Style JSON layers | Shared width table constant | Data-driven expressions |
| Label fields `anno`/`name`/`text` | Both | — | `text-field` expressions |
| Grid / hash collision + LOD budget | GDI (`map_carto2d`); map2d (ported) | Tune shared budgets | Global cross-frame optimizer |
| Along-line label rotation | Both | — | `symbol-placement: line` full curve |
| Text halo | Both (constants) | — | Variable halo from zoom stops |
| Background color / opacity | Both | — | Pattern backgrounds |
| Raster XYZ / TileProvider underlay | Modern (Views); GDI only if leftover host needs it | GDI blit of cached tiles | WMTS / vector tiles in GDI |
| Circle / dash / fill-pattern | map2d | Backport to GDI if leftover demos need | heatmap / hillshade / fill-extrusion |
| Heatmap / hillshade / fill-extrusion / interpolate | — | — | Modern only; **never** block dual-run |
| Full `mbgl::Map` in-process | — | — | Explicit non-goal for legacy |

**Parity definition for “must”:** same sample (`china` / `china_city` style fixtures) is **readable** on leftover GDI and on Views map2d/RHI (not pixel-identical). Gate with existing `map_carto2d_test` / `gdi_map_paint_test` plus Views/map2d tests.

---

## 8. `gdi_simple` removal plan

### 8.1 Why delete now

- Aux implementation already deduped into `gdi_common_sources`.
- Default product device string is already `SmtRhi2dRenderDevice`.
- Second ~1.7k-LOC device duplicates draw paths and doubles present-facade touch points.
- Stale DLL stems in `MapViewport` confuse dual-run debugging.

### 8.2 Strangler steps (single landing preferred)

1. Inventory `#include "legacy/render/gdi_simple/…"`, `CreateGdiSimple*`, `RENDER_GDI_SIMPLE_*`, docs/cutover mentions.
2. In `SmtRenderer::CreateDevice`, map `"SmtGdiSimpleRenderDevice"` → `CreateRenderDevice` / `DestroyRenderDevice` (log once optional).
3. Remove `gdi_simple` from root `BUILD.gn` deps and `RENDER_GDI_SIMPLE_EXPORTS` define/config.
4. Delete the `gdi_simple/` directory.
5. Strip obsolete LoadLibrary candidates in `MapViewport::try_local_device`.
6. Update `gdi/README.md`, new `legacy/render/README.md`, `docs/build/abi-rename-map.md` / cutover tools if they still name `render_gdi_simple`.
7. Build `legacy_render` + run `gdi_map_paint_test` / `map_carto2d_test`; smoke leftover CreateDevice alias if a fixture exists.

**Not in-scope as a silent drive-by before this plan’s Task 1:** bulk layout moves. Delete may land as its own change set ahead of layout.

---

## 9. Phased order (recommended)

| Phase | Work | Why this order |
| --- | --- | --- |
| **P0** | Inventory + docs (this spec/plan) | Shared map for agents |
| **P1** | Delete `gdi_simple` + alias + stale DLL names | Shrink surface **before** moving GDI files |
| **P2** | Subdirectory layout per fat top (gdi → gl → rhi → scene3d; thin absorbed into scene3d) | Mechanical; one top per change when possible |
| **P3** | MapLibre-style parity (must → should) | Behavior; uses stable layout homes |

Do **not** start P3 feature work inside files that P2 is about to move.

---

## 10. Risks

| Risk | Mitigation |
| --- | --- |
| Hidden LoadLibrary of `CreateGdiSimpleRenderDevice` | Alias + ripgrep/CBM sweep of export name; e2e leftover smoke |
| Include churn breaks plugin / xview | Scheme B keeps public header paths; update only moved internals |
| Dual-run pixel drift while parity lands | Fixture tests; “readable not identical” bar |
| Parallel agents collide on `gdi/` | Partition: P1 owns `gdi_simple`+bridge CreateDevice; P2 one top at a time |
| scene3d layout vs DEM unify | Coordinate with leftover-scene3d-dem-unify; prefer `dem/` name alignment |

### Open questions (non-blocking defaults)

1. **How long does the `"SmtGdiSimpleRenderDevice"` string alias live?** Default: keep until leftover hosts are audited once after delete; remove in a follow-up doc-noted cleanup, not a second device.
2. **Does any external plugin ship `render_gdi_simple.dll`?** Default assume no (DLL merge already landed); if found, document load failure → upgrade to `legacy_render`.

---

## 11. Success criteria

1. Living spec + checkbox plan exist; indexed from `docs/README.md`.
2. `gdi_simple/` absent; `legacy_render` links without `RENDER_GDI_SIMPLE_EXPORTS`; CreateDevice alias works.
3. Fat tops have responsibility subdirs (or an explicit “keep flat” note in `legacy/render/README.md` for thin tops).
4. Parity matrix §7 tracked in the plan with must-rows either done or checkbox-open.
5. `map_carto2d_test` + `gdi_map_paint_test` green; Views map2d/RHI path unchanged in P1–P2.
6. No new `src/render` → `legacy` dependency; no Qt.

---

## 12. Spec self-review

- No TBD placeholders; defaults chosen for irreversible delete.
- Consistent with present-facade dual HWND rules and map2d endgame.
- Scope is one living topic (legacy render dual-run + layout + simple delete + parity phasing); capability algorithms stay in related specs.
