<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/legacy/app` — leftover MFC shell

MFC `SmartGis.exe` and `app_core` DLL (`dll_stem=app_core`). Physically under `src/legacy/app/` so `src/app/` only holds endgame/prototype hosts (`views/`, `winui/`).

| Item | Value |
| --- | --- |
| GN | `//src/legacy/app:app` → `SmartGis.exe` |
| Core DLL | `//src/legacy/app:app_core` (`dll_stem=app_core`, sources `bootstrap/bootstrap.cpp` + `bootstrap/sample_map.cpp`) |
| Include prefix | `"legacy/app/<role>/…"` — e.g. `"legacy/app/bootstrap/bootstrap.h"`, `"legacy/app/shell/frame/win_app.h"`, `"legacy/app/views/document/document.h"`, `"legacy/app/views/viewport/edit_view.h"`; PCH stays `"legacy/app/stdafx.h"` |
| Gate | `smt_build_app` / `build.bat legacy_app`（`build.bat app` → Views） |
| Default `src_all` | **no** |
| Frozen labels | `:app` / `:app_core` / `:legacy_app_all`；`APP_CORE_EXPORT` / pragma `app_core(_d).lib` |

## Layout (§11g — `shell/` + `views/` by responsibility)

```
src/legacy/app/
  BUILD.gn  README.md  stdafx.*  resource.h  smart_gis.rc  app_export.h
  bootstrap/             # SmtApp + sample_map (app_core DLL; was core/)
    bootstrap.* sample_map.*
  shell/
    frame/               # CWinApp + MDI frames + MDI tab options
    catalog/             # CatalogTabDockPane
    dock/                # Diagnostic strip: debug_console · render_trace · diagnostic · dock_child
    showcase/            # headless HWND + BMP: showcase_host · map2d_showcase · scene3d_showcase
  views/
    document/            # CSmartGisDoc
    viewport/            # thin CView: edit_view · data_view · scene3d_view
    helper/              # mdi_menu · status_coord · self_test_mark (`legacy_app::helper`)
  res/                   # icons / cursors / rc2
```

Break includes (scheme C): headers moved with implementation; **no** shim at old flat `shell/*.h` or `views/*.h`.
`scene3d_view` stem stays distinct from `scene3d_showcase` (MSVC `.obj` clash-free).
`*_view` stems kept (class ABI frozen).
Dead stub `view/map` (`CSmartGisView`) deleted — unused by doc templates.

As-built: this README + [`docs/superpowers/src-layout.md`](../../docs/superpowers/src-layout.md). Living lock: [`docs/superpowers/specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md`](../../docs/superpowers/specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md) §11g. Diagram: [`docs/superpowers/diagrams/legacy-app-deep-layer.html`](../../docs/superpowers/diagrams/legacy-app-deep-layer.html).

## SP3 strangler (host behavior)

| Concern | Landing |
| --- | --- |
| Attribute / Catalog JSON | Already `content::feature_attrs` / `catalog_layers` + `content::MapScene` |
| Sample / china path policy | `content::resolve_sample_map_candidates` / `try_resolve_existing_sample_map` — `sample_map` / `SmtApp` calls them |
| GDAL open + leftover mapmgr / style | Still orchestrated in `bootstrap/bootstrap.cpp` (leftover singletons) |
| Draft commit HWND-free seam | Deferred — see `TODO(sp3)` / plan Task 5; viewport math stays in `content::MapScene` |
| MFC view paint / SetOperMap | Thin `views/viewport/*` forwarders over `legacy/ui/map` (+ `views/helper`) |

## China map bootstrap

- `SmtApp::DelayInit` / `InitSmtMap` load `out/data/china_city.gpkg` (area/line/point/text); fallbacks `china_city.geojson` / `china_plp.geojson`; YaHei + UTF-8 `TextOutW`.
- `--self-test`: exit before MDI; mark file + Edit create watchdog via `views/helper/self_test_mark`.
- `--map2d-showcase china` / `--scene3d-showcase <mode>`: headless BMP; optional linger env.
- Browse forensic: `legacy.browse.2d` / `legacy.browse.3d`.
- Startup: show main frame → DelayInit → PostMessage Edit only.

Do not add new product features here — freeze except compile/path fixes. Destination shell is `src/app/views` + `src/ui/views`.

---

**最后更新:** 2026-10-04