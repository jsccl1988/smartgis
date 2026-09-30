<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/legacy/render` (leftover dual-run)

Optional leftover engines ship as one DLL: `legacy_render` / `legacy_render_d` (`//src/legacy/render:legacy_render`). **Not** in `src_all`. Endgame 2D/3D pixels live under `src/render` (`rhi`, `scene`, `map2d`, `skia`, `atmosphere`).

## Dual-run

| Host | Present | Notes |
| --- | --- | --- |
| Leftover MFC / xview | GDI, GL, or D3D11 on `Init(HWND)` | BitBlt / SwapBuffers / `IDXGISwapChain::Present` own that HWND; `bind_rhi_present` records Null only — see present-facade spec |
| Views (`SmartGisViews`) | RHI / `map2d` on map HWND | Skia shell; GDI overlay is fallback via env flags |

Living design: [`docs/superpowers/specs/2026-09-27-legacy-render-subdirectory-dual-run-design.md`](../../../docs/superpowers/specs/2026-09-27-legacy-render-subdirectory-dual-run-design.md)  
`rhi2d` layout: [`docs/superpowers/specs/2026-09-28-legacy-rhi2d-layout-design.md`](../../../docs/superpowers/specs/2026-09-28-legacy-rhi2d-layout-design.md)

## Tops (B′ colocated layout)

Fat tops nest under `legacy/render/<top>/…`. After the `rhi2d` collapse, tops are only `rhi2d/` `rhi3d/` `scene3d/` — **no** separate `bridge/` or `gdi/` tops.

| Directory | Role | Subdirs |
| --- | --- | --- |
| `rhi2d/` | Leftover 2D abstract API + GDI(+) impl | `public/device/` + `detail/` + `impl/gdi/{host,worker,paint,surface,res,test}/` |
| `rhi3d/` | Leftover abstract 3D API + OpenGL/D3D11 + leftover strangler | `public/{device,resource,shader,texture,state,camera,bridge}/` + `impl/gl/` + `impl/d3d/` |
| `scene3d/` | Leftover scene + DEM + former model/terrain/pointcloud | `scene/` `primitive/` `feature/` `surface/` `dem/` `bridge/` `test/` |

`gdi_simple/` is **removed**. `"SmtGdiSimpleRenderDevice"` aliases to `CreateRenderDevice` / `SmtGdiRenderDevice`. No `RENDER_GDI_SIMPLE_EXPORTS`.

Windows note: path segment `aux` is reserved; GDI+ helpers live under `rhi2d/impl/gdi/paint/gdiplus/` (no `aux/` or `gdiaux/` dir).

### `rhi2d/` layout note

- Abstract includes (header-only, like `rhi3d/public/device/`): `legacy/render/rhi2d/public/device/{renderdevice,renderer}.h`.
- DLL implementation TUs: `legacy/render/rhi2d/detail/{bind_rhi_present,renderer}.cpp`.
- GDI includes: `legacy/render/rhi2d/impl/gdi/…`.
- GN: `//src/legacy/render/rhi2d:rhi2d_sources` + `impl/gdi:render_gdi_sources` → `legacy_render`.
- Tests: `map_carto2d_test`, `gdi_map_paint_test`.

### `rhi3d/` layout note

- Abstract includes: `legacy/render/rhi3d/public/{device,resource,shader,texture,state,camera}/…`.
- Leftover strangler: `legacy/render/rhi3d/public/bridge/leftover_*.h` (`leftover_mesh` / `LeftoverRecorder` / `smt_leftover_session`).
- GL includes: `legacy/render/rhi3d/impl/gl/…`. D3D11: `legacy/render/rhi3d/impl/d3d/…`.
- GN: `//src/legacy/render/rhi3d:rhi_sources` + `leftover_*` + `impl/gl` + `impl/d3d` → `legacy_render`.
- Distinct from modern `src/render/rhi/`.

## MapLibre-style parity (P3)

Must-row dual-run capabilities (painter order, AA, road casing/fill, label fields, collision/LOD, along-line labels, text halo, background) are tracked in the dual-run design §7 matrix. Heatmap / hillshade / fill-extrusion / full `mbgl::Map` are **Deferred** (modern-only; never block dual-run).

## Build

```bat
.\build.bat legacy_render
.\build.bat te map_carto2d_test
.\build.bat te gdi_map_paint_test
.\build.bat te gl_map_paint_test
.\build.bat te leftover_mesh_test
.\build.bat te leftover_record_test
.\build.bat te leftover_session_test
```
