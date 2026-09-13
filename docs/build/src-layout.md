<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/` layout (layered)

> **In progress:** full include-path + ABI cutover (mogu-style `#include`, snake_case / 两层命名空间). Spec: [`../superpowers/specs/2026-09-13-code-style-include-abi-cutover-design.md`](../superpowers/specs/2026-09-13-code-style-include-abi-cutover-design.md). Map: [`abi-rename-map.md`](abi-rename-map.md).  
> **DLL reorg Phase 1 已落地：** **一层一 DLL** — 平台 `base` / `sdb` / `algorithm` / `render`；app-gated `ui_legacy`（已验证 `out/ui_legacy_d.dll`）；optional `legacy_render` / `legacy_tool`（不进默认 `src_all`）；**插件仍每插件一 DLL**。手法 C：只改 `shared_library` / `dll_stem` / export，细 `source_set` + 旧 GN 标签 `group` 转发。Spec: [`../superpowers/specs/2026-09-14-dll-reorganization-design.md`](../superpowers/specs/2026-09-14-dll-reorganization-design.md)。终态 stem 表：[`abi-rename-map.md`](abi-rename-map.md)。

Product sources stay under **`src/`** (not repo-root `base/` / `core/` — those are thin GN aliases). Directory names drop the 2010 `Smt` prefix. Nesting is by layer; **on-disk DLL** follows the reorg（不再「短名各一 DLL」）。

**Nesting cap:** at most `src/<layer>/<module>` (two levels under `src/`).

**Directory vs DLL:** 目录与 GN 标签路径仍可细（`//src/sdb/datasource/gdal:sde_gdal` 等为 group → 层 DLL）。磁盘 `dll_stem` 见上表。Debug 产出在 stem 后加 `_d`（`base_d.dll`），不是尾缀 `D`。

New public namespaces stay at most two levels (`geo`, `base::detail` for internals).

## Five layers (locked)

| Layer | Tree | Notes |
| --- | --- | --- |
| app | `src/app/` + `app/{app_core,views,winui}` | All product hosts. No `src/chrome/`. Namespace `app`. |
| content | `src/content/public` | Stable embedder API. Hosts do not include sdb / render devices. |
| sdb | `src/sdb/{feature,layer,map,crs,datasource/<driver>,model,scene,tile}` | GIS model; CPU assets (`model`) and World (`scene`); HTTP XYZ tiles (`tile`). |
| render | `src/render/{rhi,scene,skia,math}` | Endgame: unified 2D+3D RHI (FlyCube DX12/Vulkan), `GpuScene`, Skia stub, scene math. Leftover engines live under `src/legacy_render/…` and are **not** in `src_all` by default (optional `//src/legacy_render:legacy_render_all`). **Paint runs in `--type=gpu`**, not in browser. |
| base | `src/base/{core,style,ipc,archive}` | `core` = `SmtCore`. `style` = `SmtBaseLib` (cartographic pen/brush/symbol + Envelope; not Views/CSS). `ipc` = named pipe + frame codec（deps → `archive`，not net）。`archive` = BinarySink / Serializer（`base::`；A1）。`net::Pickle` 仍在 `net/pack`。 |

## OSS GIS ↔ this tree

| QGIS / GDAL / GEOS / PROJ | This repo |
| --- | --- |
| `providers/*`, OGR drivers | `src/sdb/datasource/gdal`（`mem` / `smf` / `ws` 已移除） |
| `QgsFeature` / `QgsMapLayer` / `QgsProject` | `src/sdb/feature`, `layer`, `map` |
| `QgsCoordinateReferenceSystem` | `src/sdb/crs`; transforms in `algorithm/proj` |
| GEOS | `src/algorithm/geo`（`//src/algorithm:geom` → `algorithm` DLL）— wrap `//third_party:gdal` (`.install` `geos_c`); no second GEOS vendor |
| PROJ | `src/algorithm/proj` (PROJ 9 adapter only) |
| map canvas renderer | `src/render` + RHI |
| processing / analysis | `src/algorithm/` |
| `qgis_gui` / `qgis_app` | `src/ui/` / `src/app/` |
| libqgis_core for embedders | `src/content/public` |

`sdb/datasource/gdal` is `SmtSDEGdalDevice`: a decorator `GDALDriver` `"SDBD"` whose `SdbdDataset` owns a stock inner `GDALDataset` (Memory / GPKG / PostgreSQL / file). Callers use `GDALOpenEx("SDBD:…")` and may `dynamic_cast` to `SdbdDataset` / `SdbdLayer`. Do not patch `third_party/.src/gdal` or resurrect `OgrDataSource`. I/O types are `GDALDataset` / `OGRLayer` / `OGRFeature`；产品 ABI 是 `sdb::Feature` / `sdb::MapLayer`（组合持有 OGR）。栅格草稿：`CreateMemRasLayer` → `OgrRasterLayer` + GDAL **MEM**（编码 blob 在 `/vsimem`；`Open(文件)` 会回填 blob 供 `GetRasterNoClone`）；`SmtMemRasLayer` 已移除。`CreateMemTileLayer` / `SmtMemTileLayer` / `sde_mem` 已切除。2D 瓦片：`src/sdb/tile`（`TileProvider` + LRU/磁盘缓存 + WMTS 最小解析 + `make_xyz_map_layer` / Views `AddBasemapDialog`；HTTP(S) 经 net+OpenSSL；不进 `SDBD:MEM`）。`SmtAttribute`/`SmtField` 已退出 `gis`（字段走 OGR）；可选 leftover `//src/sdb/map:leftover_attr`；MFC att-struct UI 读/写 `OGRLayer`。

## DLL 粒度（Phase 1 终态）

| `dll_stem` | 树 / 吸收 | 默认 `src_all` |
| --- | --- | --- |
| `base` | `base/{core,style,ipc,archive}` + `sys` + `net` | yes |
| `sdb` | `sdb/{feature,layer,map,model,scene,tile,edit,datasource}` | yes |
| `algorithm` | `algorithm/{geo,proj,tin,stat}` | yes |
| `render` | `render/{rhi,scene,skia,…}`（endgame only） | yes |
| `ui_legacy` | `ui/{gui,mfc_ex,xview,xcatalog,xambox,chart}` | **no**（`smt_build_app`） |
| `legacy_render` | `legacy_render/**` | **no**（optional） |
| `legacy_tool` | `legacy_tool/**`（`tool_group` 源链入 `ui_legacy`） | **no**（optional） |
| `plugin_*` / `plugin` | 每插件一 DLL；host 为 source_set | host 进图；域插件按需 |

不成产品 DLL：`content`、`tool/dispatch`、`plugin/host`、`ui/views`（均为 source_set）。

## Layering plan (directory)

| Layer | Tree | Directory / product notes | In `src_all` |
| --- | --- | --- | --- |
| Foundation | `base/{core,style,ipc,archive}`, `sys`, `net` | 一层目录；**一 DLL `base`**（旧短名标签 group 转发） | yes → `base` |
| Core data model | `sdb/{feature,layer,map,model,scene,tile}` | GIS 模型 + CPU assets / World / TileProvider；**一 DLL `sdb`** | yes → `sdb` |
| Datasource | `sdb/datasource/{mgr,gdal}` | 并入 `sdb` DLL。SMF/WS/mem leftovers 已删 | yes → `sdb` |
| Algorithm | `algorithm/{geo,proj,tin,stat}` | **一 DLL `algorithm`**。Scene Vector/Matrix 在 `render/math`。**Not** dem/orthogrid（插件）/ chart（`ui_legacy`） | yes → `algorithm` |
| Render | `render/{rhi,scene,skia,math}` | Endgame **一 DLL `render`**。Leftover 引擎在 `legacy_render/` → optional `legacy_render` DLL | yes → `render`；leftover optional |
| Plugin | `plugin/` + children | Host `//src/plugin/host:host`（source_set）。域插件 **各一 DLL**。Spec: `2026-09-14-plugin-subdir-layout-design.md` | host + widgets |
| UI | `ui/{gui,mfc_ex,xview,xcatalog,xambox,chart}` | **一 DLL `ui_legacy`**（已完成）。MFC Feature Pack chrome | **no**（`smt_build_app`） |
| UI toolkit (endgame) | `ui/views` | Views stub（`//:ui_views`）；source_set | **no** |
| Hosted map | `content/public` + `content/app` | Embedder API；`ContentMain` 分发 `--type=` | **yes**（source_set，非 DLL） |
| GPU main (`--type=gpu`) | `gpu/` | 同 PE `GpuMain`；`build.bat render` 为 GPU 进程别名 | **no** |
| App | `app/` + `app/{app_core,views,winui}` | MFC exe + hosts；`app_core` 仍可独立 DLL | **no** |
| Tool | `tool/` (`dispatch`) + `sdb/edit` | `dispatch` source_set；leftover → `legacy_tool` DLL；`edit` 进 `sdb` | yes（`dispatch`）；leftover optional |

**Deliberately not merged** *(directory / product splits — DLL 已按上表合并)*

- `//src/base:core` / `:base` / `//src/sys:sys` / `//src/net:net` 等标签保留为 **group → `dll_stem=base`**，不是第二套平台 DLL。
- Homemade Vector/Matrix were removed from `algorithm/geo`. Scene `Vector3` / `Matrix` / bounds live in `src/render/math` as Eigen-backed POD adapters (`//src/render/math:math`, `:bounds`). Public umbrella header: `render/math/math.h`. API is `snake_case`. OGC `coordinateDimension` is 2 or 3 on the Geometry **instance**. See algorithm-layer-oss + render-math-refactor specs.
- `base`: keep the name. `style/` is envelope + cartographic style, not `ui/views`.
- `geo::geometry_traits` / `vector_traits` wrap OGR；Delaunay in `algorithm/tin`。No second geometry tree.
- **不要**把 `legacy_render` 并进 `render`；**不要**把插件并进平台 DLL；**不要**把 `content` / `dispatch` / `plugin/host` 做成产品 DLL。
- MFC Feature Pack / `ui_legacy` stays out of default `src_all`. Exe graph gated by `smt_build_app`（`build.bat app` / `build.bat ui_legacy`）。

### Desktop UI endgame (Views + Skia)

Chosen destination: Chromium-style **Views** + **Skia** + existing C++ map viewport. Tree: `src/ui/views`, `src/render/skia`. Doc: [`ui-views-skia.md`](ui-views-skia.md). Feature Pack, WinUI, and WebView2 are **not** the endgame. Qt is banned.

### MFC Feature Pack (legacy exe bootstrap)

`build.bat app` links **MFC Feature Pack** (`CMFCRibbonBar` / `CDockablePane` / `CMDIFrameWndEx` via `src/ui/mfc_ex/bcg_cmfc.h`). BCGControlBar Pro is **not** required and is not vendored. This is a compile bridge, **not** the destination toolkit (Views + Skia).

| Dep | How to satisfy |
| --- | --- |
| **MFC** (MBCS, `afxwin.h` / `afxres.h` / `afxcontrolbars.h`) | VS 18 Individual component **C++ MFC for x64/x86 (Latest MSVC)** = `Microsoft.VisualStudio.Component.VC.ATLMFC`, or toolset-pinned `Microsoft.VisualStudio.Component.VC.14.50.18.0.MFC`. After install, re-run `build.bat app` so `out/environment.x64.x64` picks up `atlmfc\include` + `atlmfc\lib\x64`. Close `cl`/`ninja`/`link` first, or the installer precheck `VSProcessesRunning` cancels (error `0x1f46`). |

## Path map (2010 dir → short name → layered → 终态 DLL)

| Old directory | Short (`src/…`) | Layered (`src/…`) | GN target | 终态 `dll_stem` |
| --- | --- | --- | --- | --- |
| `SmtCore` | `core` | `base/core` | `core` → `//src/base:base` | `base` |
| `SmtSysCore` | `sys` | `sys` | `sys` → `//src/base:base` | `base` |
| `SmtMathLib` | `math` | `algorithm/math` | (absorbed into geo / algorithm) | `algorithm` |
| `Smt3DMathLib` | `math3d` | `algorithm/math3d` | (absorbed) | `algorithm` |
| `SmtBaseLib` | `base` | `base/style` | `base` (`//src/base:base`) | `base` |
| `SmtGeoCore` | `geo` | `algorithm/geo` | `geo` → `//src/algorithm:algorithm` | `algorithm` |
| `Smt3DGeoCore` | `geo3d` | `algorithm/geo3d` | (absorbed) | `algorithm` |
| `SmtGisCore` | `gis` | `sdb/{feature,layer,map}` | `gis` → `//src/sdb:sdb` | `sdb` |
| `SmtGisPrj` | `proj` | `algorithm/proj` | `proj` → `algorithm` | `algorithm` |
| `SmtRender` | `render` | `render` / leftover bridge | endgame → `render`；bridge → `legacy_render` | `render` / `legacy_render` |
| `Smt3DRenderer` | `render3d` | `legacy_render/render3d` | → `legacy_render` | `legacy_render` |
| `SmtGdiRenderDevice` | `render_gdi` | `legacy_render/gdi` | → `legacy_render` | `legacy_render` |
| `SmtGdiSimpleRenderDevice` | `render_gdi_simple` | `legacy_render/gdi_simple` | → `legacy_render` | `legacy_render` |
| `SmtGLRenderDevice` | `render_gl` | `legacy_render/gl` | → `legacy_render` | `legacy_render` |
| `SmtD3DRenderDevice` | `render_d3d` | *(removed)* | — | — |
| `SmtSDEDeviceMgr` | `sde_mgr` | `sdb/datasource/mgr` | → `sdb` | `sdb` |
| `SmtSDEGdalDevice` | `sde_gdal` | `sdb/datasource/gdal` | → `sdb` | `sdb` |
| `SmtSDEMemDevice` | `sde_mem` | *(removed)* | — | — |
| `SmtSDESmfDevice` | `sde_smf` | *(removed)* | — | — |
| `SmtSDEWSDevice` | `sde_ws` | *(removed)* | — | — |
| `SmtToolCore` | `tool` | `legacy_tool` | → `legacy_tool` | `legacy_tool` |
| `SmtGroupToolCore` | `tool_group` | `legacy_tool/group` | sources → `ui_legacy`（避环） | `ui_legacy` |
| — | `dispatch` | `tool` | `dispatch` | — (source_set) |
| — | `edit` | `sdb/edit` | → `sdb` | `sdb` |
| `SmtGuiCore` | `gui` | `ui/gui` | → `ui_legacy` | `ui_legacy` |
| `SmtMFCExCore` | `mfc_ex` | `ui/mfc_ex` | → `ui_legacy` | `ui_legacy` |
| `SmtXViewCore` | `xview` | `ui/xview` | → `ui_legacy` | `ui_legacy` |
| `SmtXCatalogCore` | `xcatalog` | `ui/xcatalog` | → `ui_legacy` | `ui_legacy` |
| `SmtXAMBoxCore` | `xambox` | `ui/xambox` | → `ui_legacy` | `ui_legacy` |
| — | `views` | `ui/views` | `views` (`//:ui_views`) | — (source_set) |
| — | `skia` | `render/skia` | → `render` / Views stub | `render` / source_set |
| `SmtAuxModule` | `plugin` | `plugin` | `plugin` | `plugin` |
| `SmtAM3DModelCreater` | `plugin_model3d` | `plugin/model3d` | `plugin_model3d` | `plugin_model3d` |
| `SmtAMOrthogrid` | `plugin_orthogrid` | `plugin/orthogrid` | `plugin_orthogrid` | `plugin_orthogrid` |
| `SmtAMDemCreater` | `plugin_dem` | `plugin/dem` | `plugin_dem` | `plugin_dem` |
| `SmtAMMapPrint` | `plugin_print` | `plugin/print` | `plugin_print` | `plugin_print` |
| `SmtAMMapProject` | `plugin_proj` | `plugin/proj` | `plugin_proj` | `plugin_proj` |
| `SmartGis` | `app` | `app` | `app` | `SmartGis.exe` |
| `SmtAppCore` | `app_core` | `app/app_core` | `app_core` | `app_core` |
| — | `views` (exe) | `app/views` | `views` | `SmartGisViews.exe` |
| — | `winui` | `app/winui` | `app_winui` | `SmartGisWinui.exe` |
| `SmtTinMesh` | `tin` | `algorithm/tin` | → `algorithm` | `algorithm` |
| `Smt3DBaseLib` | `scene3d` | `legacy_render/scene3d` | → `legacy_render` | `legacy_render` |
| — | `scene` | `render/scene` | → `render` | `render` |
| — | `model` / `scene` / `tile` | `sdb/model`, `sdb/scene`, `sdb/tile` | → `sdb` | `sdb` |
| `Smt3DMdLib` | `model3d` | `legacy_render/model3d` | → `legacy_render` | `legacy_render` |
| `Smt3DPointCloud` | `pointcloud` | `legacy_render/pointcloud` | → `legacy_render` | `legacy_render` |
| `Smt3DTerrain` | `terrain` | `legacy_render/terrain` | → `legacy_render` | `legacy_render` |
| `SmtNetCore` | `net` | `net/{pack,http,rpc}` | → `base` | `base` |
| `SmtStaCore` | `stat` | `algorithm/stat` | → `algorithm` | `algorithm` |
| `SmtStaDiagram` | `stat_chart` | `ui/chart` | → `ui_legacy` | `ui_legacy` |

`app` 不进默认 `src_all`。Debug 文件名为 `{stem}_d.dll`。完整对照见 [`abi-rename-map.md`](abi-rename-map.md)。

Repo-root `//core:core` aliases `//src/base:core`; `//core:core_all` still aliases `//src:src_all`. Neither is the product source tree.

Include dirs in `//build:smt_legacy` still point at **each leftover module root** (quoted `#include "header.h"`). New trees (`net`, `content`, `sdb/scene`, …) use `"layer/module/file.h"` via `//src`.

## File naming (mgis / Chromium)

| Tree | Stem | Extension | Include |
| --- | --- | --- | --- |
| New (`content`, `gpu`, `app/{views,winui}`, `ui/views`, `render/{skia,rhi,scene}`, `sdb/{model,scene}`, `net`) | `snake_case` | `.cc` / `.h` (`net` keeps `.cpp`) | `"content/public/map_view.h"`, `"ui/views/view.h"`, `"gpu/gpu.h"`, `"render/rhi/rhi.h"`, `"sdb/scene/scene.h"`, `"net/http/http.h"` (`//src` on the include path) |
| Legacy product (`app` MFC, `ui/{gui,mfc_ex,xview,…}`, `plugin/*`, …) | `snake_case` | keep `.cpp` | still module-root `"main_frame.h"` / `"grid_ctrl.h"` |

- Drop file prefixes (`smt_`, `vw_`, `cata_`, `baog_`, `msvr_`, `am_`, `gt_`, `wa_`, `bl_`, `rd_`, plus module tags `gis_` / `geo_` / `sde_`). On-disk **DLL stems** follow reorg 终态（[`abi-rename-map.md`](abi-rename-map.md)）；legacy `Smt_*` 命名空间仍可能存在直至 ABI cutover 收尾。
- CRT collisions keep a short qualifier (`core_assert.h`, `net_string.h`), not the old prefix.
- `stdafx` / `targetver` / `resource.h` keep those conventional names.

---

**最后更新：** 2026-09-14
