<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/legacy/app` — leftover MFC shell

MFC `SmartGis.exe` and `app_core` DLL (`dll_stem=app_core`). Physically under `src/legacy/app/` so `src/app/` only holds endgame/prototype hosts (`views/`, `winui/`).

| Item | Value |
| --- | --- |
| GN | `//src/legacy/app:app` → `SmartGis.exe` |
| Core DLL | `//src/legacy/app:app_core` (`dll_stem=app_core`, sources `core/bootstrap.cpp` + `core/sample_map.cpp`) |
| Include prefix | `"legacy/app/<role>/…"` — e.g. `"legacy/app/core/bootstrap.h"`, `"legacy/app/shell/frame/win_app.h"`, `"legacy/app/doc/document.h"`, `"legacy/app/view/edit_view.h"`; PCH stays `"legacy/app/stdafx.h"` |
| Gate | `smt_build_app` / `build.bat legacy_app`（`build.bat app` → Views） |
| Default `src_all` | **no** |
| Frozen labels | `:app` / `:app_core` / `:legacy_app_all`；`APP_CORE_EXPORT` / pragma `app_core(_d).lib` |

## Layout (§11g deep layer — role stems + bind compose)

```
src/legacy/app/
  BUILD.gn  README.md  stdafx.*  resource.h  smart_gis.rc  app_export.h
  core/
    bootstrap.*          # SmtApp (app_core DLL)
    sample_map.*         # china / sample GeoJSON helpers
  shell/
    frame/    win_app.* main_frame.* child_frame.* mdi_tabs.*
    catalog/  tab_pane.*
    dock/     console.* render_trace.* diagnostic.* pane_host.h
    showcase/ host.h map2d.* scene3d.*
  doc/        document.*
  view/
    bind/     mdi_menu.* status_coord.* self_test_mark.*
    edit_view.* data_view.* scene3d_view.*
  res/        icons / cursors / rc2
```

Break includes (scheme C): headers moved with implementation; **no** shim at old paths.
`scene3d_view` stem avoids MSVC `scene3d.obj` clash with `shell/showcase/scene3d`.
Dead stub `view/map` (`CSmartGisView`) deleted — unused by doc templates.

As-built: this README + [`docs/build/src-layout.md`](../../docs/build/src-layout.md). Living lock: [`docs/superpowers/specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md`](../../docs/superpowers/specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md) §11g. Diagram: [`docs/superpowers/diagrams/legacy-app-deep-layer.html`](../../docs/superpowers/diagrams/legacy-app-deep-layer.html).

## SP3 strangler (host behavior)

| Concern | Landing |
| --- | --- |
| Attribute / Catalog JSON | Already `content::feature_attrs` / `catalog_layers` + `content::MapScene` |
| Sample / china path policy | `content::resolve_sample_map_candidates` / `try_resolve_existing_sample_map` — `sample_map` / `SmtApp` calls them |
| GDAL open + leftover mapmgr / style | Still orchestrated in `core/bootstrap.cpp` (leftover singletons) |
| Draft commit HWND-free seam | Deferred — see `TODO(sp3)` / plan Task 5; viewport math stays in `content::MapScene` |
| MFC view paint / SetOperMap | Thin `view/*` forwarders over `legacy/ui/map` (+ `view/bind`) |

## China map bootstrap

- `SmtApp::DelayInit` / `InitSmtMap` load `out/data/china_city.gpkg` (area/line/point/text); fallbacks `china_city.geojson` / `china_plp.geojson`; YaHei + UTF-8 `TextOutW`.
- `--self-test`: exit before MDI; mark file + Edit create watchdog via `view/bind/self_test_mark`.
- `--map2d-showcase china` / `--scene3d-showcase <mode>`: headless BMP; optional linger env.
- Browse forensic: `legacy.browse.2d` / `legacy.browse.3d`.
- Startup: show main frame → DelayInit → PostMessage Edit only.

Do not add new product features here — freeze except compile/path fixes. Destination shell is `src/app/views` + `src/ui/views`.

---

**最后更新:** 2026-10-02
