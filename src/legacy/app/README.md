<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/legacy/app` ? leftover MFC shell

MFC `SmartGis.exe` and `app_core` DLL (`dll_stem=app_core`). Physically under `src/legacy/app/` so `src/app/` only holds endgame/prototype hosts (`views/`, `winui/`).

| Item | Value |
| --- | --- |
| GN | `//src/legacy/app:app` ? `SmartGis.exe` |
| Core DLL | `//src/legacy/app:app_core` (`dll_stem=app_core`, source `core/smtapp.cpp`) |
| Include prefix | `"legacy/app/<module>/?"` ? e.g. `"legacy/app/core/smtapp.h"`, `"legacy/app/shell/frame/app.h"`, `"legacy/app/doc/smart_gis_doc.h"`, `"legacy/app/view/edit/edit.h"`; PCH stays `"legacy/app/stdafx.h"` |
| Gate | `smt_build_app` / `build.bat legacy_app`??? `build.bat app` ? Views? |
| Default `src_all` | **no** |
| Frozen labels | `:app` / `:app_core` / `:legacy_app_all`?`APP_CORE_EXPORT` / pragma `app_core(_d).lib` |

## Layout (approach C ? capability subdirs)

```
src/legacy/app/
  BUILD.gn  README.md  stdafx.*  resource.h  smart_gis.rc
  core/     smtapp.*                         ? app_core DLL
  shell/
    frame/    app.* main.* child.*           (CSmartGisApp / CMainFrame / CChildFrame)
    dock/     console.* render_trace.*       (MFC diagnostic docks)
    showcase/ host.h map2d.* scene3d.*       (headless --*-showcase)
  doc/      smart_gis_doc.*
  view/
    map/map.*  edit/edit.*  datasource/datasource.*  scene3d/scene3d_view.*
  res/      icons / cursors / rc2
```

Break includes (tool-style): headers moved with implementation; **no** shim at old paths.
Note: stems under `view/` are capability-named (not `view.*`) so MSVC does not collide on `view.obj`; `scene3d_view` avoids clash with `shell/showcase/scene3d`.

As-built: this README + [`docs/build/src-layout.md`](../../docs/build/src-layout.md). Prior scheme C land: [`archive/specs/2026-09-27-legacy-app-subdirectory-layout-design.md`](../../docs/superpowers/archive/specs/2026-09-27-legacy-app-subdirectory-layout-design.md).

## SP3 strangler (host behavior)

| Concern | Landing |
| --- | --- |
| Attribute / Catalog JSON | Already `content::feature_attrs` / `catalog_layers` + `content::MapScene` |
| Sample / china path policy | `content::resolve_sample_map_candidates` / `try_resolve_existing_sample_map` ? `SmtApp` calls them |
| GDAL open + leftover mapmgr / style | Still orchestrated in `core/smtapp.cpp` (leftover singletons) |
| Draft commit HWND-free seam | Deferred ? see `TODO(sp3)` / plan Task 5; viewport math stays in `content::MapScene` |
| MFC view paint / SetOperMap | Thin `view/*` forwarders over `legacy/ui/map` (+ `shell`) |

## China map bootstrap

- ??? `SmtApp::DelayInit` / `InitSmtMap` ???? `out/data/china_city.gpkg`?`area`/`line`/`point`/`text` ???????????? `china_city.geojson` / `china_plp.geojson`??? `name`/`adcode` ???????? YaHei + UTF-8 `TextOutW`?
- `--self-test`????? ?1 ??? ?3?? `china-plp-ok`?Edit ?????? BCG ???? `CSmartMapEditView::OnCreate` ??? `TerminateProcess(0)`?
- `--map2d-showcase china`：绕过 MDI，GDI headless 中国样例 → `legacy-map2d-showcase-china.bmp` + perf/trace；loop：`legacy.map2d.china`。可选 `SMT_MAP2D_SHOWCASE_LINGER_MS`（ms）供 forensic OS inject / 录像（`legacy.browse.2d`）。
- `--scene3d-showcase china`：leftover GL/D3D stereo DEM + BMP；loop：`legacy.scene3d.china`。可选 `SMT_SCENE3D_SHOWCASE_LINGER_MS`（`legacy.browse.3d`）。
- **Browse forensic：** `legacy.browse.2d` / `legacy.browse.3d` — OS pan/wheel inject + 可选 `SMT_HARNESS_RECORD=1`；见 `docs/build/ui-testing.md` L1′ map browse forensic。
- **启动**：`InitInstance` 先显示主框再 `DelayInit` 加载 `china_city`，然后只 **`PostMessage(ID_WND_MAPEDIT)`** 打开 Edit（Data/3D 从窗口菜单手动开——连发三个 MDI 子窗会在第二个 `CreateNewFrame` 挂死）。`--self-test` 仍在 MDI 前退出。
- **????**?????????? RC ???**??(&W)** ????? `append_mdi_window_menu`????? / ???? / ??????
- **3D**?`Smt3DXView::CreateRender` ? `china_city.gpkg` ?? leftover GL?DEM ??? `gis::land_mask` ? bbox ?????????????`gl_map_paint_test` ???????

Do not add new product features here ? freeze except compile/path fixes. Destination shell is `src/app/views` + `src/ui/views`.

See: [`docs/superpowers/specs/2026-09-14-app-legacy-split-design.md`](../../docs/superpowers/specs/2026-09-14-app-legacy-split-design.md), [`docs/superpowers/specs/2026-09-19-legacy-host-behavior-extract-design.md`](../../docs/superpowers/specs/2026-09-19-legacy-host-behavior-extract-design.md), [`docs/build/src-layout.md`](../../docs/build/src-layout.md).

---

**?????** 2026-09-30
