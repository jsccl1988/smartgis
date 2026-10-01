# SmartGIS `src/`

Five locked layers plus plugin / ui. Spatial kernels (`geo` / `proj` / `tin` / `stat`) live under `gis/kernel/` and link in `gis.dll`. Under `gis/`, modules sit in `model/` (feature, layer, map, crs, edit), `datasource/`, `present/` (style, tile), `vista/` (frame, world, assets, domain/atmosphere), and `kernel/`. That grouping is directories only — public namespaces stay two levels (`gis::style`, `geo`, …). Datasource drivers nest as `gis/datasource/<driver>` like QGIS `providers/*`.

GN targets keep short names (`sde_gdal`, `render_gl`). DLL stems stay `Smt*` (`dll_stem`). C++ `Smt_*` ABI is unchanged.

## Five layers

| Layer | Tree | Role |
| --- | --- | --- |
| **app** | `app/{views,winui,cef,cs}`；MFC 壳 → `legacy/app/`（含 `app_core`） | Endgame/prototype hosts only. No `src/shell/`. Namespace `app` (+ `detail`). |
| **content** | `content/public` | Stable map/session/view API. App/UI hosts include only this — not gis model headers or render devices. Local shell tools: `ViewHost` / `LocalToolRouter` (Workspace + EventBus + EditSession); leftover IPC is the OOP adapter. |
| **gis** | `gis/{model/{feature,layer,map,crs,edit},datasource/*,present/{style,tile},vista/{frame,world,assets,domain},kernel/{geo,proj,tin,stat}}` | **GIS 模型层**（不是 literal DB）+ CPU `MapFrame` / World + HTTP XYZ tiles + Style JSON (`gis::style`) + 空间内核（`geo` / `proj` / `tin` / `stat`），同一 `gis.dll`。`domain/atmosphere` 是 `kAtmosphere` 会话包。目录分组不增加公开命名空间。GDAL decorator driver `"SDBD"` (`SdbdDataset` owns stock inner datasets). |
| **render** | `render/{rhi,scene,graph,skia}` | Unified 2D+3D via `render/rhi` (FlyCube DX12/Vulkan), `GpuScene`, frame graph. `gpu/` is the process. Scene math is `base/math`. |
| **effect** | `effect/{map,atmosphere}` | GPU map and atmosphere passes. `source_set` linked by callers, not a DLL. |
| **base** | `base/`（`:foundation` + base DLL leftovers + `math/`） | foundation = log/threading/files/archive/ipc；产品 DLL `dll_stem=base`；style POD → `legacy/gis/present/carto`（`gis` DLL）；`gis::Envelope` → `gis/model/envelope.h`。`base/math`（`//src/base/math:math`、`:bounds`）是场景数学，命名空间仍为 `render`，不进 `base.dll` |

## OSS GIS ↔ this tree

| QGIS / GDAL / GEOS / PROJ | This repo |
| --- | --- |
| `providers/*`, OGR drivers | `src/gis/datasource/{gdal,ogr,sdbd}`（`mem` / `smf` / `ws` 已移除；文件/内存矢量走 GDAL / `SDBD`） |
| `QgsFeature` / `QgsVectorLayer` / `QgsProject` | `OGRFeature` / `OGRLayer` / `SmtMap` (`gis/model/feature` keeps `SmtFeatureType` only) |
| `QgsCoordinateReferenceSystem` | `src/gis/model/crs` (id on the layer); transforms in `gis/kernel/proj` |
| GEOS predicates/ops | Call `OGRGeometry` (`Intersects` / `Buffer` / …); GEOS is inside `//third_party:gdal`. `SmtGeoCore` is TIN/grid/surface meshes only. Delaunay: `src/gis/kernel/tin`. Do not vendor a second GEOS |
| PROJ transforms | `src/gis/kernel/proj` (PROJ 9 adapter only) |
| QgsMapRenderer / canvas | `src/render` + `render/rhi` |
| processing / analysis | `src/gis/kernel/{geo,proj,tin,stat}` (`gis.dll`) |
| `qgis_gui` | `src/ui/views`（终局）；leftover MFC → `src/legacy/ui/` |
| `qgis_app` | `src/app/{views,winui,cef,cs}`；MFC `SmartGis.exe` → `src/legacy/app/` |
| libqgis_core embedder API | `src/content/public` (thin; not all of gis) |
| QgsApplication / settings | `src/base` |
| PDAL / point I/O | future `gis/datasource` driver; `render/pointcloud` is the 3D engine |

## Also

- **gis/kernel/{geo,proj,tin,stat}** — `geo` (`SmtGeoCore`: TIN/grid/surface meshes; OGC types are OGR), proj, tin, stat. Compiled into `gis.dll` (`//src/gis:gis`; `//src/gis:algorithm` forwards there). Scene Vector/Matrix live in `base/math` (Eigen, namespace `render`). Not dem (plugin + GDAL). Not orthogrid (plugin + Eigen Laplace). Chart UI is `legacy/ui/shell/chart`（Views：`ui/views` ChartView）。
- **plugin/** — host `plugin::Registry` + leftover `SmtAuxModule`; shell talks through `content::PluginHost`; Python embed; zip / `plugins.json` store. Spec: `docs/superpowers/specs/2026-09-13-plugin-host-design.md`. Domain children keep leftover `dll_stem`.
- **ui/** — endgame `ui/views` only. Leftover MFC shell (`gui` / `mfc_ex` / `xview` / `xcatalog` / `xambox` / `chart`) → `legacy/ui/`（`dll_stem=ui_legacy`；`//src/ui:ui_legacy` 转发）。
- **legacy/** — leftover trees under `legacy/{app,ui,render,tool}`（见 [`legacy/README.md`](legacy/README.md)）。MFC exe：`build.bat legacy_app` → `//src/legacy/app:app`。Spec: `docs/superpowers/specs/2026-09-14-app-legacy-split-design.md`.
- **tool/** — endgame `//src/tool:dispatch` only (Command / Interaction / Workspace). Leftover `SmtIATool` / `SmtGroupTool` live under `legacy/tool/{iatool,group}/`; optional `//src/legacy/tool:legacy_tool_all`, not in `src_all` by default. Pointer/wheel go only through `ViewHost` / Workspace; leftover tools apply completed drafts (`apply_draft`), they do not own a second Interaction. Live rubber-band is `Interaction::aux_overlay` painted by leftover shell. 3D cameras are `make_view3d_camera`. `flash` start/stop is command-driven; leftover GDI blink honors those commands plus `SET_FLASH_DATA` / mode. Map writes stay on `gis::EditSession`. Domain events: `content::EventBus`. Host composition is `content::ViewHost`. Mapped `GT_MSG_*` / `AM_MSG` execute on the host **and** leftover Notify (product effect / dialogs); unmapped menus do not broadcast. Specs: `docs/superpowers/specs/2026-09-13-tool-event-dispatch-design.md`, `docs/superpowers/archive/specs/2026-09-13-tool-legacy-split-design.md`.
- **net/** — `SmtNetCore`: `pack/` (BinarySink + Pickle), `http/`, `rpc/`, `udp/`. Include `"net/http/http.h"`. No mogu POSIX `net/`. No product web GIS / mapd / WMS stack.
- **gpu/** — `SmartGisRender.exe`
- **sys** — stays beside base

## GDAL seam

`src/gis/datasource/gdal` (`SmtSDEGdalDevice`) registers GDAL and the in-tree **SDBD** driver (`GDALOpenEx("SDBD:MEM:…")` / `SDBD:GPKG:…`). OGR codec / raster scratch live in `datasource/ogr`; remote mogu sdbd (HTTP / FnRPC) and the local SDBD decorator live in `datasource/sdbd`. Layer management is `GDALDataset` / `OGRLayer` / `OGRFeature`, not `SmtDataSource` / `SmtVectorLayer` / `SmtFeature`. Product fields are OGR only — `SmtAttribute`/`SmtField` are removed; MFC att UI reads `OGRLayer`. Product type is `gis::Feature` (`using SmtFeature = Feature` for leftover TUs). Raster scratch: `CreateMemRasLayer` → `OgrRasterLayer` + GDAL **MEM** (`/vsimem` encoded blob；`Open(文件)` 回填 blob). `SmtMemRasLayer` / `SmtMemTileLayer` / `CreateMemTileLayer` removed (`sde_mem` DLL gone). 2D tiles: `src/gis/present/tile` (`TileProvider` + LRU/disk + WMTS parse + `make_xyz_map_layer` via `net::HttpClient` HTTPS; Views `AddBasemapDialog`; not OGR / `SDBD:MEM`). Missing GPKG/PostgreSQL drivers fail Open honestly. No second GDAL tree.

## Model v1 (`gis::model`)

Standalone files go through `load_file` (Assimp when `smt_has_assimp`; otherwise only the built-in `"cube"`). 3D Tiles are an explicit `tileset.json` plus `select_tiles`; content `.gltf` / `.glb` / `.b3dm` is `decode_content` via tinygltf. A `.gltf` file is not a tileset. Leftover `src/legacy/render/scene3d` is not the default loader. World v1 handles: `attach_model` / `attach_tileset` / `attach_terrain` / `attach_pointcloud`.

## RHI v1

`src/render/rhi`: Facade `Device` / `CommandList` / `Buffer` (null + leftover GDI/GL stubs + FlyCube DX12/Vulkan). `bind_camera` takes ortho (2D GIS) or perspective (3D) `CameraMatrices`; FlyCube uploads those as GPU constants and samples uploaded raster/tile textures on DX12. `World::attach_map` keeps `OGRLayer*` (vector) and leftover `SmtLayer*` (raster/tile); `attach_3d_geometry` keeps `OGRGeometry*`. `gis::tessellate_*` turns OGR Point/LineString/Polygon and `Smt3DSurface` into GPU verts. `GpuScene::record` uploads those meshes and issues 2D then 3D on **one** CommandList. Leftover HWND present stays on GDI BitBlt / GL `SwapBuffers` / D3D11 `Present` — the former `leftover_session` / `bind_rhi_present` bridge under `rhi3d/public/bridge` was removed. Debug compiles FlyCube `/MDd` from the junction into `out/flycube`; Release links the MD prebuilt. D3D9/D3DX tree removed. Spec: `docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md`.

## Build

`build.bat` → `//src:src_all` (`SmtGeoCore` is OGR + TIN/grid/surface; scene math is Eigen in `base/math`; no `SmtDemCore`). Hosts: `build.bat app` / `views` / `winui` / `cef` / `cs` / `render`.

---

**最后更新：** 2026-10-01
