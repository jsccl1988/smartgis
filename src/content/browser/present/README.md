<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# src/content/browser/present — Chromium-style present stack

## Product source of truth (SoT)

| Lane | Product SoT | Opt-in exploratory |
| --- | --- | --- |
| map2d | **Vista MapPass** via map2d/gpu/present (+ shared Map2dFrameCache MapIR) | --map2d-engine=scenic → scenic::Engine (Map2dEngine) |
| scene3d | **Vista WorldPass** via scene3d/gpu/present (+ tileset LRU stream) | --scene3d-engine=scenic → ScenicScene3dHost |

**Soft HWND present is removed.** There is no `{lane}/software/`, `{lane}/hdc/`,
or `host/scene3d_cpu/` MapIR→HDC / soft DEM present path. HWND `paint()` is
scenic MemFrame only (or ocean fill / no-op). Selection/flash for map2d is
MapIR on MapPass. Product `export_bmp` paints MapIR (fill/line/hillshade) via
`frame/export/`; scenic `export_bmp` is `MAP2D_ENGINE=scenic` only.

Default product builds never load scenic.dll. Scenic hosts **gate** on
`prefer_map2d_scenic()` / `prefer_scene3d_scenic()`.

## File naming (locked)

| Pattern | Example |
| --- | --- |
| Facade | `{lane}/{lane}_presenter.*` (lane root) |
| Phase clocks | `{lane}/profile/{lane}_phase_profile.*` |
| GPU present | `{lane}/gpu/present/{lane}_gpu_present.*` |
| Frame / CPU IR | `{lane}/frame/<role>/{lane}_{unit}.*` |
| Shared host surface | `host/blit/`, `host/overlay/`, `host/scenic/`, `host/gdi/` |
| Selection overlay (GPU) | `map2d/gpu/selection/map2d_selection_overlay.*` |
| Scenic engine host | `scene3d/scenic/scenic_engine_host.*` |

## Layers

```
present/
  host/                      # Shared HWND / preview / scenic GIS bind
    blit/                    # BlitFrameCache (StretchBlt preview)
    overlay/                 # ShellOverlayEffect → render/graph
    scenic/                  # GisScene → scenic POD bind
    gdi/                     # Thin Win32 helpers (pen/brush/text) — not soft present
  map2d/
    map2d_presenter.*        # Facade: bind + gpu / scenic::Engine
    profile/                 # Phase clocks
    frame/
      cache/                 # Map2dFrameCache (MapIR gen / StaticReuse)
      carto/                 # Carto heuristics
      batches/               # LayerBatch POD build
      layout/                # LayoutInput → vista::Layout::build
      tile/                  # Mercator / tile math
    gpu/
      present/               # MapPass present
      selection/             # Selection / flash MapIR overlay
  scene3d/
    scene3d_presenter.*      # Facade: bind + present_gpu / scenic
    profile/                 # Phase clocks
    scenic/                  # ScenicScene3dHost (not a facade member API)
    session/                 # Engine SoT (prefer_*) + Scene3dStereoSession
    atmosphere/              # Environment load + prepare_*
    frame/
      orbit/                 # OrbitGeoFrame glue
      terrain/               # rebuild_terrain_mesh
      tileset/               # TilesetStreamSession LRU
      overlay/               # Pointcloud / TIN overlays + China labels
    gpu/
      present/               # WorldPass / present_gpu / shell overlay
```

| Layer | Role | Public include surface |
| --- | --- | --- |
| Facade (`*_presenter.h`) | Orchestration | Shell + tests |
| `frame/<role>/` | Frame / compositor inputs | Prefer internal |
| `gpu/present/` | GPU present | Facade |
| `gpu/selection/` | map2d selection/flash | Facade via gpu |
| `atmosphere/` | Atmosphere session prep | `atmosphere_session()` |
| `session/` | Vista / Stereo / Scenic SoT | Shell / MapSession |
| `scenic/` | scene3d scenic host | Facade |
| `profile/` | Phase clocks | Harness / matrix |
| `host/blit|overlay|scenic|gdi/` | Surface / preview / bind | Shell gesture preview |

Namespaces stay `content` (internals in `content::detail`).

Living lock: RHI umbrella **§Content present subdirectory layers** ·
diagram [`content-present-subdirectory-layers.html`](../../../../docs/superpowers/diagrams/content-present-subdirectory-layers.html).

## Chromium ↔ Topology A mapping

Semantics only — **no** FrameSink / Mojo Viz clone. Product default is
**Topology A** (in-process Views + FlyCube RHI, one CommandList).

| Chromium | SmartGIS Topology A |
| --- | --- |
| RWHV | `DrawHost` → `*_Presenter` bind |
| cc Commit / Activate | `Map2dFrameCache` + `PresentAction` |
| CompositorFrame | vista Pass + published MapFrame / World inputs |
| Viz Display Present | `graph::present` (1 CL) |

Living §: **§Content present ↔ Chromium mapping** · diagram §4 swimlanes in
[`content-present-subdirectory-layers.html`](../../../../docs/superpowers/diagrams/content-present-subdirectory-layers.html#chromium-swimlanes).

### Optimize on C / G / E (not a second Viz)

Remaining cost after warm StaticReuse is cold / settle / interactive. Work
lives in **§content present accelerate** (Cache / GPU compose / Execution):

| Lane | Role |
| --- | --- |
| **C** | fingerprint, published slot, PresentAction / StaticReuse |
| **G** | camera-only pan, incremental upload, textured hillshade; 1 CL |
| **E** | `stage_frame` mailbox, no same-tick `async`+`get`, HWND never joins |

Diagram: [`content-present-accelerate.html`](../../../../docs/superpowers/diagrams/content-present-accelerate.html).
Plan Task 7 checkboxes are hints — **C0 / E1 land status follows code + that §**.

## Composition

- **map2d:** `Map2dPresenter` owns `Map2dGpuPresent` + shared `Map2dFrameCache`.
  Selection/flash append on GPU present. No CPU painter member.
- **scene3d:** `Scene3dPresenter` owns `AtmosphereSession` + `Scene3dGpuPresent`.
  Callers use `atmosphere_session()` / `gpu()`. No soft DEM painter.
- **tileset:** `TilesetStreamSession` (`scene3d/frame/tileset/`).

## GN

- `:gis_present` — `host/` + `map2d/**` (no soft present TUs)
- `:scene3d_present` — `scene3d/**` except `session/scene3d_rhi_session.*`
  (that TU stays in `:content` for `CONTENT_EXPORT`)

---

**最后更新：** 2026-10-13
