<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/` layout (layered)

Product sources stay under **`src/`** (not repo-root `base/` / `core/` — those are thin GN aliases). Directory names drop the 2010 `Smt` prefix. This pass **nests by layer** after the short-name rename. It does **not** invent a new C++ API. Algorithm modernization (2026-09-13) **does** merge math + math3d + geo3d into one `SmtGeoCore` DLL; other layers stay one GN target per former DLL.

**Nesting cap:** at most `src/<layer>/<module>` (two levels under `src/`).

**This pass:** directory + `BUILD.gn` label + include-dir paths. C++ `Smt_*` namespaces, `Export_Smt*` macros, and on-disk DLL stems stay for ABI / `LoadLibrary`. New public namespaces in later work stay at most two levels (`core`, `core::math`); deeper goes in `detail`.

GN **target** names stay the short names from the rename pass (`sde_smf`, `render_gl`, …) so `dll_stem` identity stays obvious. **Label paths** follow the tree (`//src/sdb/datasource/smf:sde_smf`).

Debug/release DLL file names still use `dll_stem` (legacy `Smt*` + optional `D`).

## Five layers (locked)

| Layer | Tree | Notes |
| --- | --- | --- |
| app | `src/app/` + `app/{app_core,views,webview2,winui}` | All product hosts. No `src/chrome/`. Namespace `app`. |
| content | `src/content/public` | Stable embedder API. Hosts do not include sdb / render devices. |
| sdb | `src/sdb/{feature,layer,map,crs,datasource/<driver>,model,scene}` | GIS model; CPU assets (`model`) and World (`scene`). |
| render | `src/render/` + `render/rhi` + `render/scene` | Unified 2D+3D RHI (FlyCube DX12/Vulkan). GPU instance cache in `render/scene`. Leftover `scene3d`/`model3d` stay. **Paint runs in `--type=gpu`**, not in browser. |
| base | `src/base/` + `base/ipc` (planned) | Merged former `core` + envelope/style. Two DLLs: `:core` + `:base`. IPC facade for Mojo invitation. |

## OSS GIS ↔ this tree

| QGIS / GDAL / GEOS / PROJ | This repo |
| --- | --- |
| `providers/*`, OGR drivers | `src/sdb/datasource/{mem,smf,ws,gdal}` |
| `QgsFeature` / `QgsMapLayer` / `QgsProject` | `src/sdb/feature`, `layer`, `map` |
| `QgsCoordinateReferenceSystem` | `src/sdb/crs`; transforms in `algorithm/proj` |
| GEOS | `src/algorithm/geo` (`//src/algorithm:geom` → `SmtGeoCore`) — wrap `gdal_sdk` `geos_c`; no second GEOS vendor |
| PROJ | `src/algorithm/proj` (PROJ 9 adapter only) |
| map canvas renderer | `src/render` + RHI |
| processing / analysis | `src/algorithm/` |
| `qgis_gui` / `qgis_app` | `src/ui/` / `src/app/` |
| libqgis_core for embedders | `src/content/public` |
| WMS/WFS | leftover `src/web/` (not `sdb/map`) |
| mapd HTTP | `src/web/mapd` (`web::MapdClient`) |

`sdb/datasource/gdal` is `SmtSDEGdalDevice`: registers GDAL driver `"SDBD"` plus file/DB/Memory via the same `gdal_sdk`. Product layer types are `GDALDataset` / `OGRLayer` / `OGRFeature`. ADO sources are removed.

## Layering plan (this pass)

| Layer | Tree | Merge vs nest | In `src_all` |
| --- | --- | --- | --- |
| Foundation | `base` (core+style), `sys`, `net` | One `src/base/` dir; two DLLs (`SmtCore` / `SmtBaseLib`). | yes |
| Core data model | `sdb/{feature,layer,map,model,scene}` | `SmtGisCore` DLL plus source_sets `sdb/model` (Assimp/3D Tiles CPU) and `sdb/scene` (World). | yes (`gis` + model + scene) |
| Datasource | `sdb/datasource/{mgr,gdal,mem,smf,ws}` | Provider drivers. DB path is OGR (`sde_gdal`). | yes (`//src/sdb:datasource`) |
| Algorithm | `algorithm/{geo,proj,tin,baogrid,stat}` | `geo` is one DLL (`SmtGeoCore`) compiling geo + math + math3d + geo3d sources; old math/math3d/geo3d labels are groups. `proj` / `tin` stay their DLLs. **Not** dem (plugin + GDAL + tin). **Not** chart (`ui/chart`). | yes (`//src/algorithm:algorithm`; not `chart`) |
| Render | `render/` + children | RHI Facade + GPU scene (`render/scene`) + leftover 3D engines (`render3d`, `scene3d`, `model3d`, `terrain`, `pointcloud`). `d3d` unwired. `skia` opt-in stub. | yes (`render_all`, not `d3d` / not `skia`) |
| Web GIS | `web/{mapd,service,server,client,server_mgr,server_dev,cgi,…}` | `mapd` is the HTTP client (`:8020`). Leftover WMS stack is former `src/map`. **Not** the map document. | `mapd_client` **yes**; leftover servers **no** (xcatalog / MFC) |
| Plugin | `plugin/` + children | Host stays; domain modules nest as children. | host only |
| UI | `ui/{gui,mfc_ex,xview,xcatalog,xambox,chart}` | Nested only. **Legacy** MFC Feature Pack chrome (`bcg_cmfc.h`). `ui/chart` is the MFC modal diagram (`SmtStaDiagram`); data stays in `algorithm/stat`. | **no** (MFC; gated by `smt_build_app`) |
| UI toolkit (endgame) | `ui/views` | Chromium-style Views stub (`//:ui_views`). | **no** |
| Hosted map (mgis `content`) | `content/public` + `content/app` | Stable embedder API (`MapContents` / `MapWidgetHostView`, rename from `MapSession` / `MapView`). `content::ContentMain` in `src/content/app` dispatches `--type=`. | **yes** (`content` source_set, not a DLL) |
| GPU main (`--type=gpu`) | `gpu/` | Entry `GpuMain` in the **same** `SmartGis.exe` PE — not a separate product render exe. Today `//src/gpu:gpu` / `build.bat render` is transitional. | **no** (linked into product exe) |
| App | `app/` + `app/{app_core,views,webview2,winui}` | MFC exe + product hosts (no `src/chrome/`). `wWinMain` → `content::ContentMain`. | **no** |
| Tool | `tool/` + `tool/group` + `sdb/edit` | Leftover `SmtIATool` DLLs plus session-scoped Command / Input / EventBus (`//src/tool:dispatch`). Document writes go through `sdb/edit`. | `tool` + `dispatch` (`group` needs UI) |

**Deliberately not merged**

- `//src/base:core` + `//src/base:base`: two DLLs in one directory; `sys` stays beside base.
- `math` + `math3d` + `geo3d` **sources** stay in those directories (include paths / nesting cap). They **link** as `SmtGeoCore` (`//src/algorithm/geo:geo`). OGC `coordinateDimension` is 2 or 3 on the Geometry **instance**; compile-time dim stays on Vector2/3/4. See [`../superpowers/specs/2026-09-13-algorithm-layer-oss-design.md`](../superpowers/specs/2026-09-13-algorithm-layer-oss-design.md).
- `base`: keep the name. It is `SmtBaseLib` (envelope/style), not a Chromium-style foundation.
- No `Smt_*` renames. New algorithm traits (`geo::geometry_traits` / `vector_traits`) are additive; they do not replace the virtual ABI.
- MFC Feature Pack / D3D targets stay out of `src_all`. This GN’s ninja `all` lists every **loaded** target, so the exe graph is gated by `smt_build_app` (default false). `build.bat app` sets it and builds `//:smartgis` → `out/SmartGis.exe`.

### Desktop UI endgame (Views + Skia)

Chosen destination: Chromium-style **Views** + **Skia** + existing C++ map viewport. Tree: `src/ui/views`, `src/render/skia`. Doc: [`ui-views-skia.md`](ui-views-skia.md). Feature Pack, WinUI, and WebView2 are **not** the endgame. Qt is banned.

### MFC Feature Pack (legacy exe bootstrap)

`build.bat app` links **MFC Feature Pack** (`CMFCRibbonBar` / `CDockablePane` / `CMDIFrameWndEx` via `src/ui/mfc_ex/bcg_cmfc.h`). BCGControlBar Pro is **not** required and is not vendored. This is a compile bridge, **not** the destination toolkit (Views + Skia).

| Dep | How to satisfy |
| --- | --- |
| **MFC** (MBCS, `afxwin.h` / `afxres.h` / `afxcontrolbars.h`) | VS 18 Individual component **C++ MFC for x64/x86 (Latest MSVC)** = `Microsoft.VisualStudio.Component.VC.ATLMFC`, or toolset-pinned `Microsoft.VisualStudio.Component.VC.14.50.18.0.MFC`. After install, re-run `build.bat app` so `out/environment.x64.x64` picks up `atlmfc\include` + `atlmfc\lib\x64`. Close `cl`/`ninja`/`link` first, or the installer precheck `VSProcessesRunning` cancels (error `0x1f46`). |

## Path map (2010 dir → short name → layered)

| Old directory | Short (`src/…`) | Layered (`src/…`) | GN target | DLL stem (unchanged) |
| --- | --- | --- | --- | --- |
| `SmtCore` | `core` | `base` | `core` (`//src/base:core`) | `SmtCore` |
| `SmtSysCore` | `sys` | `sys` | `sys` | `SmtSysCore` |
| `SmtMathLib` | `math` | `algorithm/math` | `math` (group → geo) | *(absorbed)* `SmtGeoCore` |
| `Smt3DMathLib` | `math3d` | `algorithm/math3d` | `math3d` (group → geo) | *(absorbed)* `SmtGeoCore` |
| `SmtBaseLib` | `base` | `base` | `base` | `SmtBaseLib` |
| `SmtGeoCore` | `geo` | `algorithm/geo` | `geo` | `SmtGeoCore` |
| `Smt3DGeoCore` | `geo3d` | `algorithm/geo3d` | `geo3d` (group → geo) | *(absorbed)* `SmtGeoCore` |
| `SmtGisCore` | `gis` | `sdb/{feature,layer,map}` | `gis` (`//src/sdb/map:gis`) | `SmtGisCore` |
| `SmtGisPrj` | `proj` | `algorithm/proj` | `proj` | `SmtGisPrj` |
| `SmtRender` | `render` | `render` | `render` | `SmtRender` |
| `Smt3DRenderer` | `render3d` | `render/render3d` | `render3d` | `Smt3DRenderer` |
| `SmtGdiRenderDevice` | `render_gdi` | `render/gdi` | `render_gdi` | `SmtGdiRenderDevice` |
| `SmtGdiSimpleRenderDevice` | `render_gdi_simple` | `render/gdi_simple` | `render_gdi_simple` | `SmtGdiSimpleRenderDevice` |
| `SmtGLRenderDevice` | `render_gl` | `render/gl` | `render_gl` | `SmtGLRenderDevice` |
| `SmtD3DRenderDevice` | `render_d3d` | `render/d3d` | — | `SmtD3DRenderDevice` |
| `SmtSDEDeviceMgr` | `sde_mgr` | `sdb/datasource/mgr` | `sde_mgr` | `SmtSDEDeviceMgr` |
| `SmtSDEGdalDevice` | `sde_gdal` | `sdb/datasource/gdal` | `sde_gdal` | `SmtSDEGdalDevice` |
| `SmtSDEMemDevice` | `sde_mem` | `sdb/datasource/mem` | `sde_mem` | `SmtSDEMemDevice` |
| `SmtSDESmfDevice` | `sde_smf` | `sdb/datasource/smf` | `sde_smf` | `SmtSDESmfDevice` |
| `SmtSDEWSDevice` | `sde_ws` | `sdb/datasource/ws` | `sde_ws` | `SmtSDEWSDevice` |
| `SmtMapService` | `map_service` | `web/service` | `map_service` | `SmtMapService` |
| `SmtMapServer` | `map_server` | `web/server` | `map_server` | `SmtMapServer` |
| `SmtMapClient` | `map_client` | `web/client` | `map_client` | `SmtMapClient` |
| `SmtMapServerDeviceMgr` | `map_server_mgr` | `web/server_mgr` | `map_server_mgr` | `SmtMapServerDeviceMgr` |
| `SmtMapServerDevice111` | `map_server_dev` | `web/server_dev` | `map_server_dev` | `SmtMapServerDevice111` |
| `SmtConsoleMapServer` | `map_server_console` | `web/server_console` | — | — |
| `SmtWinServiceMapServer` | `map_server_winsvc` | `web/server_winsvc` | — | — |
| `SmtCgiWrapper` | `cgi` | `web/cgi` | — | — |
| — | `mapd_client` | `web/mapd` | `mapd_client` | — (source_set) |
| `SmtToolCore` | `tool` | `tool` | `tool` | `SmtToolCore` |
| `SmtGroupToolCore` | `tool_group` | `tool/group` | `tool_group` | `SmtGroupToolCore` |
| — | `dispatch` | `tool` (`command` / `interaction` / `workspace`) | `dispatch` | — (source_set) |
| — | `edit` | `sdb/edit` | `edit` | — (source_set) |
| `SmtGuiCore` | `gui` | `ui/gui` | `gui` | `SmtGuiCore` |
| `SmtMFCExCore` | `mfc_ex` | `ui/mfc_ex` | `mfc_ex` | `SmtMFCExCore` |
| `SmtXViewCore` | `xview` | `ui/xview` | `xview` | `SmtXViewCore` |
| `SmtXCatalogCore` | `xcatalog` | `ui/xcatalog` | `xcatalog` | `SmtXCatalogCore` |
| `SmtXAMBoxCore` | `xambox` | `ui/xambox` | `xambox` | `SmtXAMBoxCore` |
| — | `views` | `ui/views` | `views` (`//:ui_views`) | — (source_set stub) |
| — | `skia` | `render/skia` | `skia` (`//:ui_views`) | — (source_set stub; no Skia tree) |
| `SmtAuxModule` | `plugin` | `plugin` | `plugin` | `SmtAuxModule` |
| `SmtAM3DModelCreater` | `plugin_model3d` | `plugin/model3d` | `plugin_model3d` | `SmtAM3DModelCreater` |
| `SmtAMBAOGridCreater` | `plugin_baogrid` | `plugin/baogrid` | `plugin_baogrid` | `SmtAMBAOGridCreater` |
| `SmtAMDemCreater` | `plugin_dem` | `plugin/dem` | `plugin_dem` | `SmtAMDemCreater` |
| `SmtAMMapPrint` | `plugin_print` | `plugin/print` | `plugin_print` | `SmtAMMapPrint` |
| `SmtAMMapProject` | `plugin_proj` | `plugin/proj` | `plugin_proj` | `SmtAMMapProject` |
| `SmtAMMapServiceMgr` | `plugin_map_service` | `plugin/map_service` | `plugin_map_service` | `SmtAMMapServiceMgr` |
| `SmartGis` | `app` | `app` | `app` | `SmartGis.exe` (`build.bat app`) |
| `SmtAppCore` | `app_core` | `app/app_core` | `app_core` | `SmtAppCore` |
| — | `views` (exe) | `app/views` | `views` (`//:ui_views`) | `SmartGisViews.exe` |
| — | `webview2` | `app/webview2` | `app_webview2` | `SmartGisWeb.exe` |
| — | `winui` | `app/winui` | `app_winui` | `SmartGisWinui.exe` |
| `SmtTinMesh` | `tin` | `algorithm/tin` | `tin` | `SmtTinMesh` |
| `SmtBAOrthGrid` | `baogrid` | `algorithm/baogrid` | `baogrid` | `SmtBAOrthGrid` |
| `Smt3DBaseLib` | `scene3d` | `render/scene3d` | `scene3d` | `Smt3DBaseLib` (leftover) |
| — | `scene` | `render/scene` | `scene` | GPU cache (source_set) |
| — | `model` / `scene` | `sdb/model`, `sdb/scene` | `model` / `scene` | CPU assets + World (source_sets) |
| `Smt3DMdLib` | `model3d` | `render/model3d` | `model3d` | `Smt3DMdLib` (leftover) |
| `Smt3DPointCloud` | `pointcloud` | `render/pointcloud` | `pointcloud` | `Smt3DPointCloud` |
| `Smt3DTerrain` | `terrain` | `render/terrain` | `terrain` | `Smt3DTerrain` |
| `SmtNetCore` | `net` | `net` | `net` | `SmtNetCore` |
| `SmtStaCore` | `stat` | `algorithm/stat` | `stat` | `SmtStaCore` |
| `SmtStaDiagram` | `stat_chart` | `ui/chart` | `stat_chart` | `SmtStaDiagram` |

`—` = no `BUILD.gn` yet (do not drop already-wired `src_all` deps). `app` is `//src/app:app` → `out/SmartGis.exe`; keep it out of `src_all` so the daily `build.bat` DLL set stays green without loading the MFC graph. After algorithm modernization, `SmtMathLib` / `Smt3DMathLib` / `Smt3DGeoCore` / `SmtDemCore` are not separate outputs (`SmtDemCore` is gone; heightmap I/O is `plugin/dem` + GDAL).

Repo-root `//core:core_all` still aliases `//src:src_all`. It is not the product tree.

Include dirs in `//build:smt_legacy` still point at **each module root** (quoted `#include "header.h"`), not at layer parents.

## File naming (mgis / Chromium)

| Tree | Stem | Extension | Include |
| --- | --- | --- | --- |
| New (`content`, `gpu`, `app/{views,webview2,winui}`, `ui/views`, `render/{skia,rhi,scene}`, `sdb/{model,scene}`) | `snake_case` | `.cc` / `.h` | `"content/public/map_view.h"`, `"ui/views/view.h"`, `"gpu/gpu.h"`, `"render/rhi/rhi.h"`, `"sdb/scene/scene.h"` (`//src` on the include path) |
| Legacy product (`app` MFC, `ui/{gui,mfc_ex,xview,…}`, `plugin/*`, `net`, …) | `snake_case` | keep `.cpp` | still module-root `"main_frame.h"` / `"grid_ctrl.h"` |

- Drop file prefixes (`smt_`, `vw_`, `cata_`, `baog_`, `msvr_`, `am_`, `gt_`, `wa_`, `bl_`, `rd_`, plus module tags `gis_` / `geo_` / `sde_`). **DLL stems**, `Export_Smt*`, and `Smt_*` namespaces stay.
- CRT collisions keep a short qualifier (`core_assert.h`, `net_string.h`), not the old prefix.
- `stdafx` / `targetver` / `resource.h` keep those conventional names.

---

**最后更新：** 2026-09-13
