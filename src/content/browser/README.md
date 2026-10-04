<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/content/browser` — Chromium-style browser process

Responsibility split (scheme C). Headers colocated with sources; **no**
forwarding shims at old paths. Embedders still include **`content/public/`**
only; these directories are internals.

**As-built (C10):** 12 siblings under `browser/` (no root TUs).  
**Target (C11):** fold thin DLL impls into `contents/`, gestures into
`session/`, flatten `document/` helpers — living **§Content browser
subdirectory tighten**. Present / debug / camera / capability stay.

```
browser/   # C10 now → C11 target
  bootstrap/ catalog/ attrs/ plugin/   # C11 → contents/
  contents/                            # MapContents; C11 also public-impl TUs
  session/                             # MapSession; C11 + MapHwndGestures
  input/                               # C11 → session/
  document/{store,ingest,query,style,edit}/  # C11 files at document/
  camera/                              # keep (already flat)
  present/                             # keep C3 nest (see present/README.md)
    map2d/{frame,gpu,software}
    scene3d/{session,frame,atmosphere,gpu,software}
  capability/                          # keep
  debug/                               # keep cmd/policy/schema/wire
```

| Dir | Owns | GN |
| --- | --- | --- |
| `contents/` | MapContents pipe; C11: bootstrap/catalog/attrs/plugin impls | `:content` |
| `session/` | In-process WebContents-ish owner; C11: HWND gestures | `:map_session` |
| `document/` | MapScene + helpers | `:map_scene` |
| `camera/` | 2D view + 3D orbit + navigation | `:map_camera` |
| `present/` | Facades + CPU frame + GPU + GDI software | `:map_present` / `:scene3d_present` |
| `present/scene3d/session/` | Engine SoT (`CONTENT_EXPORT` in DLL) + leftover stereo | `:content` + `:scene3d_present` |
| `capability/` | Harness verb host | `:capability` |
| `debug/` | Loopback console agent | `:debug_agent` |

`software/` must not live under `src/render` (would reverse-depend on content).
`render` must not depend on `content`. Public C++ stays two layers (`content`).

Do **not** merge `content::MapSession` with `gis::MapEditSession`, or hosted
`detail::MapLayer` with `gis::MapLayer`.

Tests sit next to the code they cover (`*_test.cc`). `map2d_presenter_test.cc`
is still compiled into `:map_scene_test` (one exe, two colocated TUs).

---

**最后更新：** 2026-10-04
