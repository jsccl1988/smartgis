<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/` layout (layered)

> **In progress:** full include-path + ABI cutover (mogu-style `#include`, snake_case / 两层命名空间). Spec: [`specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md`](specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md). Map: [`abi-rename-map.md`](abi-rename-map.md).  
> **DLL reorg�?026-10-04）：** 产品 import-link stems �?**`base`** · **`gis`** · **`vista`** · **`render`** · **`net`** · **`content`** · **`ui_views`** · **`tool`** · **`plugin_host`**。探索�?**`scenic`** 不在 `src_all`；app-gated `ui_legacy`；optional `legacy_render` / `legacy_tool` / `scenic_copy_all`；legacy **`*.am` �?LoadLibrary**。产物在 **`out/Debug`** / **`out/Release`**�? 
> **`src/base` foundation Hybrid（Phases 0�? 已收口；真源仅在 `src/`）：** [`../../src/base/`](../../src/base/) �?mogu 对齐 foundation（`//src/base:foundation`，非产品 DLL）。仓库根**�?*物理 `base/`、`core/`；兼容别�?`//:base` / `//:core` 仅在�?`BUILD.gn`。产品平�?DLL **`dll_stem=base`**（`base.dll` / `base_d.dll`�?*不含** net）。Spec: [`specs/2026-09-14-base-root-hybrid-design.md`](specs/2026-09-14-base-root-hybrid-design.md)�?
Product sources for GIS / UI / render stay under **`src/`**. Foundation also lives under **`src/base/`** (`:foundation` source_set). Compatibility forwards are **`//:base`** and **`//:core`** only (no physical repo-root `base/` or `core/`). Directory names under `src/` drop the 2010 `Smt` prefix. Nesting is by layer; **on-disk product DLL** follows the reorg（不再「短名各一 DLL」）�?
**Nesting cap:** at most `src/<layer>/<module>` (two levels under `src/`). **`src/legacy/` is deleted.** Remaining leftover ABI, if any, is `branches/`.

**Dependency direction (locked):** leftover �?product only. Product TUs under `src/` except `src/legacy/` (and `algorithm/`, `testing/`) must not `#include` leftover or GN-dep leftover aggregators (`ui_legacy`, `legacy_render`, `legacy_tool`, `legacy/app`, `branches/`). Agent rule: [`.cursor/rules/repo/legacy-unidirectional.mdc`](../../.cursor/rules/repo/legacy-unidirectional.mdc). Existing reverse edges are strangler debt �?shrink on touch, never add.

**Directory vs DLL:** 目录�?GN 标签路径仍可细（`//src/gis/datasource/gdal:sde_gdal` 等为 group �?�?DLL）。磁�?`dll_stem` 见上表。Debug 产出�?`out/Debug/`，stem 后加 `_d`（`base_d.dll`），不是尾缀 `D`；Release �?`out/Release/`（无 `_d`）�?
New public namespaces stay at most two levels (`geo`, `base::detail` for internals).

## Five layers (locked)

| Layer | Tree | Notes |
| --- | --- | --- |
| app | `src/app/{views,ui_designer,winui,cef,cs}`；`ui_designer/{app,shell,canvas,document,io,llm}`；Views �?peers：`app/views/{app,browser,ui,il.runtime,util}`（`browser/plugin/` = PluginShell / present / playback / preview / `report_suite`；`app/startup/` = LaunchPolicy + scenario registry；`il.runtime/{bind,frontend,codegen,backend,ir}` �?Interact Language；`ir` �?`backend` 同为 `document` / `view` / `plugin` / `horizon`；`:lower` 不依�?`:backend`；`:il.runtime` 别名 `:driver`；product GIS showcase tests compile from `src/plugin/product/<pkg>/scenario/` `*_harness`；HWND/BMP in `il.runtime/backend/view/{pixel,dib,host,present,shot,browse}`；`src/app/views/harness/` **deleted**）；MFC �?�?leftover | Endgame/prototype hosts only. Product runtime gate is `testing/tools` (`loop_runner --gate`), not `testing/e2e`. Do not add a separate browser-shell tree. Namespace `app`. |
| content | `src/content/public`�? 个头�? `{app,browser,renderer,view,embed,common}` | 嵌入方只包含 `public/`：`types`、`map_contents`（含视口�?`GisContentsObserver`）、`event_bus`（含领域事件）、`tool_session`、`plugin_host`、`gis_document`。管道帧�?`common/host_protocol.h`�?|
| sdb / gis | `src/gis/{feature,map,edit,datasource/{session,provider,pipeline,ogr,sdbd,gdal},style/{document,eval,symbol},tile/{protocol,cache,provider,layer},geo/{ops,proj,tin,grid},stat,analysis}` + leftover `legacy/gis/{layer,feature,datasource,present/carto}` | GIS model flattened (no `gis/model/`, no one-file `layer/`/`crs/`); **`gis::Feature` geometry is `OGRwkbGeometryType`**; leftover catalog `FeatureType` (Anno/Grid/Tin/ChildImage) stays leftover. Style JSON + tiles under **`gis/style`** / **`gis/tile`** (no product `gis/carto/` grouping dir); **no product `gis/present`**. CPU MapFrame / World / GPU passes �?**`vista`**. gis �?vista. |
| vista | `src/vista/{component/{map,world},pass/{map,world},assets,mesh,terrain,domain}` | CPU `MapIR` (`vista/component/map`) / `vista::World` + `Instance` (`vista/component/world`：`space/` · `instance/` · `terrain/` · `pointcloud/`) / CPU `vista::atmosphere` (`vista/component/world/atmosphere`) + GPU `MapPass` (`vista/pass/map`) / `WorldPass` (`vista/pass/world`) / `AtmosphereFrame` (`vista/pass/world/atmosphere`) in **`vista.dll`**. Leftover `legacy/gis/vista` adapters compile here (`VISTA_EXPORT`). **IR/Pass lanes:** [`specs/2026-09-13-render-rhi-scene-design.md`](specs/2026-09-13-render-rhi-scene-design.md) **§Vista IR/Pass lanes** · [`diagrams/vista-subdirectory-layers.html`](diagrams/vista-subdirectory-layers.html) · [`../../src/vista/README.md`](../../src/vista/README.md). No orphan `src/gis/vista/` / `src/effect/` trees. |
| render | `src/render/{rhi,graph,programs,…}` | Endgame: unified 2D+3D RHI (Vista DX12/Vulkan) + frame graph. **No** `render/scene` �?`WorldPass` lives under `vista/pass/world` inside **vista.dll**. Scene math in `src/base/math`. Exploratory **Scenic** `src/scenic/{engine,render,scene3d}` / `scenic.dll` is **not** in `src_all` (`ninja scenic`). Frozen leftover `src/legacy/render` + opt-in copy internals `scenic_copy_all` are **not** in `src_all`. **Paint runs in `--type=gpu`**, not in browser. |
| base | `src/base/`（foundation only�? `src/legacy/{core,sys}` | **`//src/base:foundation`**：mogu �?log/threading/files/�?+ `archive`/`ipc`�?*产品 DLL** **`dll_stem=base`**（leftovers+sys�?*不含** net / carto；XML �?`//third_party:pugixml`）。HTTP/RPC �?**`//src/net:net`**。style POD �?`legacy/gis/present/carto`（`gis` DLL）；`gis::Envelope` in `gis/envelope.h`�?|

## OSS GIS �?this tree

| QGIS / GDAL / GEOS / PROJ | This repo |
| --- | --- |
| `providers/*`, OGR drivers | `src/sdb/datasource/gdal`（`mem` / `smf` / `ws` 已移除） |
| `QgsFeature` / `QgsMapLayer` / `QgsProject` | `src/sdb/feature`, `layer`, `map` |
| `QgsCoordinateReferenceSystem` | `src/sdb/crs`; transforms in `gis/geo/proj` |
| GEOS | `src/gis/geo`（`//src/gis:geom` �?`gis` DLL）�?OGR owns OGC types; `geo::Grid` is a 2D computation buffer; wrap `//third_party:gdal` (`geos_c`); no second GEOS vendor |
| PROJ | `src/gis/geo/proj`（PROJ 9 adapter；[`README`](../../src/gis/geo/proj/README.md)�?|
| map canvas renderer | `src/render` + RHI |
| processing / analysis | `src/gis/analysis/{ops,geometry,network,raster/{dem,filter,mask}}` + `src/gis/{geo/{mesh,ops,proj,tin,grid},stat}` (`gis` DLL). Product face: `plugin::run_builtin_op` (thin forward). Vista terrain bake calls `horn_lambert_shade_grid` / `fill_ring_mask`. |
| `qgis_gui` / `qgis_app` | `src/ui/` / `src/app/` |
| libqgis_core for embedders | `src/content/public` |

`gis/datasource/gdal` is `GdalDevice` (OGR codec in `gis/datasource/ogr`, SDBD in `gis/datasource/sdbd/`): a decorator `GDALDriver` `"SDBD"` whose `SdbdDataset` owns a stock inner `GDALDataset` (Memory / GPKG / PostgreSQL / file). Callers use `GDALOpenEx("SDBD:�?)` and may `dynamic_cast` to `SdbdDataset` / `SdbdLayer`. **Remote mogu sdbd** is the only HTTP contract: `PROVIDER_SDBD` + `SdbdClient` (`sdbd_client.*`; HTTP `/api/v1/sdbd/*` + FnRPC `sdbd.*` on `:9032`), `SdbdRemoteDataset`, JSON via `sdbd_json.*`. Process-local `SdbdHandler` and `/sdbd/api/v1/*` are **removed**. Do not patch `third_party/.src/gdal` or resurrect `OgrDataSource`. I/O types are `GDALDataset` / `OGRLayer` / `OGRFeature`；产�?ABI �?`gis::Feature` / `gis::MapLayer`（组合持�?OGR）。栅格草稿：`CreateMemRasLayer` �?`OgrRasterLayer` + GDAL **MEM**（编�?blob �?`/vsimem`；`Open(文件)` 会回�?blob �?`GetRasterNoClone`）；`SmtMemRasLayer` 已移除。`CreateMemTileLayer` / `SmtMemTileLayer` / `sde_mem` 已切除�?D 瓦片：`src/gis/tile/{protocol,cache,provider,layer}`（`TileProvider` + LRU/磁盘缓存 + WMTS 最小解�?+ `make_xyz_map_layer` / Views `AddBasemapDialog`；HTTP(S) �?net+OpenSSL；不�?`SDBD:MEM`）。`SmtAttribute`/`SmtField` 已删除（字段只走 OGR）；MFC att-struct UI �?�?`OGRLayer`。产品类型是 `gis::Feature`（leftover TU �?leftover `SmtFeature`）�?
## DLL 粒度（as-built 2026-09-28�?
| `dll_stem` | �?/ 吸收 | 默认 `src_all` / 宿主 |
| --- | --- | --- |
| **`base`** | leftovers + sys。foundation（`archive`/`ipc`）`:foundation` �?DLL。XML �?pugixml（非内嵌 TinyXML）。carto POD �?`gis` | yes |
| **`net`** | `src/net`（HTTP/RPC；从 base 抽出�?| yes |
| `gis` | `src/gis/**`（model / datasource / style / tile / geo / analysis�?*不含** vista�?| yes |
| `vista` | `src/vista/**` + leftover `legacy/gis/vista` adapters | yes |
| `scenic` | `src/scenic` public façade (`scenic.dll`) | **no**（exploratory；`ninja scenic`�?|
| `render` | `render/{rhi,graph,…}`（endgame only；`WorldPass` �?`vista/pass/world` �?`vista.dll`�?| yes |
| `content` | `src/content/**` | yes |
| `tool` | `src/tool/**`（`TOOL_*`；`:dispatch` 转发�?| yes |
| `ui_views` | `ui/views` + `ui/gfx` + `ui/gis`（同 PE；`UI_EXPORT`�?| **no**（SmartGIS.exe / plugin_host�?|
| `plugin_host` | `plugin/runtime/host` + widgets（`PLUGIN_HOST_*`�?| **no**（Views 宿主�?|
| `ui_legacy` | `legacy/ui/{shell/{ambox,chart},map,inspect/{host,sys,edit},catalog/{tree,ds,map,scene},dialogs/{toolkit,gis,detail},widgets/{feature_pack,prop,dll}}` + `res/` | **no**（`build_app`�?|
| `app_core` | `legacy/app/bootstrap/bootstrap.cpp` | **no**（`build_app`�?|
| `legacy_render` | `legacy/render/**`（冻�?dual-run；Scenic 工作副本�?`src/scenic`�?| **no**（optional�?|
| `legacy_tool` | `legacy/tool/**`（非产品 `tool.dll`�?| **no**（optional�?|
| `plugin_*` / `plugin` | 域插�?/ leftover AuxModule；`*.am` LoadLibrary | 按需 |

仍为 **source_set**（有意）：产�?`*_views` / `processing_views`、`gpu`、`base/math`、legacy `adapter`。vista GPU 已并�?**`vista.dll`**。gpu �?deps `//src/ui/gfx:gfx_headers`（不�?`ui_views`）�?
## Layering plan (directory)

| Layer | Tree | Directory / product notes | In `src_all` |
| --- | --- | --- | --- |
| Foundation | `src/base/`（含 `archive`/`ipc`；core leftovers �?`legacy/core`�? `legacy/{core,sys}` | **`//src/base:foundation`**（static）�?*产品 DLL**：`dll_stem=base`（无 net；无 carto）。XML �?`//third_party:pugixml`（`legacy/xml` 目录已删�?| yes �?`base`；foundation �?deps 链入 |
| Net | `src/net` | **`dll_stem=net`**（import-link�?| yes �?`net` |
| Core data model | `src/gis/{feature,map,edit,style/{document,eval,symbol},tile}` | GIS 模型 + TileProvider / StyleDocument；leftover carto POD 仍在 `legacy/gis/present/carto`�?*一 DLL `gis`**；`gis::Envelope` 头在 `gis/envelope.h`（header-only）。CPU MapFrame / World �?**`src/vista`**（`vista.dll`）�?*方案�?* leftover catalog ABI �?`legacy/gis`；产�?`LayerType`/`VectorSchema` �?`gis/map/layer_kind.h`。Style 根上保留 leftover 钉死�?`paint_resolve.h`�?| yes �?`gis` |
| Vista | `src/vista/{component/{map,world},pass/{map,world},assets,mesh,terrain,domain}` | CPU `MapIR` / `World` + `Instance` / `vista::atmosphere` + GPU `MapPass` / `WorldPass` / `AtmosphereFrame`；leftover `legacy/gis/vista` 编进�?DLL。gis �?vista。`assets` / `mesh` / `terrain` 不依�?`world`。Include `"vista/<module>/�?`。目录以 **§Vista IR/Pass lanes** 为准；`component/world` 顶层�?**§Vista world component layers** 为准�?| yes �?`vista` |
| Datasource | `gis/datasource/{session,provider,pipeline,ogr,sdbd,gdal}` | 并入 `gis` DLL�?*分层�?* session �?provider；backends �?`ogr/` `sdbd/` `gdal/`（无 `provider/impl/`）。产�?ABI `MapLayer`/`Feature`�?*遗留 catalog�?* `legacy/gis/datasource`。SDBD：`sdbd/{sdbd_client,sdbd_driver,sdbd_remote_dataset,sdbd_json}`；活体硬测：`sdbd_live_test.cc` | yes �?`gis` |
| Algorithm | `gis/{geo/{mesh,ops,proj,tin,grid},stat}` + `gis/analysis/{ops,geometry,network,raster/{dem,filter,mask}}` | 编进 **`gis.dll`**（`//src/gis:algorithm` 转发）。`geo` = OGR 封装/几何 + CRS + TIN 与结构化格网光滑；`analysis/ops` = native GeoJSON runners；`geometry` / `network` / `raster` = 内核落点（`raster` �?`dem` / `filter` / `mask` 拆分）。Vista 地形烘焙调用 `horn_lambert_shade_grid` �?`fill_ring_mask`。Scene Vector/Matrix �?`base/math`（命名空�?`base`，`render::` 过渡别名；不�?`base.dll`）。orthogrid 插件只做边界/热力/写出，求解走 `geo::solve_*`。chart 仍在 `ui_legacy` | yes �?`gis` |
| Render | `render/{rhi,graph,…}` | Endgame **一 DLL `render`**。`WorldPass` �?`vista/pass/world`�?*vista.dll**）。场景数学在 `src/base/math`（不进本 DLL）。探索�?**Scenic** `src/scenic/{engine,render/{rhi2d,rhi3d},scene3d}` / `scenic.dll` **不在** `src_all`�?D map paint �?`scenic/render/rhi2d/.../paint/{map,carto}`；`scene3d`↔`vista/component/world` + `vista/pass/world`�?*禁止**下沉 `rhi3d/impl/common`）。leftover `legacy/render/` **冻结**；copy internals `scenic_copy_all` optional | yes �?`render`；scenic/copy/leftover optional |
| Plugin | `plugin/runtime`（host `{catalog,native,capability,processing,present,ui}`（§runtime/host subdirectory tighten�?/ web / processing forwarders / widgets / **python**（embed + `samples/{hello,analysis,product_orchestrate,industry_pack}`））+ `plugin/product`（world3d / traffic / flood / map2d / …；包内 `manifest`�?*map2d** �?`scenario/` GIS (`map2d_scenario`) + `scenario/` HWND BMP (`map2d_harness`，仅 exe/test，不�?native DLL) + `print/`；world3d �?`scene/{dem,orthogrid,hexgrid,earth,model,pointcloud,look,fly,atmosphere,present,detail}` + `scenario/`（无 `showcase/` 层；�?`atmosphere/{capture,common,present,seed,session}`）�?| Host **`dll_stem=plugin_host`**（`//src/plugin:host`；`PLUGIN_HOST_*`）。Processing **kernels** �?`gis/analysis`，`plugin/runtime/processing` 仅转�?`plugin::`。Leftover **`runtime/{auxmodule,bridge}`**：AuxModule �?`//src/legacy/plugin:plugin`；header-only `AM_MSG` �?`//src/legacy/plugin/runtime:cmd`（`//src/plugin:cmd`）；`*.am` scan �?`:bridge`（仅链入 `plugin_host`，忌�?`ui_legacy`）。MFC 域插�?`am_plugin=true` �?**`out/plugin/<stem>[_d].am`**；域�?`shell/` + `views/`。Views �?builtin + import-link `plugin_host`，不�?`*.am`。world3d 命令分层：`commands.h` 门面 + `scene/register`（dem/ortho/hex + earth/model/pointcloud）。包约定�?[`specs/2026-09-13-plugin-host-design.md`](specs/2026-09-13-plugin-host-design.md) § Product domain package�?| host DLL + widgets；域插件按需 |
| UI (leftover) | `legacy/ui/{shell/{ambox,chart},map,inspect/{host,sys,edit},catalog/{tree,ds,map,scene},dialogs/{toolkit,gis,detail},widgets/{feature_pack,prop,dll}}` + `res/{shell,shell/ambox,shell/chart,…}/` | **一 DLL `ui_legacy`**。B1 �?`ui/gis` + `views/map` 词汇；�?1c Feature Pack（无 `grid/`/`dock/`）；§11d widgets；�?1e map；�?1f shell；�?1h inspect；�?1i dialogs；scheme C；不�?`legacy/` | **no**（`build_app`�?|
| UI toolkit (endgame) | `ui/views`（含 `map/{viewport,input,chrome,device}`；根�?`map_viewport.h`/`touch_multitouch.h` 为公共转发）+ `ui/gfx` + `ui/gis` | **`dll_stem=ui_views`**（同 PE；`UI_EXPORT`）；`:views` / `:gfx` / `:gis` 转发；`:gfx_headers` �?gpu | **no**（SmartGIS.exe�?|
| Hosted map | `content/public` + `content/{app,browser,renderer,view,common,embed}`。`content/browser/` **C10 as-built** 12 siblings `{bootstrap,contents,catalog,attrs,plugin,session,document,camera,present,input,capability,debug}`（无根上 TU）�?*C11 目标**（living **§Content browser subdirectory tighten**）：`{contents,session,document,camera,present,capability,debug}` �?DLL public 实现并入 `contents/`；手势并�?`session/`；`document/` helpers 拍平。Present �?`present/{host,map2d/{frame,gpu,software},scene3d/{session,frame,atmosphere,gpu,software}}`。`//src/content:map_session` **�?*�?content.dll（`scene3d_rhi_session` 例外）。GDI �?`present/*/software/`；GPU �?`*/gpu/`；render �?content。模�?README：[`../../src/content/README.md`](../../src/content/README.md) · 分层图：[`diagrams/content-browser-layers.html`](diagrams/content-browser-layers.html) | **`dll_stem=content`**（管�?/ GisContents / ToolSession）；Views 另链 `:map_session` | yes �?`content`（DLL）；map_session �?Views/exe |
| GPU main (`--type=gpu`) | `gpu/` | �?PE `GpuMain`；deps **`:gfx_headers`**（不�?ui_views）。`build.bat render` �?GPU 进程别名�?| **no** |
| App (endgame) | `app/{views,winui,cef,cs}` | SmartGIS.exe import-link 产品 DLL 集；Interact Language �?**`app/views/il.runtime`** | **no** |
| App (leftover) | `legacy/app/{bootstrap,shell/{frame,catalog,dock,showcase},views/{document,viewport,helper}}` + �?`.rc`/`stdafx`/`res/`（`bootstrap/bootstrap.cpp` �?`dll_stem=app_core`�?| MFC `SmartGIS-Legacy.exe`（`//src/legacy/app:app`�?| **no**（`build_app`�?|
| Tool | `tool/{command,interaction,draft,nav,workspace}` + `gis/edit`；`GT_MSG` bridge at `legacy/tool/msg` | **`dll_stem=tool`**（`:dispatch`→`:tool`）；adapter source_set；leftover �?`legacy_tool`；`edit` �?`gis` | yes �?`tool` |

**Deliberately not merged** *(directory / product splits �?DLL 已按上表合并)*

- `//src/base:core` / `:base` / `:platform` / `//src/legacy/sys:sys` �?**group �?`base` DLL**。`//src/net:net` �?*独立**产品 DLL�?*不要**�?`//src/base:foundation`（或 `//:base` / `//:core`）混淆�?- Homemade Vector/Matrix were removed from `gis/geo`. Scene `Vector3` / `Matrix` / bounds live in `src/base/math`（namespace `base`，分层见 `math/{scalar,linear,traits,geom,xform,simd}`；不�?`base.dll` / `render.dll`）�?- `src/legacy/gis/present/carto/`（`Style` / StyleManager POD）链�?**gis.dll**。`gis::Envelope` �?`gis/envelope.h`。Style JSON �?`gis/style`�?- `geo::geometry_traits` / `vector_traits` / �?`indexed_tin` wrap OGR + mesh（`Grid` 缓冲）�?D hex **不在** `gis/geo`：`plugin/product/world3d/hexgrid` `HexLattice`（`OGRMultiPoint` XYZ + nx/ny/nz）。Delaunay in `gis/geo/tin`；structured elliptic in `gis/geo/grid`；Feature/WKB �?datasource；vista `tessellate_*` 消费 mesh。No second geometry tree. **几何体（`ops/` envelope / traits）不反向依赖算法实现（`tin/` / `grid/` / `proj/` / analysis�?* �?living：algorithm-layer-oss **§2–�? / P5**。图（主）：`docs/superpowers/diagrams/gis-geo-layers.html` ·（补）：`gis-algorithm-geometry-split.html`�?- **不要**�?Scenic / `legacy_render` 并进 `render` �?`vista.dll`�?*不要**�?`scenic` / `scenic_copy_all` 塞进 `src_all`�?*不要**把域插件并进平台 DLL�?*不要**�?`ui_views` 拆成 `gfx.dll`�?- MFC Feature Pack / `ui_legacy` stays out of default `src_all`。日常产品入口：`build.bat app` �?Views�?
### Desktop UI endgame (Views + Skia)

Chosen destination: Chromium-style **Views** + **Skia** + existing C++ map viewport. Tree: `src/ui/views`, `src/ui/gfx` (gfx public dirs: `geometry/` · `color/` · `canvas/` · `display_list/` · `raster/` · `image/` · `font/` · `animation/`). Doc: [`ui-views-skia.md`](ui-views-skia.md). Feature Pack, WinUI, and WebView2 are **not** the endgame. Qt is banned.

### MFC Feature Pack (legacy exe bootstrap)

`build.bat legacy_app` links **MFC Feature Pack** (`CMFCRibbonBar` / `CDockablePane` / `CMDIFrameWndEx` via `legacy/ui/widgets/feature_pack/feature_pack.h`). BCGControlBar Pro is **not** required and is not vendored. This is a compile bridge, **not** the destination toolkit (Views + Skia). `build.bat app` builds Views.

| Dep | How to satisfy |
| --- | --- |
| **MFC** (MBCS, `afxwin.h` / `afxres.h` / `afxcontrolbars.h`) | VS 18 Individual component **C++ MFC for x64/x86 (Latest MSVC)** = `Microsoft.VisualStudio.Component.VC.ATLMFC`, or toolset-pinned `Microsoft.VisualStudio.Component.VC.14.50.18.0.MFC`. After install, re-run `build.bat legacy_app` so `out/environment.x64.x64` picks up `atlmfc\include` + `atlmfc\lib\x64`. Close `cl`/`ninja`/`link` first, or the installer precheck `VSProcessesRunning` cancels (error `0x1f46`). |

## Path map (2010 dir �?short name �?layered �?终�?DLL)

| Old directory | Short (`src/…`) | Layered (`src/…`) | GN target | 终�?`dll_stem` |
| --- | --- | --- | --- | --- |
| `SmtCore` | `core` | `legacy/core/{macros,struct,api,listener,command,msg,diag}`（foundation 头仍�?`base/core`�?| `legacy/core` �?`//src/base:base` | `base` |
| `SmtSysCore` | `sys` | `sys` | `sys` �?`//src/base:base` | `base` |
| `SmtMathLib` | `math` | `algorithm/math` | (absorbed into geo) | `gis` |
| `Smt3DMathLib` | `math3d` | `algorithm/math3d` | (absorbed) | `gis` |
| `SmtBaseLib` | `base` | `legacy/gis/present/carto`（style POD�? `gis/envelope.h` | `carto_sources` �?`//src/gis:gis`；Envelope header-only | `gis` |
| `SmtGeoCore` | `geo` | `gis/geo` | `geo` �?`//src/gis:gis` | `gis` |
| `Smt3DGeoCore` | `geo3d` | `gis/geo` | (absorbed) | `gis` |
| `SmtGisCore` | `gis` | `sdb/{feature,layer,map}` | `gis` �?`//src/sdb:sdb` | `sdb` |
| `SmtGisPrj` | `proj` | `gis/geo/proj` | `proj` �?`//src/gis:gis` | `gis` |
| `SmtRender` | `render` | `render` / leftover bridge | endgame �?`render`；bridge �?`legacy_render` | `render` / `legacy_render` |
| `Smt3DRenderer` | `render3d` | `legacy/render/rhi3d` (was `render3d`) | �?`legacy_render` | `legacy_render` |
| `SmtGdiRenderDevice` | `render_gdi` | `legacy/render/rhi2d/impl/gdi` | �?`legacy_render` | `legacy_render` |
| `SmtGdiSimpleRenderDevice` *(retired)* | *(was `render_gdi_simple`)* | �?| Alias �?`CreateRenderDevice` / `gdi/` | `legacy_render` |
| `SmtGLRenderDevice` | `render_gl` | `legacy/render/rhi3d/impl/gl` (was `legacy/render/gl`) | �?`legacy_render` | `legacy_render` |
| `SmtD3DRenderDevice` | `render_d3d` | *(removed)* | �?| �?|
| `SmtSDEDeviceMgr` | `sde_mgr` | `legacy/gis/datasource` | �?`gis` | `gis` |
| `GdalDevice` | `sde_gdal` | `sdb/datasource/gdal` | �?`sdb` | `sdb` |
| `SmtSDEMemDevice` | `sde_mem` | *(removed)* | �?| �?|
| `SmtSDESmfDevice` | `sde_smf` | *(removed)* | �?| �?|
| `SmtSDEWSDevice` | `sde_ws` | *(removed)* | �?| �?|
| `SmtToolCore` | `tool` | `legacy_tool` | �?`legacy_tool` | `legacy_tool` |
| `SmtGroupToolCore` | `tool_group` | `legacy/tool/{nav,select,draft,base,factory}` | sources �?`ui_legacy`（避环） | `ui_legacy` |
| �?| `dispatch` | `tool/{command,interaction,draft,nav,workspace}` | `dispatch` | �?(group→source_sets) |
| �?| `tool_adapter` | `legacy/tool/msg` | `//src/legacy/tool/msg:adapter` | �?(source_set；`namespace tool`) |
| �?| `edit` | `sdb/edit` | �?`sdb` | `sdb` |
| `SmtGuiCore` | `dialogs` + `inspect` | `legacy/ui/dialogs/{toolkit,gis,detail}` + `legacy/ui/inspect/{host,sys,edit}` | �?`ui_legacy` | `ui_legacy` |
| `SmtMFCExCore` | `widgets` | `legacy/ui/widgets/{feature_pack,prop,dll}`（Feature Pack glue；Catalog dock 内联�?`legacy/app/shell/catalog`�?| �?`ui_legacy` | `ui_legacy` |
| `SmtXViewCore` | `shell`+`map` | `legacy/ui/shell` · `legacy/ui/map` | �?`ui_legacy` | `ui_legacy` |
| `SmtXCatalogCore` | `catalog` | `legacy/ui/catalog/{tree,ds,map,scene}` | �?`ui_legacy` | `ui_legacy` |
| `SmtXAMBoxCore` | `shell/ambox` | `legacy/ui/shell/ambox` | �?`ui_legacy` | `ui_legacy` |
| �?| `views` | `ui/views` | `views` (`//:ui_views`) | �?(source_set) |
| �?| `gfx` | `ui/gfx` | `//src/ui/gfx:gfx` | �?(source_set) |
| `SmtAuxModule` | `plugin` | `legacy/plugin/runtime/auxmodule` | `plugin` | `plugin` |
| `SmtAM3DModelCreater` | `plugin_model3d` | `legacy/plugin/product/model3d` | `plugin_model3d` | `plugin_model3d` |
| `SmtAMOrthogrid` | `plugin_orthogrid` | `legacy/plugin/product/orthogrid` | `plugin_orthogrid` | `plugin_orthogrid` |
| `SmtAMDemCreater` | `plugin_dem` | `legacy/plugin/product/dem` | `plugin_dem` | `plugin_dem` |
| `SmtAMMapPrint` | `plugin_print` | `legacy/plugin/product/print` | `plugin_print` | `plugin_print` |
| `SmtAMMapProject` | `plugin_proj` | `legacy/plugin/product/proj` | `plugin_proj` | `plugin_proj` |
| `SmartGis` | `app` | `app` | `app` | `SmartGIS-Legacy.exe` |
| `SmtAppCore` | `app_core` | `app/app_core` | `app_core` | `app_core` |
| �?| `views` (exe) | `app/views` (`{app/{host,process,cmdline,startup},browser,ui,harness,il.runtime,util}`) + content `document` / `camera` / `present/{host,map2d,scene3d/{session,frame,atmosphere,gpu,software}}` / `input` | `views` | `SmartGIS.exe` |
| �?| `winui` | `app/winui` | `app_winui` | `SmartGisWinui.exe` |
| �?| `cef` | `app/cef` | `cef` | `SmartGisCef.exe` |
| `SmtTinMesh` | `tin` | `gis/geo/tin` | �?`//src/gis:gis` | `gis` |
| `Smt3DBaseLib` | `scene3d` | `legacy/render/scene3d` | �?`legacy_render` | `legacy_render` |
| �?| `scene` | `vista/pass/world` (`WorldPass`) | �?`//src/vista:vista` | `vista` |
| �?| `model` / `scene` / `tile` | `sdb/model`, `sdb/scene`, `sdb/tile` | �?`sdb` | `sdb` |
| `Smt3DMdLib` | `model3d` | `legacy/render/scene3d/primitive` (was top `model3d/` + scene3d `feature/`) | �?`legacy_render` | `legacy_render` |
| `PointCloud3d` | `pointcloud` | `legacy/render/scene3d/primitive` (was top `pointcloud/` / `surface/`) | �?`legacy_render` | `legacy_render` |
| `Smt3DTerrain` | `terrain` | `legacy/render/scene3d/primitive` (was top `terrain/` / `surface/`) | �?`legacy_render` | `legacy_render` |
| �?| leftover vista | `legacy/gis/vista` (`DemHeightField` / dem→World / Y-up coord) | �?`//src/vista:vista` | `vista` |
| `SmtNetCore` | `net` | `net/{pack,http,rpc}` | �?`base` | `base` |
| `SmtStaCore` | `stat` | `gis/stat` (`StatExpr.g4` �?`$root_gen_dir/g4/gis/stat/cpp`) | �?`//src/gis:gis` | `gis` |
| `SmtStaDiagram` | `stat_chart` | `legacy/ui/shell/chart` | �?`ui_legacy` | `ui_legacy` |

`app` 不进默认 `src_all`。Debug 文件名为 `{stem}_d.dll`。完整对照见 [`abi-rename-map.md`](abi-rename-map.md)�?
`//src/base:foundation` is the mogu-aligned foundation. Root `group("base")` (`//:base`) and `group("core")` (`//:core`) forward to it in `BUILD.gn` only �?**no** physical repo-root `base/` or `core/` directory. `//:core_all` aliases `//src:src_all`. Product platform DLL is `//src/base:base` with **`dll_stem=base`**.

Include dirs: `BUILDCONFIG` puts **`//src` before `//`** so `#include "base/�?` �?`src/base/` only；`//build:legacy` same。Product modules use `"layer/module/file.h"` via `//src`.

## File naming (mgis / Chromium)

| Tree | Stem | Extension | Include |
| --- | --- | --- | --- |
| New (`content`, `gpu`, `app/{views,winui,cef,cs}`, `ui/views`, `render/{skia,rhi,scene}`, `sdb/{model,scene}`, `net`) | `snake_case` | `.cc` / `.h` (`net` keeps `.cpp`) | `"content/public/gis_contents.h"`, `"ui/views/kernel/view.h"`, `"gpu/gpu.h"`, `"render/rhi/rhi.h"`, `"sdb/scene/scene.h"`, `"net/http/http.h"` (`//src` on the include path) |
| Legacy product (`legacy/app` MFC, `legacy/ui/{shell,map,inspect,…}`, `plugin/*`, �? | `snake_case` | keep `.cpp` | `"legacy/app/�?` / `"legacy/ui/�?` |

- Drop file prefixes (`smt_`, `vw_`, `cata_`, `baog_`, `msvr_`, `am_`, `gt_`, `wa_`, `bl_`, `rd_`, plus module tags `gis_` / `geo_` / `sde_`). On-disk **DLL stems** follow reorg 终态（[`abi-rename-map.md`](abi-rename-map.md)）；legacy `Smt_*` 命名空间仍可能存在直�?ABI cutover 收尾�?- CRT collisions keep a short qualifier (`core_assert.h`, `net_string.h`), not the old prefix.
- `stdafx` / `targetver` / `resource.h` keep those conventional names.

---

**最后更新：** 2026-10-06
