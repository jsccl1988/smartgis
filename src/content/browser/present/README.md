<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/content/browser/present` — Chromium-style present stack

## Product source of truth (SoT)

| Lane | Product SoT | Opt-in exploratory |
| --- | --- | --- |
| map2d | **Vista `MapPass`** via `map2d/gpu` (+ shared `Map2dFrameCache` MapIR) | `--map2d-engine=scenic` → `ScenicRhi2dHost` / Map2dEngine |
| scene3d | **Vista `WorldPass`** via `scene3d/gpu` (+ tileset LRU stream) | `--scene3d-engine=scenic` → `ScenicScene3dHost` |

Default product builds never load `scenic.dll`. Scenic hosts **gate** on
`prefer_map2d_scenic()` / `prefer_scene3d_scenic()` and drop sticky engines when
the switch flips back to Vista — do not pay Scenic sync on the product path.

## File naming (locked)

| Pattern | Example |
| --- | --- |
| Facade | `{lane}_presenter.*` |
| Phase clocks | `{lane}_phase_profile.*` |
| GPU present | `{lane}/gpu/{lane}_gpu_present.*` |
| HDC / soft paint | `{lane}/hdc/{lane}_hdc_{unit}.*` |
| Frame / CPU IR | `{lane}/frame/{lane}_{unit}.*` |
| Shared host GDI | `host/gdi/gdi_{unit}.*` |
| Selection overlay (GPU) | `map2d/gpu/map2d_selection_overlay.*` |

Do **not** mix `gdi` / `software` / `hdc` prefixes inside `{lane}/hdc/`.
HDC opt-in is not a GPU sticky backup; product selection/flash for map2d is
MapIR overlay on MapPass.

## Layers

```
present/
  host/                 # BlitFrameCache; ShellOverlayEffect → render/graph
    gdi/                # Shared HDC primitives (ScopedGdiPen/Brush, halo text)
  map2d/
    map2d_presenter.*   # Thin facade: bind + forward to gpu / hdc
    frame/              # CPU compositor inputs: carto, LayerBatch, tile math
    gpu/                # MapPass present + selection MapIR overlay
    hdc/                # Opt-in MapIR→HDC (export / FORCE_GDI / ContentMapView)
    scenic/             # Opt-in ScenicRhi2dHost
  scene3d/
    scene3d_presenter.* # Thin facade: bind + present/paint
    session/            # Engine SoT (prefer_*) + Scene3dStereoSession
    frame/              # OrbitGeoFrame + rebuild_terrain_mesh
    atmosphere/         # Environment load + prepare_*
    gpu/                # WorldPass / present_gpu / shell overlay
    hdc/                # Soft DEM + HUD (uses host/gdi primitives)
```

| Layer | Role | Public include surface |
| --- | --- | --- |
| Facade (`*_presenter.h`) | Orchestration | Shell + tests |
| `frame/` | Frame / compositor inputs | Prefer internal |
| `gpu/` | GPU present (+ map2d selection overlay) | Facade or present-only hosts |
| `hdc/` | Opt-in HDC paint / export / HUD | Facade; harness FORCE_GDI |
| `atmosphere/` | Atmosphere session prep | `atmosphere_session()` |
| `session/` | Vista / Stereo / GDI SoT | Shell / MapSession |
| `host/` | Surface / preview cache | Shell gesture preview |
| `host/gdi/` | Thin Win32 GDI helpers | Product HDC paths only |

Namespaces stay `content` (internals in `content::detail`).

## Composition

- **map2d:** `Map2dPresenter` owns `Map2dGpuPresent` + `Map2dHdcPainter`
  sharing one `Map2dFrameCache`. Selection/flash strokes append on GPU present.
  HDC keeps a separate present DIB for opt-in StaticReuse/InteractiveReuse.
- **scene3d:** `Scene3dPresenter` owns `AtmosphereSession` + `Scene3dGpuPresent` +
  `Scene3dHdcPainter`. Callers use `atmosphere_session()` / `gpu()` / `hdc()`.
- **tileset:** `TilesetStreamSession` (`scene3d/frame/tileset_stream.*`).

## GN

- `:gis_present` — `host/` + `map2d/**`
- `:scene3d_present` — `scene3d/**` except `session/scene3d_rhi_session.*`
  (that TU stays in `:content` for `CONTENT_EXPORT`)
- Test: `map2d_hdc_frame_test` (MapIR→HDC unit)
