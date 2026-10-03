<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/content/browser/present` — Chromium-style present stack

Capability directory for **map presentation** (not document data, not shell chrome).
Layout mirrors Chromium **compositor / software / gpu** adapted to this repo’s
colocation rule (`.h` next to `.cc`; no forwarding shims at old paths).

## Layers

```
present/
  host/                 # Surface helpers (BlitFrameCache, ShellOverlayEffect)
  map2d/
    map2d_presenter.*   # Thin facade: bind + forward to gpu/software
    frame/              # CPU compositor inputs: carto, LayerBatch, tile math
    gpu/                # Pass lifetime, MapFrame cache, present_gpu
    software/           # GDI fallback (Map2dSoftwarePainter + paint TUs)
  scene3d/
    scene3d_presenter.* # Thin facade: bind + present/paint + accessors only
    policy/             # Scene3dEngine runtime (set_scene3d_engine / prefer_*)
    stereo/             # Scene3dStereoSession (legacy_render LoadLibrary)
    frame/              # OrbitGeoFrame + rebuild_terrain_mesh
    atmosphere/         # Environment load + prepare_* + M3 hooks
    gpu/                # GpuScene / present_gpu / shell overlay
    software/           # GDI HUD, wind, wireframe, engine-logo
```

| Layer | Role (Chromium analogue) | Public include surface |
| --- | --- | --- |
| Facade (`*/…_presenter.h`) | WebContents-ish orchestration | Shell + tests (may call nested types) |
| `frame/` | Frame / compositor inputs | Prefer internal; carto also used by document tests |
| `gpu/` | GPU present + cache / mesh | Facade or direct for hosts that only present |
| `software/` | Software (GDI) paint | Facade or direct for HUD / export |
| `atmosphere/` | Atmosphere session prep | `atmosphere_session()` (not Presenter forwards) |
| `policy/` | FlyCube / Stereo / GDI runtime SoT (`SMT_SCENE3D_ENGINE`) | Shell when choosing present path |
| `stereo/` | Legacy stereo LoadLibrary | MapSession / Browser |
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
- `:scene3d_present` — `scene3d/**` (deps `:map_present` for label overlay)

## Verify

```bat
build.bat
build.bat map_scene_test
build.bat scene3d_presenter_test
```
