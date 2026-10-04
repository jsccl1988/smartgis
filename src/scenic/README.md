<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/scenic` — Scenic engine (product DLL)

Previous-generation map/scene engine. Peer of `src/vista`. **Not** a FlyCube `render::rhi::Device` port. **Not** merged into `vista.dll`.

| Layer | Role |
| --- | --- |
| Public façade | Root umbrellas `engine.h` + `scenic_export.h` — `namespace scenic` only (`Engine` / `SessionDesc` / `DrawItem` / `ViewXform` / `OrbitXform` / factories). DLL **`scenic.dll`**. Hosts name these types; no rhi2d/rhi3d/scene3d/gis types |
| `engine/` | Product stubs (`Map2dEngine` / `Scene3dEngine` / Null) + `mem_frame.h`. No `scenic/detail/` dump dir |
| `render/` | Copy shared: `err.h` / `frame.h` / `scenic_impl_export.h` + `backend_dll.h` + `rhi2d/` + `rhi3d/`. Point/Rect/vectors come from `base/math`. No `render/detail/` dump. 2D map paint + carto live under `render/rhi2d/impl/common/paint/{map,carto}/` |
| `scene3d/` | 3D scene graph / primitives (peer of `vista/scene`; leftover layout `scene/` + `primitive/`) |
| Host | Exploratory. Default presenters **do not** link `scenic.dll`. Direction: scenic may use product `gis::`; scenic ↛ content/app/leftover |

**Compile isolation:** `scenic.dll` is **not** in `src_all` / `//:all` / e2e / te. Explicit `build.bat debug scenic` builds the façade; product GIS (`//src/gis`) is allowed, leftover/content/app are not. `//src:src_all` `assert_no_deps` includes `//src/scenic:*`.

**Smt strip (copy trees):** scenic-owned types use unprefixed names (`Style`, `RenderOptions2d`, `Object3d`, …). Device status is `scenic::detail::Err` (`kErrNone`, …) in `render/err.h`. DEM/coord live in `//src/vista`; OGR tess lives in `scene3d/primitive/feature/`.

Living lock: [`docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md`](../../docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md) **§Scenic**.

## Layout vs vista

| Scenic | Vista |
| --- | --- |
| `render/rhi2d/.../paint/{map,carto}/` | `map/` (Vista peer is product GPU Pass; Scenic map paint stays under rhi2d) |
| `scene3d/` | `scene/` |
| `render/{rhi2d,rhi3d}` | (FlyCube lives in `src/render`; vista GPU consumes it) |
| façade `Engine` | `Pass` / `GpuScene` public types |

## GN

| Target | Stem / note |
| --- | --- |
| `//src/scenic:scenic` | **`scenic`** — product `shared_library` (C++23, not `//build:legacy`). **Not** in `src_all`. Opt-in `ninja scenic`. `assert_no_deps` content/app/tool/plugin/vista/render/leftover + copy engines (gis allowed) |
| `//src/scenic:scenic_copy_all` | Opt-in copy engines (`scenic_impl`, `scenic_rhi2d_*`, `scenic_render_gl`, `scenic_render_d3d`). Product gis/vista; no leftover. **Not** in `src_all` |

```bat
build.bat debug scenic
build.bat debug scenic_engine_test
```
