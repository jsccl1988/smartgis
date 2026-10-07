<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/content/browser` — Chromium-style browser process

Responsibility split (scheme C). Headers colocated with sources; **no**
forwarding shims at old paths. Embedders include **`content/public/`** only
for the abstract surface; concrete adapters may be included by shell TUs that
already dep `:gis_scene` / `:browser_session`.

**As-built (C11):** thin C10 dirs folded; `document/` uses responsibility
subdirs with scene core at the root.

```
browser/
  contents/     # GisContents + gis_bootstrap + catalog_layers + plugin_host
  session/      # BrowserSession + GisHwndGestures
  document/
    gis_scene.* / gis_document.*   # scene core + GisDocument adapter (root)
    store/ edit/ ingest/ query/ style/
  camera/       # ViewFrame, OrbitFrame, ViewNavigation
  present/      # C3 nest (see present/README.md)
  capability/   # IL Host (header-only)
  debug/        # DebugAgent + cmd/policy/schema/wire
  child/        # ChildProcessHost
```

| Dir | Owns | GN |
| --- | --- | --- |
| `contents/` | GisContents pipe + bootstrap/catalog/plugin_host | `:content` |
| `session/` | BrowserSession + HWND gestures | `:browser_session` / `:gis_hwnd_gestures` |
| `document/` | GisScene + GisSceneDocument + helpers | `:gis_scene` |
| `camera/` | 2D view + 3D orbit + navigation | `:gis_camera` |
| `present/` | Facades + CPU frame + GPU + GDI software | `:gis_present` / `:scene3d_present` |
| `capability/` | Harness verb host | `:capability` |
| `debug/` | Loopback console agent | `:debug_agent` |
| `child/` | OOP child launch helper | `:content` |

`gis_bootstrap` (sample path policy) lives under **`contents/`**, not
`document/store`. Feature token POD helpers live in **`gis/feature/attrs.h`**
(re-exported from `content/public/types.h`).

`GisContents` = capability host (viewport + owned `GisDocument` adapter).
`GisDocument` = narrow layer/feature edit API. `GisScene` stays on
`BrowserSession`; do not merge with `gis::MapEditSession`.

---

**最后更新：** 2026-10-07
