<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/` layout (layered)

> **In progress:** full include-path + ABI cutover (mogu-style `#include`, snake_case / 两层命名空间). Spec: [`../superpowers/specs/2026-09-13-code-style-include-abi-cutover-design.md`](../superpowers/specs/2026-09-13-code-style-include-abi-cutover-design.md). Map: [`abi-rename-map.md`](abi-rename-map.md).  
> **DLL reorg（2026-09-28）：** 产品 import-link stems — **`base`** · **`gis`**（原 sdb/algorithm）· **`render`** · **`net`**（从 base 抽出）· **`content`** · **`ui_views`**（views+gfx 同 PE）· **`tool`** · **`plugin_host`**。app-gated `ui_legacy`；optional `legacy_render` / `legacy_tool`；legacy **`*.am` 仍 LoadLibrary**。产物在仓库根 **`out/`**（与 exe 同目录；不要 `out/Default`）。手法 C：细 `source_set` + `group` 转发。Spec: [`../superpowers/specs/2026-09-14-dll-reorganization-design.md`](../superpowers/specs/2026-09-14-dll-reorganization-design.md)。终态 stem 表：[`abi-rename-map.md`](abi-rename-map.md)。  
> **`src/base` foundation Hybrid（Phases 0–6 已收口；真源仅在 `src/`）：** [`../../src/base/`](../../src/base/) 为 mogu 对齐 foundation（`//src/base:foundation`，非产品 DLL）。仓库根**无**物理 `base/`、`core/`；兼容别名 `//:base` / `//:core` 仅在根 `BUILD.gn`。产品平台 DLL **`dll_stem=base`**（`base.dll` / `base_d.dll`；**不含** net）。Spec: [`../superpowers/specs/2026-09-14-base-root-hybrid-design.md`](../superpowers/specs/2026-09-14-base-root-hybrid-design.md)。

Product sources for GIS / UI / render stay under **`src/`**. Foundation also lives under **`src/base/`** (`:foundation` source_set). Compatibility forwards are **`//:base`** and **`//:core`** only (no physical repo-root `base/` or `core/`). Directory names under `src/` drop the 2010 `Smt` prefix. Nesting is by layer; **on-disk product DLL** follows the reorg（不再「短名各一 DLL」）。

**Nesting cap:** at most `src/<layer>/<module>` (two levels under `src/`). Leftover MFC/GDI trees sit under layer `legacy/` → `src/legacy/{app,ui,render,tool,plugin,sys,xml}`.

**Directory vs DLL:** 目录与 GN 标签路径仍可细（`//src/gis/datasource/gdal:sde_gdal` 等为 group → 层 DLL）。磁盘 `dll_stem` 见上表。Debug 产出在仓库根 `out/`，stem 后加 `_d`（`base_d.dll`），不是尾缀 `D`。

New public namespaces stay at most two levels (`geo`, `base::detail` for internals).

## Five layers (locked)

| Layer | Tree | Notes |
| --- | --- | --- |
| app | `src/app/{views,winui,cef,cs}`；MFC 壳 → `src/legacy/app/{core,shell,doc,view}`（根留 `.rc`/`stdafx`/`res/`） | Endgame/prototype hosts only. Do not add a separate browser-shell tree. Namespace `app`. |
| content | `src/content/public`（9 个头）+ `{app,browser,renderer,view,embed,common}` | 嵌入方只包含 `public/`：`map_types`、`map_contents`（含视口）、`map_contents_observer`、`event_bus`（含领域事件）、`view_host`、`plugin_host`、`catalog_layers`、`feature_attrs`、`map_bootstrap`。管道帧在 `common/host_protocol.h`。 |
| sdb | `src/sdb/{feature,layer,map,crs,datasource/<driver>,model,scene,tile,style,carto}` | GIS model; CPU assets (`model`) and World (`scene`); HTTP XYZ tiles (`tile`); MapLibre-subset Style JSON / symbol / rules (`style`); cartographic POD (`carto`, linked into base.dll) |
| render | `src/render/{rhi,scene,graph,skia,math}` | Endgame: unified 2D+3D RHI (FlyCube DX12/Vulkan), `GpuScene`, frame graph, Skia stub, scene math. Leftover engines live under `src/legacy/render/…` and are **not** in `src_all` by default (optional `//src/legacy/render:legacy_render_all`). **Paint runs in `--type=gpu`**, not in browser. |
| effect | `src/effect/{map,atmosphere}` | GPU map and atmosphere passes. `source_set` linked by callers, not a DLL, and not inside `render.dll`. |
| base | `src/base/`（foundation only）+ `src/base/carto` + `src/legacy/{core,xml,sys}` | **`//src/base:foundation`**：mogu 式 log/threading/files/… + `archive`/`ipc`。**产品 DLL** **`dll_stem=base`**（leftovers+carto+xml+sys；**不含** net）。HTTP/RPC → **`//src/net:net`**。`carto` = cartographic POD（**not** Style JSON）。 |

## OSS GIS ↔ this tree

| QGIS / GDAL / GEOS / PROJ | This repo |
| --- | --- |
| `providers/*`, OGR drivers | `src/sdb/datasource/gdal`（`mem` / `smf` / `ws` 已移除） |
| `QgsFeature` / `QgsMapLayer` / `QgsProject` | `src/sdb/feature`, `layer`, `map` |
| `QgsCoordinateReferenceSystem` | `src/sdb/crs`; transforms in `gis/kernel/proj` |
| GEOS | `src/gis/kernel/geo`（`//src/gis:geom` → `gis` DLL）— wrap `//third_party:gdal` (`.install` `geos_c`); no second GEOS vendor |
| PROJ | `src/gis/kernel/proj` (PROJ 9 adapter only) |
| map canvas renderer | `src/render` + RHI |
| processing / analysis | `src/gis/kernel/{geo,proj,tin,stat}` (`gis` DLL) |
| `qgis_gui` / `qgis_app` | `src/ui/` / `src/app/` |
| libqgis_core for embedders | `src/content/public` |

`gis/datasource/gdal` is `SmtSDEGdalDevice` (OGR codec in `gis/datasource/ogr`, remote + decorator SDBD in `gis/datasource/sdbd`): a decorator `GDALDriver` `"SDBD"` whose `SdbdDataset` owns a stock inner `GDALDataset` (Memory / GPKG / PostgreSQL / file). Callers use `GDALOpenEx("SDBD:…")` and may `dynamic_cast` to `SdbdDataset` / `SdbdLayer`. **Remote mogu sdbd** uses `PROVIDER_SDBD` + `SdbdClient` (HTTP+FnRPC), not the local Handler prefix. Do not patch `third_party/.src/gdal` or resurrect `OgrDataSource`. I/O types are `GDALDataset` / `OGRLayer` / `OGRFeature`；产品 ABI 是 `sdb::Feature` / `sdb::MapLayer`（组合持有 OGR）。栅格草稿：`CreateMemRasLayer` → `OgrRasterLayer` + GDAL **MEM**（编码 blob 在 `/vsimem`；`Open(文件)` 会回填 blob 供 `GetRasterNoClone`）；`SmtMemRasLayer` 已移除。`CreateMemTileLayer` / `SmtMemTileLayer` / `sde_mem` 已切除。2D 瓦片：`src/gis/present/tile`（`TileProvider` + LRU/磁盘缓存 + WMTS 最小解析 + `make_xyz_map_layer` / Views `AddBasemapDialog`；HTTP(S) 经 net+OpenSSL；不进 `SDBD:MEM`）。`SmtAttribute`/`SmtField` 已删除（字段只走 OGR）；MFC att-struct UI 读/写 `OGRLayer`。产品类型是 `gis::Feature`（leftover TU 用 `using SmtFeature = Feature`）。

## DLL 粒度（as-built 2026-09-28）

| `dll_stem` | 树 / 吸收 | 默认 `src_all` / 宿主 |
| --- | --- | --- |
| **`base`** | leftovers + carto + xml + sys。foundation（`archive`/`ipc`）`:foundation` 非 DLL | yes |
| **`net`** | `src/net`（HTTP/RPC；从 base 抽出） | yes |
| `gis` | `src/gis/**`（原 sdb + algorithm 内核） | yes |
| `render` | `render/{rhi,scene,skia,…}`（endgame only） | yes |
| `content` | `src/content/**` | yes |
| `tool` | `src/tool/**`（`SMT_TOOL_*`；`:dispatch` 转发） | yes |
| `ui_views` | `ui/views` + `ui/gfx`（同 PE） | **no**（SmartGisViews / plugin_host） |
| `plugin_host` | `plugin/runtime/host` + widgets（`PLUGIN_HOST_*`） | **no**（Views 宿主） |
| `ui_legacy` | `legacy/ui/{gui,mfc_ex,…}` | **no**（`smt_build_app`） |
| `app_core` | `legacy/app/core/smtapp.cpp` | **no**（`smt_build_app`） |
| `legacy_render` | `legacy/render/**` | **no**（optional） |
| `legacy_tool` | `legacy/tool/**`（非产品 `tool.dll`） | **no**（optional） |
| `plugin_*` / `plugin` | 域插件 / leftover AuxModule；`*.am` LoadLibrary | 按需 |

仍为 **source_set**（有意）：产品 `*_views` / `processing_views`、`effect/*`、`gpu`、`base/math`、legacy `adapter`。gpu 只 deps `//src/ui/gfx:gfx_headers`（不拉 `ui_views`）。

## Layering plan (directory)

| Layer | Tree | Directory / product notes | In `src_all` |
| --- | --- | --- | --- |
| Foundation | `src/base/`（含 `archive`/`ipc`；core leftovers 在 `legacy/core`）, `base/carto`, `legacy/{core,xml,sys}` | **`//src/base:foundation`**（static）。**产品 DLL**：`dll_stem=base`（无 net） | yes → `base`；foundation 经 deps 链入 |
| Net | `src/net` | **`dll_stem=net`**（import-link） | yes → `net` |
| Core data model | `src/gis/model`、`present`、`scene`（目录名仍可能写 sdb） | GIS 模型 + CPU assets / World / TileProvider / StyleDocument；**一 DLL `gis`**（`carto` 链入平台 DLL） | yes → `gis` |
| Datasource | `sdb/datasource/{mgr,gdal}`（树或已迁 `gis/datasource`） | 并入 `sdb`/`gis` DLL。SMF/WS/mem leftovers 已删。**`PROVIDER_SDBD`**：`DataSourceMgr::open_dataset` → `SdbdRemoteDataset` + `SdbdClient` 双通道对接 WSL mogu sdbd（HTTP `/api/v1/sdbd/*` 默认 `:8021`；FnRPC `sdbd.*` 默认 `:9032`；`szUrl` 为 `http(s)://…` 或 `sdbd-rpc://host:port`）。本地 `SdbdHandler` 仍用 `/sdbd/api/v1/*`，勿混用。活体硬测：`sdbd_live_test`（非 SKIP） | yes → `gis` |
| Algorithm | `gis/kernel/{geo,proj,tin,stat}` | 编进 **`gis.dll`**（`//src/gis:algorithm` 转发）。Scene Vector/Matrix 在 `base/math`（命名空间 `render`，不进 `base.dll`）。**Not** dem/orthogrid（插件）/ chart（`ui_legacy`） | yes → `gis` |
| Render | `render/{rhi,scene,skia}` | Endgame **一 DLL `render`**。场景数学在 `src/base/math`（不进本 DLL）。Leftover 引擎在 `legacy/render/` → optional `legacy_render` DLL | yes → `render`；leftover optional |
| Plugin | `plugin/runtime`（host / processing / widgets / python）+ `plugin/product`（dem / proj / print / model3d / orthogrid）+ leftover `legacy/plugin/` | Host **`dll_stem=plugin_host`**（`//src/plugin:host`；`PLUGIN_HOST_*`）。`*.am` / `AM_MSG` 在 `legacy/plugin/adapter`。Leftover AuxModule `//src/legacy/plugin:plugin`。MFC 域插件 `am_plugin=true` → **`out/plugin/<stem>[_d].am`**。Views 走 builtin + import-link `plugin_host`，不扫 `*.am`。 | host DLL + widgets；域插件按需 |
| UI (leftover) | `legacy/ui/{gui,mfc_ex,xview,xcatalog,xambox,chart}` | **一 DLL `ui_legacy`**。`//src/ui:ui_legacy` 转发 | **no**（`smt_build_app`） |
| UI toolkit (endgame) | `ui/views` + `ui/gfx` | **`dll_stem=ui_views`**（同 PE）；`:views` / `:gfx` 转发；`:gfx_headers` 给 gpu | **no**（SmartGisViews） |
| Hosted map | `content/public` + `content/{app,browser,renderer,view,common}` | **`dll_stem=content`**；`:view_host` 同 PE | yes → `content` |
| GPU main (`--type=gpu`) | `gpu/` | 同 PE `GpuMain`；deps **`:gfx_headers`**（不拉 ui_views）。`build.bat render` 为 GPU 进程别名。 | **no** |
| App (endgame) | `app/{views,winui,cef,cs}` | SmartGisViews import-link 产品 DLL 集 | **no** |
| App (leftover) | `legacy/app/{core,shell,doc,view}` + 根 `.rc`/`stdafx`/`res/`（`core/smtapp.cpp` → `dll_stem=app_core`） | MFC `SmartGis.exe`（`//src/legacy/app:app`） | **no**（`smt_build_app`） |
| Tool | `tool/{command,interaction,draft,nav,workspace}` + `gis/model/edit`；`GT_MSG` bridge at `legacy/tool/adapter` | **`dll_stem=tool`**（`:dispatch`→`:tool`）；adapter source_set；leftover → `legacy_tool`；`edit` 进 `gis` | yes → `tool` |

**Deliberately not merged** *(directory / product splits — DLL 已按上表合并)*

- `//src/base:core` / `:base` / `:platform` / `//src/legacy/sys:sys` 为 **group → `base` DLL**。`//src/net:net` 是**独立**产品 DLL。**不要**与 `//src/base:foundation`（或 `//:base` / `//:core`）混淆。
- Homemade Vector/Matrix were removed from `gis/kernel/geo`. Scene `Vector3` / `Matrix` / bounds live in `src/base/math`（namespace `render`；不进 `base.dll` / `render.dll`）。
- `src/base/carto/` 链入 **base.dll**。Style JSON 在 `gis/present/style`。
- `geo::geometry_traits` / `vector_traits` wrap OGR；Delaunay in `gis/kernel/tin`。No second geometry tree.
- **不要**把 `legacy_render` 并进 `render`；**不要**把域插件并进平台 DLL；**不要**把 `ui_views` 拆成 `gfx.dll`。
- MFC Feature Pack / `ui_legacy` stays out of default `src_all`。日常产品入口：`build.bat app` → Views。

### Desktop UI endgame (Views + Skia)

Chosen destination: Chromium-style **Views** + **Skia** + existing C++ map viewport. Tree: `src/ui/views`, `src/ui/gfx` (gfx public dirs: `geometry/` · `color/` · `canvas/` · `display_list/` · `raster/` · `image/` · `font/` · `animation/`). Doc: [`ui-views-skia.md`](ui-views-skia.md). Feature Pack, WinUI, and WebView2 are **not** the endgame. Qt is banned.

### MFC Feature Pack (legacy exe bootstrap)

`build.bat legacy_app` links **MFC Feature Pack** (`CMFCRibbonBar` / `CDockablePane` / `CMDIFrameWndEx` via `legacy/ui/mfc_ex/bcg_cmfc.h`). BCGControlBar Pro is **not** required and is not vendored. This is a compile bridge, **not** the destination toolkit (Views + Skia). `build.bat app` builds Views.

| Dep | How to satisfy |
| --- | --- |
| **MFC** (MBCS, `afxwin.h` / `afxres.h` / `afxcontrolbars.h`) | VS 18 Individual component **C++ MFC for x64/x86 (Latest MSVC)** = `Microsoft.VisualStudio.Component.VC.ATLMFC`, or toolset-pinned `Microsoft.VisualStudio.Component.VC.14.50.18.0.MFC`. After install, re-run `build.bat legacy_app` so `out/environment.x64.x64` picks up `atlmfc\include` + `atlmfc\lib\x64`. Close `cl`/`ninja`/`link` first, or the installer precheck `VSProcessesRunning` cancels (error `0x1f46`). |

## Path map (2010 dir → short name → layered → 终态 DLL)

| Old directory | Short (`src/…`) | Layered (`src/…`) | GN target | 终态 `dll_stem` |
| --- | --- | --- | --- | --- |
| `SmtCore` | `core` | `legacy/core`（foundation 头仍在 `base/core`） | `legacy/core` → `//src/base:base` | `base` |
| `SmtSysCore` | `sys` | `sys` | `sys` → `//src/base:base` | `base` |
| `SmtMathLib` | `math` | `algorithm/math` | (absorbed into geo) | `gis` |
| `Smt3DMathLib` | `math3d` | `algorithm/math3d` | (absorbed) | `gis` |
| `SmtBaseLib` | `base` | `base/carto` | `carto_sources` → `//src/base:base` | `base` |
| `SmtGeoCore` | `geo` | `gis/kernel/geo` | `geo` → `//src/gis:gis` | `gis` |
| `Smt3DGeoCore` | `geo3d` | `gis/kernel/geo` | (absorbed) | `gis` |
| `SmtGisCore` | `gis` | `sdb/{feature,layer,map}` | `gis` → `//src/sdb:sdb` | `sdb` |
| `SmtGisPrj` | `proj` | `gis/kernel/proj` | `proj` → `//src/gis:gis` | `gis` |
| `SmtRender` | `render` | `render` / leftover bridge | endgame → `render`；bridge → `legacy_render` | `render` / `legacy_render` |
| `Smt3DRenderer` | `render3d` | `legacy/render/rhi3d` (was `render3d`) | → `legacy_render` | `legacy_render` |
| `SmtGdiRenderDevice` | `render_gdi` | `legacy/render/rhi2d/impl/gdi` | → `legacy_render` | `legacy_render` |
| `SmtGdiSimpleRenderDevice` *(retired)* | *(was `render_gdi_simple`)* | — | Alias → `CreateRenderDevice` / `gdi/` | `legacy_render` |
| `SmtGLRenderDevice` | `render_gl` | `legacy/render/rhi3d/impl/gl` (was `legacy/render/gl`) | → `legacy_render` | `legacy_render` |
| `SmtD3DRenderDevice` | `render_d3d` | *(removed)* | — | — |
| `SmtSDEDeviceMgr` | `sde_mgr` | `sdb/datasource/mgr` | → `sdb` | `sdb` |
| `SmtSDEGdalDevice` | `sde_gdal` | `sdb/datasource/gdal` | → `sdb` | `sdb` |
| `SmtSDEMemDevice` | `sde_mem` | *(removed)* | — | — |
| `SmtSDESmfDevice` | `sde_smf` | *(removed)* | — | — |
| `SmtSDEWSDevice` | `sde_ws` | *(removed)* | — | — |
| `SmtToolCore` | `tool` | `legacy_tool` | → `legacy_tool` | `legacy_tool` |
| `SmtGroupToolCore` | `tool_group` | `legacy/tool/group/{base,view,select,input,factory}` | sources → `ui_legacy`（避环） | `ui_legacy` |
| — | `dispatch` | `tool/{command,interaction,draft,nav,workspace}` | `dispatch` | — (group→source_sets) |
| — | `tool_adapter` | `legacy/tool/adapter` | `//src/legacy/tool/adapter:adapter` | — (source_set；`namespace tool`) |
| — | `edit` | `sdb/edit` | → `sdb` | `sdb` |
| `SmtGuiCore` | `gui` | `legacy/ui/gui` | → `ui_legacy` | `ui_legacy` |
| `SmtMFCExCore` | `mfc_ex` | `legacy/ui/mfc_ex` | → `ui_legacy` | `ui_legacy` |
| `SmtXViewCore` | `xview` | `legacy/ui/xview` | → `ui_legacy` | `ui_legacy` |
| `SmtXCatalogCore` | `xcatalog` | `legacy/ui/xcatalog` | → `ui_legacy` | `ui_legacy` |
| `SmtXAMBoxCore` | `xambox` | `legacy/ui/xambox` | → `ui_legacy` | `ui_legacy` |
| — | `views` | `ui/views` | `views` (`//:ui_views`) | — (source_set) |
| — | `gfx` | `ui/gfx` | `//src/ui/gfx:gfx` | — (source_set) |
| `SmtAuxModule` | `plugin` | `legacy/plugin` | `plugin` | `plugin` |
| `SmtAM3DModelCreater` | `plugin_model3d` | `legacy/plugin/model3d` | `plugin_model3d` | `plugin_model3d` |
| `SmtAMOrthogrid` | `plugin_orthogrid` | `legacy/plugin/orthogrid` | `plugin_orthogrid` | `plugin_orthogrid` |
| `SmtAMDemCreater` | `plugin_dem` | `legacy/plugin/dem` | `plugin_dem` | `plugin_dem` |
| `SmtAMMapPrint` | `plugin_print` | `legacy/plugin/print` | `plugin_print` | `plugin_print` |
| `SmtAMMapProject` | `plugin_proj` | `legacy/plugin/proj` | `plugin_proj` | `plugin_proj` |
| `SmartGis` | `app` | `app` | `app` | `SmartGis.exe` |
| `SmtAppCore` | `app_core` | `app/app_core` | `app_core` | `app_core` |
| — | `views` (exe) | `app/views` (`shell/{app,browser,ui}` + `document` / `camera` / `present/{host,map2d,scene3d}` / `input`; GN `:shell_ui` → `:shell_browser`; camera sibling of shell) | `views` | `SmartGisViews.exe` |
| — | `winui` | `app/winui` | `app_winui` | `SmartGisWinui.exe` |
| — | `cef` | `app/cef` | `cef` | `SmartGisCef.exe` |
| `SmtTinMesh` | `tin` | `gis/kernel/tin` | → `//src/gis:gis` | `gis` |
| `Smt3DBaseLib` | `scene3d` | `legacy/render/scene3d` | → `legacy_render` | `legacy_render` |
| — | `scene` | `render/scene` | → `render` | `render` |
| — | `model` / `scene` / `tile` | `sdb/model`, `sdb/scene`, `sdb/tile` | → `sdb` | `sdb` |
| `Smt3DMdLib` | `model3d` | `legacy/render/scene3d/{primitive,feature}` (was top `model3d/`) | → `legacy_render` | `legacy_render` |
| `Smt3DPointCloud` | `pointcloud` | `legacy/render/scene3d/surface` (was top `pointcloud/`) | → `legacy_render` | `legacy_render` |
| `Smt3DTerrain` | `terrain` | `legacy/render/scene3d/surface` (was top `terrain/`) | → `legacy_render` | `legacy_render` |
| `SmtNetCore` | `net` | `net/{pack,http,rpc}` | → `base` | `base` |
| `SmtStaCore` | `stat` | `gis/kernel/stat` | → `//src/gis:gis` | `gis` |
| `SmtStaDiagram` | `stat_chart` | `legacy/ui/chart` | → `ui_legacy` | `ui_legacy` |

`app` 不进默认 `src_all`。Debug 文件名为 `{stem}_d.dll`。完整对照见 [`abi-rename-map.md`](abi-rename-map.md)。

`//src/base:foundation` is the mogu-aligned foundation. Root `group("base")` (`//:base`) and `group("core")` (`//:core`) forward to it in `BUILD.gn` only — **no** physical repo-root `base/` or `core/` directory. `//:core_all` aliases `//src:src_all`. Product platform DLL is `//src/base:base` with **`dll_stem=base`**.

Include dirs: `BUILDCONFIG` puts **`//src` before `//`** so `#include "base/…"` → `src/base/` only；`//build:legacy` same。Product modules use `"layer/module/file.h"` via `//src`.

## File naming (mgis / Chromium)

| Tree | Stem | Extension | Include |
| --- | --- | --- | --- |
| New (`content`, `gpu`, `app/{views,winui,cef,cs}`, `ui/views`, `render/{skia,rhi,scene}`, `sdb/{model,scene}`, `net`) | `snake_case` | `.cc` / `.h` (`net` keeps `.cpp`) | `"content/public/map_contents.h"`, `"ui/views/kernel/view.h"`, `"gpu/gpu.h"`, `"render/rhi/rhi.h"`, `"sdb/scene/scene.h"`, `"net/http/http.h"` (`//src` on the include path) |
| Legacy product (`legacy/app` MFC, `legacy/ui/{gui,mfc_ex,xview,…}`, `plugin/*`, …) | `snake_case` | keep `.cpp` | still module-root `"main_frame.h"` / `"grid_ctrl.h"`（产品代码用 `"legacy/app/…"` / `"legacy/ui/…"`） |

- Drop file prefixes (`smt_`, `vw_`, `cata_`, `baog_`, `msvr_`, `am_`, `gt_`, `wa_`, `bl_`, `rd_`, plus module tags `gis_` / `geo_` / `sde_`). On-disk **DLL stems** follow reorg 终态（[`abi-rename-map.md`](abi-rename-map.md)）；legacy `Smt_*` 命名空间仍可能存在直至 ABI cutover 收尾。
- CRT collisions keep a short qualifier (`core_assert.h`, `net_string.h`), not the old prefix.
- `stdafx` / `targetver` / `resource.h` keep those conventional names.

---

**最后更新：** 2026-09-28
