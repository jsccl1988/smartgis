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
Equal-profile matrix cells may force scenic; that is not the ship default.

Facade (`map2d_presenter` / `scene3d_presenter`) routes to Vista GPU/software
unless the scenic switch is set. Leftover stereo/GDI session flags remain under
`scene3d/session/`.

Layout mirrors Chromium **compositor / software / gpu** adapted to this repo’s
colocation rule (`.h` next to `.cc`; no forwarding shims at old paths).

## Layers

```
present/
  host/                 # Surface helpers (BlitFrameCache; ShellOverlayEffect → render/graph)
  map2d/
    map2d_presenter.*   # Thin facade: bind + forward to gpu/software
    frame/              # CPU compositor inputs: carto, LayerBatch, tile math
    gpu/                # Pass lifetime, MapIR cache, present_gpu
    software/           # GDI fallback (Map2dSoftwarePainter + paint TUs)
  scene3d/
    scene3d_presenter.* # Thin facade: bind + present/paint + accessors only
    session/            # Engine SoT (prefer_*) + Scene3dStereoSession
    frame/              # OrbitGeoFrame + rebuild_terrain_mesh
    atmosphere/         # Environment load + prepare_* + M3 hooks
    gpu/                # WorldPass / present_gpu / shell overlay
    software/           # GDI HUD, wind, wireframe, engine-logo
```

| Layer | Role (Chromium analogue) | Public include surface |
| --- | --- | --- |
| Facade (`*/…_presenter.h`) | WebContents-ish orchestration | Shell + tests (may call nested types) |
| `frame/` | Frame / compositor inputs | Prefer internal; carto also used by document tests |
| `gpu/` | GPU present + cache / mesh | Facade or direct for hosts that only present |
| `software/` | Software (GDI) paint | Facade or direct for HUD / export |
| `atmosphere/` | Atmosphere session prep | `atmosphere_session()` (not Presenter forwards) |
| `session/` | Vista / Stereo / GDI SoT + leftover stereo LoadLibrary | Shell / MapSession (`scene3d_rhi_session` is `CONTENT_EXPORT` in `content.dll`) |
| `host/` | Surface / preview cache | Shell gesture preview |

Namespaces stay `content` (internals in `content::detail`). Input bridging stays
in `../input/`; camera/orbit in `../camera/`; layers/features in `../document/`.
**`software/` must not live under `src/render`** — those TUs implement content
presenters and would reverse-depend on content.

## Composition (scheme C)

- **map2d:** `Map2dPresenter` owns `Map2dGpuPresent` + `Map2dSoftwarePainter`
  sharing one `Map2dFrameCache` (MapIR layout). Software keeps a separate GDI
  present DIB for StaticReuse/InteractiveReuse; GPU MapPass reuses the same
  MapIR — do not big-bang merge pixel caches.
- **scene3d:** `Scene3dPresenter` owns `AtmosphereSession` + `Scene3dGpuPresent` +
  `Scene3dSoftwarePainter`. Callers use `atmosphere_session()` / `gpu()` /
  `software()` for domain toggles (no pure-forward API on the facade).
- **tileset:** `TilesetStreamSession` (`scene3d/frame/tileset_stream.*`) pumps
  select → LRU `ensure` under `kDefaultMaxTiles` / `kDefaultMaxEnsure`; product
  present wires those defaults (Vista `TilesetContentCache` already LRU-evicts).

## GN

- `:gis_present` — `host/` + `map2d/**`
- `:scene3d_present` — `scene3d/**` except `session/scene3d_rhi_session.*` (that TU stays in `:content` for `CONTENT_EXPORT`)

## Verify

```bat
build.bat
build.bat gis_scene_test
build.bat scene3d_presenter_test
```
