<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `legacy/app` deep layer — Implementation Plan

**Status:** landed (archived 2026-10-03 — checkboxes complete)

> **Living lock:** [`../../specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md`](../../specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md) §11g.  
> **Diagram:** [`../../diagrams/legacy-app-deep-layer.html`](../../diagrams/legacy-app-deep-layer.html)

**Goal:** Role stems + composition helpers under `legacy/app`; scheme C; freeze MFC class ABI / `dll_stem=app_core` / `SmartGis.exe`.

**Done when:** tree matches §11g; `build.bat debug legacy_app` green.

## Tasks

- [x] Rename/move stems (`smtapp`→`bootstrap`, `app`→`win_app`, `main`→`main_frame`, `child`→`child_frame`, `smart_gis_doc`→`document`, flatten `view/*` → `edit_view` / `data_view` / `scene3d_view`)
- [x] Delete dead `view/map` (`CSmartGisView`)
- [x] `core/sample_map` — extract sample GeoJSON / path helpers from bootstrap
- [x] `shell/catalog/tab_pane` — extract `CatalogTabDockPane` from main_frame
- [x] `shell/frame/mdi_tabs` — extract `CMDITabOptions`
- [x] `view/bind/{mdi_menu,status_coord,self_test_mark}` — share across Edit/Data/3D
- [x] `shell/dock` — keep role stems `console` / `render_trace` / `diagnostic` (+ optional `pane_host`)
- [x] `BUILD.gn` + include sweep; README + `docs/superpowers/src-layout.md` + umbrella §11g
- [x] `build.bat debug legacy_app` green (`SmartGis.exe` + `app_core_d.dll` 2026-10-02 23:19)
- [x] 2026-10-03 top-level tighten: `core`→`bootstrap`; flatten `shell/*`; fold `doc`→`views`; `view`→`views` + `bind`→`helper`; `host`→`showcase_host`; `legacy_app` green again
- [x] 2026-10-03 shell stem rename: `tab_pane`→`catalog_pane`, `pane_host`→`dock_child`, `console`→`debug_console`, `mdi_tabs`→`mdi_tab_options`, `map2d`/`scene3d`→`*_showcase`
- [x] 2026-10-03 views flatten: fold `views/helper/{mdi_menu,status_coord,self_test_mark}` into flat `views/`; keep `*_view` + `document` stems; `legacy_app::helper` namespace retained
