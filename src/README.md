# SmartGIS `src/`

Five locked layers plus algorithm / plugin / ui. Nesting: `src/<layer>/<module>` (datasource drivers nest as `sdb/datasource/<driver>` like QGIS `providers/*`).

GN targets keep short names (`sde_smf`, `render_gl`). DLL stems stay `Smt*` (`dll_stem`). C++ `Smt_*` ABI is unchanged.

## Five layers

| Layer | Tree | Role |
| --- | --- | --- |
| **app** | `app/`, `app/{app_core,views,winui}` | Product hosts. No `src/chrome/`. Namespace `app` (+ `detail`). |
| **content** | `content/public` | Stable map/session/view API. App/UI hosts include only this — not sdb or render devices. Local chrome tools: `ViewHost` / `LocalToolRouter` (Workspace + EventBus + EditSession); leftover IPC is the OOP adapter. |
| **sdb** | `sdb/{feature,layer,map,crs,datasource/*,model,scene}` | Spatial DB / GIS model + CPU models / World. GDAL decorator driver `"SDBD"` (`SdbdDataset` owns stock inner datasets). |
| **render** | `render/` + RHI + GPU scene | Unified 2D+3D via `render/rhi` (FlyCube DX12/Vulkan). `gpu/` is the process. |
| **base** | `base/{core,style,ipc}` | `core` = `SmtCore`; `style` = `SmtBaseLib` (cartographic style + Envelope); `ipc` = named pipe + pickle. |

## OSS GIS ↔ this tree

| QGIS / GDAL / GEOS / PROJ | This repo |
| --- | --- |
| `providers/*`, OGR drivers | `src/sdb/datasource/{mem,smf,ws,gdal}` |
| `QgsFeature` / `QgsVectorLayer` / `QgsProject` | `OGRFeature` / `OGRLayer` / `SmtMap` (`sdb/feature` keeps `SmtFeatureType` only) |
| `QgsCoordinateReferenceSystem` | `src/sdb/crs` (id on the layer); transforms in `algorithm/proj` |
| GEOS predicates/ops | Call `OGRGeometry` (`Intersects` / `Buffer` / …); GEOS is inside `//third_party:gdal`. `SmtGeoCore` is TIN/grid/surface meshes only. Delaunay: `src/algorithm/tin`. Do not vendor a second GEOS |
| PROJ transforms | `src/algorithm/proj` (PROJ 9 adapter only) |
| QgsMapRenderer / canvas | `src/render` + `render/rhi` |
| processing / analysis | `src/algorithm/` |
| `qgis_gui` | `src/ui/` |
| `qgis_app` | `src/app/` |
| libqgis_core embedder API | `src/content/public` (thin; not all of sdb) |
| QgsApplication / settings | `src/base` |
| PDAL / point I/O | future `sdb/datasource` driver; `render/pointcloud` is the 3D engine |

## Also

- **algorithm/** — `geo` (`SmtGeoCore`: TIN/grid/surface meshes; OGC types are OGR), proj, tin, stat. Scene Vector/Matrix live in `render/math` (Eigen). Not dem (plugin + GDAL). Not orthogrid (plugin + Eigen Laplace). Chart UI is `ui/chart`.
- **plugin/** — host `plugin::Registry` + leftover `SmtAuxModule`; chrome talks through `content::PluginHost`; Python embed; zip / `plugins.json` store. Spec: `docs/superpowers/specs/2026-09-13-plugin-host-design.md`. Domain children keep leftover `dll_stem`.
- **ui/** — legacy MFC (including `ui/chart`) + `ui/views` toolkit
- **tool/** — leftover `SmtIATool` DLL (LoadLibrary ABI) plus `//src/tool:dispatch` (Command / InputRouter / Workspace). Pointer/wheel go only through `ViewHost` / Workspace; leftover tools apply completed drafts (`apply_draft`), they do not own a second Interaction. Live rubber-band is `Interaction::aux_overlay` painted by leftover chrome. 3D cameras are `make_view3d_camera`. `flash` start/stop is command-driven; leftover GDI blink honors those commands plus `SET_FLASH_DATA` / mode. Map writes stay on `sdb::EditSession`. Domain events: `content::EventBus`. Host composition is `content::ViewHost`. Mapped `GT_MSG_*` / `AM_MSG` execute on the host **and** leftover Notify (product effect / dialogs); unmapped menus do not broadcast. Spec: `docs/superpowers/specs/2026-09-13-tool-event-dispatch-design.md`.
- **net/** — `SmtNetCore`: `pack/` (BinarySink + Pickle), `http/`, `rpc/`, `udp/`. Include `"net/http/http.h"`. No mogu POSIX `net/`. No product web GIS / mapd / WMS stack.
- **gpu/** — `SmartGisRender.exe`
- **sys** — stays beside base

## GDAL seam

`src/sdb/datasource/gdal` (`SmtSDEGdalDevice`) registers the in-tree **SDBD** GDAL driver (`GDALOpenEx("SDBD:MEM:…")` / `SDBD:GPKG:…`). Layer management is `GDALDataset` / `OGRLayer` / `OGRFeature`, not `SmtDataSource` / `SmtVectorLayer` / `SmtFeature`. Missing GPKG/PostgreSQL drivers fail Open honestly. No second GDAL tree.

## Model v1 (`sdb::model`)

Standalone files go through `load_file` (Assimp when `smt_has_assimp`; otherwise only the built-in `"cube"`). 3D Tiles are an explicit `tileset.json` plus `select_tiles`; content `.gltf` / `.glb` / `.b3dm` is `decode_content` via tinygltf. A `.gltf` file is not a tileset. Leftover `src/render/model3d` is not the default loader. World v1 handles: `attach_model` / `attach_tileset` / `attach_terrain` / `attach_pointcloud`.

## RHI v1

`src/render/rhi`: Facade `Device` / `CommandList` / `Buffer` (null + leftover GDI/GL + FlyCube DX12/Vulkan). `bind_camera` takes ortho (2D GIS) or perspective (3D leftover) `CameraMatrices`; FlyCube uploads those as GPU constants and samples uploaded raster/tile textures on DX12. `World::attach_map` keeps `OGRLayer*` (vector) and leftover `SmtLayer*` (raster/tile); `attach_3d_geometry` keeps `OGRGeometry*`. `sdb::scene::tessellate_*` turns OGR Point/LineString/Polygon and `Smt3DSurface` into GPU verts. `GpuScene::record` uploads those meshes and issues 2D then 3D on **one** CommandList. Leftover `SmtVertexBuffer` / `SmtIndexBuffer` (host or GL, ABI unchanged) copy through `render::scene::upload_leftover_buffers` onto the same `Device` and `record_leftover_draw` on the same CommandList. GDI / GDI-simple / GL / SmtRender share one leftover RHI session via `render::scene::leftover_session()` (owned in `SmtRender`). `SmtRenderDevice::Init(HWND)` calls `BindRhiPresent`. Debug compiles FlyCube `/MDd` from the junction into `out/flycube`; Release links the MD prebuilt. D3D9/D3DX tree removed. Spec: `docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md`.

## Build

`build.bat` → `//src:src_all` (`SmtGeoCore` is OGR + TIN/grid/surface; scene math is Eigen in `render/math`; no `SmtDemCore`). Hosts: `build.bat app` / `views` / `winui` / `render`.

---

**最后更新：** 2026-09-13
