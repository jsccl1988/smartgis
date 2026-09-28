<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/app/views/present` — Chromium-style present stack

Capability directory for **map presentation** (not document data, not shell chrome).
Layout mirrors Chromium content/UI separation adapted to this repo’s colocation
rule (`.h` next to `.cc`; no forwarding shims at old paths).

## Layers

```
present/
  host/           # Present surface helpers (BlitFrameCache preview DIB)
  map2d/
    map2d_presenter.*     # Thin public facade + orchestration (GPU + export)
    frame/                # Compositor inputs: carto policy, LayerBatch, tile math
    paint/                # GDI fallback paint (Map2dPresenter paint TUs)
  scene3d/
    scene3d_presenter.*   # Thin public facade + mesh/camera/present_gpu
    session/              # SoT policy (`prefer_scene3d_flycube`) + stereo LoadLibrary
    frame/                # OrbitGeoFrame + atmosphere field prep → pass POD
    paint/                # GDI HUD, wind, wireframe, engine-logo overlay
```

| Layer | Role (Chromium analogue) | Public include surface |
| --- | --- | --- |
| Facade (`*/…_presenter.h`) | WebContents / Browser-ish orchestration | Shell + tests |
| `frame/` | Frame / compositor pipeline helpers | Prefer internal; carto also used by document tests |
| `paint/` | Software paint path | Implementation TUs only |
| `session/` | SoT policy + stereo LoadLibrary | Shell when choosing FlyCube vs stereo |
| `host/` | Surface / preview cache | Shell gesture preview |

Namespaces stay `app` (internals in `app::detail` where needed). Input bridging
stays in `../input/`; camera/orbit in `../camera/`; layers/features in `../document/`.

## GN

- `:map_present` — `host/` + `map2d/**`
- `:scene3d_present` — `scene3d/**` (deps `:map_present` for label overlay)

## Verify

```bat
build.bat
```
