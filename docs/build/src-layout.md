<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/` layout (layered)

> **In progress:** full include-path + ABI cutover (mogu-style `#include`, new DLL stems, no `Smt*` export ABI). Spec: [`../superpowers/specs/2026-09-13-code-style-include-abi-cutover-design.md`](../superpowers/specs/2026-09-13-code-style-include-abi-cutover-design.md). Map: [`abi-rename-map.md`](abi-rename-map.md).

Product sources stay under **`src/`** (not repo-root `base/` / `core/` — those are thin GN aliases). Directory names drop the 2010 `Smt` prefix. This pass **nests by layer** after the short-name rename. Algorithm modernization (2026-09-13) **does** merge math + math3d + geo3d into one `geo` DLL (`SmtGeoCore` stem retired by cutover); other layers stay one GN target per former DLL.

**Nesting cap:** at most `src/<layer>/<module>` (two levels under `src/`).

**This pass:** directory + `BUILD.gn` label + include-dir paths. C++ `Smt_*` namespaces, `Export_Smt*` macros, and on-disk DLL stems stay for ABI / `LoadLibrary`. New public namespaces in later work stay at most two levels (`core`, `core::math`); deeper goes in `detail`.

GN **target** names stay the short names from the rename pass (`sde_smf`, `render_gl`, …) so `dll_stem` identity stays obvious. **Label paths** follow the tree (`//src/sdb/datasource/smf:sde_smf`).

Debug/release DLL file names still use `dll_stem` (legacy `Smt*` + optional `D`).

## Five layers (locked)

| Layer | Tree | Notes |
| --- | --- | --- |
| app | `src/app/` + `app/{app_core,views,winui}` | All product hosts. No `src/chrome/`. Namespace `app`. |
| content | `src/content/public` | Stable embedder API. Hosts do not include sdb / render devices. |
| sdb | `src/sdb/{feature,layer,map,crs,datasource/<driver>,model,scene}` | GIS model; CPU assets (`model`) and World (`scene`). |
| render | `src/render/` + `render/rhi` + `render/scene` | Unified 2D+3D RHI (FlyCube DX12/Vulkan). GPU instance cache in `render/scene`. Leftover `scene3d`/`model3d` stay. **Paint runs in `--type=gpu`**, not in browser. |
| base | `src/base/{core,style,ipc}` | `core` = `SmtCore`. `style` = `SmtBaseLib` (cartographic pen/brush/symbol + Envelope; not Views/CSS). `ipc` = named pipe + pickle (not a dep of SmtCore). |

## OSS GIS ↔ this tree

| QGIS / GDAL / GEOS / PROJ | This repo |
| --- | --- |
| `providers/*`, OGR drivers | `src/sdb/datasource/{mem,smf,ws,gdal}` |
| `QgsFeature` / `QgsMapLayer` / `QgsProject` | `src/sdb/feature`, `layer`, `map` |
| `QgsCoordinateReferenceSystem` | `src/sdb/crs`; transforms in `algorithm/proj` |
| GEOS | `src/algorithm/geo` (`//src/algorithm:geom` → `SmtGeoCore`) — wrap `//third_party:gdal` (`.install` `geos_c`); no second GEOS vendor |
| PROJ | `src/algorithm/proj` (PROJ 9 adapter only) |
| map canvas renderer | `src/render` + RHI |
| processing / analysis | `src/algorithm/` |
| `qgis_gui` / `qgis_app` | `src/ui/` / `src/app/` |
| libqgis_core for embedders | `src/content/public` |

`sdb/datasource/gdal` is `SmtSDEGdalDevice`: a decorator `GDALDriver` `"SDBD"` whose `SdbdDataset` owns a stock inner `GDALDataset` (Memory / GPKG / PostgreSQL / file). Callers use `GDALOpenEx("SDBD:…")` and may `dynamic_cast` to `SdbdDataset` / `SdbdLayer`. Do not patch `third_party/.src/gdal` or resurrect `OgrDataSource`. Product types are `GDALDataset` / `OGRLayer` / `OGRFeature`.

## Layering plan (this pass)

| Layer | Tree | Merge vs nest | In `src_all` |
| --- | --- | --- | --- |
| Foundation | `base/{core,style,ipc}`, `sys`, `net` | One layer dir; two DLLs (`SmtCore` / `SmtBaseLib`) plus `ipc` source_set. | yes |
| Core data model | `sdb/{feature,layer,map,model,scene}` | `SmtGisCore` DLL plus source_sets `sdb/model` (Assimp/3D Tiles CPU) and `sdb/scene` (World). | yes (`gis` + model + scene) |
| Datasource | `sdb/datasource/{mgr,gdal,mem}` | Provider drivers. Product types are `GDALDataset` / `OGRLayer` / `OGRFeature`. SMF/WS leftovers stay on disk, not in `src_all`. | yes (`//src/sdb:datasource`) |
| Algorithm | `algorithm/{geo,proj,tin,stat}` | `geo` is one DLL (`SmtGeoCore`): TIN/grid/surface meshes. OGC types are OGR (callers include `ogr_geometry.h`). Scene Vector/Matrix/Aabb live in `src/render/math` (Eigen). Public headers: `geometry.h` (Grid / Tin / `Smt3DSurface` using OGR TIN), `projection.h`, `tin.h`. `proj` / `tin` stay their DLLs. **Not** dem (plugin + GDAL + tin). **Not** orthogrid (plugin + Eigen Laplace). **Not** chart (`ui/chart`). | yes (`//src/algorithm:algorithm`; not `chart`) |
| Render | `render/` + children | RHI Facade + GPU scene (`render/scene`) + leftover 3D engines (`render3d`, `scene3d`, `model3d`, `terrain`, `pointcloud`). D3D9 tree removed. `skia` opt-in stub. | yes (`render_all`, not `skia`) |
| Plugin | `plugin/` + children | Host `//src/plugin:host` (source_set, not a new DLL): Registry / store / Python / processing. Domain children keep leftover `dll_stem`. Boundary-adaptive orthogonal grid (Eigen Laplace) lives in `plugin/orthogrid`, not `algorithm/`. Shared preview in `plugin/widgets`. Embed in `plugin/python`. | host + widgets |
| UI | `ui/{gui,mfc_ex,xview,xcatalog,xambox,chart}` | Nested only. **Legacy** MFC Feature Pack chrome (`bcg_cmfc.h`). `ui/chart` is the MFC modal diagram (`SmtStaDiagram`); data stays in `algorithm/stat`. | **no** (MFC; gated by `smt_build_app`) |
| UI toolkit (endgame) | `ui/views` | Chromium-style Views stub (`//:ui_views`). | **no** |
| Hosted map (mgis `content`) | `content/public` + `content/app` | Stable embedder API (`MapContents` / `MapWidgetHostView`, rename from `MapSession` / `MapView`). `content::ContentMain` in `src/content/app` dispatches `--type=`. | **yes** (`content` source_set, not a DLL) |
| GPU main (`--type=gpu`) | `gpu/` | Entry `GpuMain` in the **same** `SmartGis.exe` PE — not a separate product render exe. Today `//src/gpu:gpu` / `build.bat render` is transitional. | **no** (linked into product exe) |
| App | `app/` + `app/{app_core,views,winui}` | MFC exe + product hosts (no `src/chrome/`). `wWinMain` → `content::ContentMain`. | **no** |
| Tool | `tool/` + `tool/group` + `sdb/edit` | Leftover `SmtIATool` DLLs plus session-scoped Command / Input / EventBus (`//src/tool:dispatch`). Document writes go through `sdb/edit`. | `tool` + `dispatch` (`group` needs UI) |

**Deliberately not merged**

- `//src/base:core` + `//src/base:base`: two DLLs in one directory; `sys` stays beside base.
- Homemade Vector/Matrix were removed from `algorithm/geo`. Leftover names `Vector3` / `Matrix` are Eigen-backed adapters in `src/render/math` (`//src/render/math:math`). Aabb/Obb/Plane/Ray compile as `//src/render/math:bounds`. OGC `coordinateDimension` is 2 or 3 on the Geometry **instance**; compile-time dim stays on Vector2/3/4. See [`../superpowers/specs/2026-09-13-algorithm-layer-oss-design.md`](../superpowers/specs/2026-09-13-algorithm-layer-oss-design.md).
- `base`: keep the name. `style/` is `SmtBaseLib` (envelope + cartographic style), not a Chromium-style foundation and not `ui/views`.
- No `Smt_*` renames. `geo::geometry_traits` / `vector_traits` wrap OGR instance dim and `Vector2/3/4`; `geo::buffer` calls OGR. Delaunay is `algorithm/tin`. No empty GEOS glue and no second geometry tree.
- MFC Feature Pack stays out of `src_all`. This GN’s ninja `all` lists every **loaded** target, so the exe graph is gated by `smt_build_app` (default false). `build.bat app` sets it and builds `//:smartgis` → `out/SmartGis.exe`.

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
| `SmtCore` | `core` | `base/core` | `core` (`//src/base:core`) | `SmtCore` |
| `SmtSysCore` | `sys` | `sys` | `sys` | `SmtSysCore` |
| `SmtMathLib` | `math` | `algorithm/math` | `math` (group → geo) | *(absorbed)* `SmtGeoCore` |
| `Smt3DMathLib` | `math3d` | `algorithm/math3d` | `math3d` (group → geo) | *(absorbed)* `SmtGeoCore` |
| `SmtBaseLib` | `base` | `base/style` | `base` (`//src/base:base`) | `SmtBaseLib` |
| `SmtGeoCore` | `geo` | `algorithm/geo` | `geo` | `SmtGeoCore` |
| `Smt3DGeoCore` | `geo3d` | `algorithm/geo3d` | `geo3d` (group → geo) | *(absorbed)* `SmtGeoCore` |
| `SmtGisCore` | `gis` | `sdb/{feature,layer,map}` | `gis` (`//src/sdb/map:gis`) | `SmtGisCore` |
| `SmtGisPrj` | `proj` | `algorithm/proj` | `proj` | `SmtGisPrj` |
| `SmtRender` | `render` | `render` | `render` | `SmtRender` |
| `Smt3DRenderer` | `render3d` | `render/render3d` | `render3d` | `Smt3DRenderer` |
| `SmtGdiRenderDevice` | `render_gdi` | `render/gdi` | `render_gdi` | `SmtGdiRenderDevice` |
| `SmtGdiSimpleRenderDevice` | `render_gdi_simple` | `render/gdi_simple` | `render_gdi_simple` | `SmtGdiSimpleRenderDevice` |
| `SmtGLRenderDevice` | `render_gl` | `render/gl` | `render_gl` | `SmtGLRenderDevice` |
| `SmtD3DRenderDevice` | `render_d3d` | *(removed)* | — | D3D9/D3DX tree deleted; do not resurrect |
| `SmtSDEDeviceMgr` | `sde_mgr` | `sdb/datasource/mgr` | `sde_mgr` | `SmtSDEDeviceMgr` |
| `SmtSDEGdalDevice` | `sde_gdal` | `sdb/datasource/gdal` | `sde_gdal` | `SmtSDEGdalDevice` |
| `SmtSDEMemDevice` | `sde_mem` | `sdb/datasource/mem` | `sde_mem` | `SmtSDEMemDevice` |
| `SmtSDESmfDevice` | `sde_smf` | `sdb/datasource/smf` | `sde_smf` | `SmtSDESmfDevice` |
| `SmtSDEWSDevice` | `sde_ws` | `sdb/datasource/ws` | `sde_ws` | `SmtSDEWSDevice` |
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
| `SmtAMOrthogrid` | `plugin_orthogrid` | `plugin/orthogrid` | `plugin_orthogrid` | `SmtAMOrthogrid` (orthogrid UI + Eigen Laplace kernel; not `src_all`). Leftover AM stem `SmtAMBAOGridCreater` still maps to `smartgis.orthogrid`. |
| `SmtAMDemCreater` | `plugin_dem` | `plugin/dem` | `plugin_dem` | `SmtAMDemCreater` |
| `SmtAMMapPrint` | `plugin_print` | `plugin/print` | `plugin_print` | `SmtAMMapPrint` |
| `SmtAMMapProject` | `plugin_proj` | `plugin/proj` | `plugin_proj` | `SmtAMMapProject` |
| `SmartGis` | `app` | `app` | `app` | `SmartGis.exe` (`build.bat app`) |
| `SmtAppCore` | `app_core` | `app/app_core` | `app_core` | `SmtAppCore` |
| — | `views` (exe) | `app/views` | `views` (`//:ui_views`) | `SmartGisViews.exe` |
| — | `winui` | `app/winui` | `app_winui` | `SmartGisWinui.exe` |
| `SmtTinMesh` | `tin` | `algorithm/tin` | `tin` | `SmtTinMesh` |
| `Smt3DBaseLib` | `scene3d` | `render/scene3d` | `scene3d` | `Smt3DBaseLib` (leftover) |
| — | `scene` | `render/scene` | `scene` | GPU cache (source_set) |
| — | `model` / `scene` | `sdb/model`, `sdb/scene` | `model` / `scene` | CPU assets + World (source_sets) |
| `Smt3DMdLib` | `model3d` | `render/model3d` | `model3d` | `Smt3DMdLib` (leftover) |
| `Smt3DPointCloud` | `pointcloud` | `render/pointcloud` | `pointcloud` | `Smt3DPointCloud` |
| `Smt3DTerrain` | `terrain` | `render/terrain` | `terrain` | `Smt3DTerrain` |
| `SmtNetCore` | `net` | `net/{pack,http,rpc,udp}` | `net` | `SmtNetCore` |
| `SmtStaCore` | `stat` | `algorithm/stat` | `stat` | `SmtStaCore` |
| `SmtStaDiagram` | `stat_chart` | `ui/chart` | `stat_chart` | `SmtStaDiagram` |

`—` = no `BUILD.gn` yet (do not drop already-wired `src_all` deps). `app` is `//src/app:app` → `out/SmartGis.exe`; keep it out of `src_all` so the daily `build.bat` DLL set stays green without loading the MFC graph. After algorithm modernization, `SmtMathLib` / `Smt3DMathLib` / `Smt3DGeoCore` / `SmtDemCore` are not separate outputs (`SmtDemCore` is gone; heightmap I/O is `plugin/dem` + GDAL).

Repo-root `//core:core_all` still aliases `//src:src_all`. It is not the product tree.

Include dirs in `//build:smt_legacy` still point at **each leftover module root** (quoted `#include "header.h"`). New trees (`net`, `content`, `sdb/scene`, …) use `"layer/module/file.h"` via `//src`.

## File naming (mgis / Chromium)

| Tree | Stem | Extension | Include |
| --- | --- | --- | --- |
| New (`content`, `gpu`, `app/{views,winui}`, `ui/views`, `render/{skia,rhi,scene}`, `sdb/{model,scene}`, `net`) | `snake_case` | `.cc` / `.h` (`net` keeps `.cpp`) | `"content/public/map_view.h"`, `"ui/views/view.h"`, `"gpu/gpu.h"`, `"render/rhi/rhi.h"`, `"sdb/scene/scene.h"`, `"net/http/http.h"` (`//src` on the include path) |
| Legacy product (`app` MFC, `ui/{gui,mfc_ex,xview,…}`, `plugin/*`, …) | `snake_case` | keep `.cpp` | still module-root `"main_frame.h"` / `"grid_ctrl.h"` |

- Drop file prefixes (`smt_`, `vw_`, `cata_`, `baog_`, `msvr_`, `am_`, `gt_`, `wa_`, `bl_`, `rd_`, plus module tags `gis_` / `geo_` / `sde_`). **DLL stems**, `Export_Smt*`, and `Smt_*` namespaces stay.
- CRT collisions keep a short qualifier (`core_assert.h`, `net_string.h`), not the old prefix.
- `stdafx` / `targetver` / `resource.h` keep those conventional names.

---

**最后更新：** 2026-09-13
