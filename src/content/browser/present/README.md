<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/content/browser/present` — Chromium-style present stack

Facade (`map2d_presenter` / `scene3d_presenter`) presents Vista/FlyCube and leftover
stereo/GDI session flags. `src/scenic` is exploratory and is **not** linked from
this stack; `MAP2D_ENGINE=scenic` / `SCENE3D_ENGINE=scenic` do not compile
or load `scenic.dll` on the default product graph.

Layout mirrors Chromium **compositor / software / gpu** adapted to this repo’s
colocation rule (`.h` next to `.cc`; no forwarding shims at old paths).

## Layers

```
present/
  host/                 # Surface helpers (BlitFrameCache, ShellOverlayEffect)
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
| `session/` | FlyCube / Stereo / GDI SoT + leftover stereo LoadLibrary | Shell / MapSession (`scene3d_rhi_session` is `CONTENT_EXPORT` in `content.dll`) |
| `host/` | Surface / preview cache | Shell gesture preview |

Namespaces stay `content` (internals in `content::detail`). Input bridging stays
in `../input/`; camera/orbit in `../camera/`; layers/features in `../document/`.
**`software/` must not live under `src/render`** — those TUs implement content
presenters and would reverse-depend on content.

## Composition (scheme C)

- **map2d:** `Map2dPresenter` owns `Map2dGpuPresent` + `Map2dSoftwarePainter`.
  Call sites may use the facade or `gpu()` / `software()`.
- **scene3d:** `Scene3dPresenter` owns `AtmosphereSession` + `Scene3dGpuPresent` +
  `Scene3dSoftwarePainter`. Callers use `atmosphere_session()` / `gpu()` /
  `software()` for domain toggles (no pure-forward API on the facade).

## GN

- `:map_present` — `host/` + `map2d/**`
- `:scene3d_present` — `scene3d/**` except `session/scene3d_rhi_session.*` (that TU stays in `:content` for `CONTENT_EXPORT`)

## Verify

```bat
build.bat
build.bat map_scene_test
build.bat scene3d_presenter_test
```
