# SmartGIS `src/`

Five locked layers plus algorithm / plugin / ui. Nesting: `src/<layer>/<module>` (datasource drivers nest as `sdb/datasource/<driver>` like QGIS `providers/*`).

GN targets keep short names (`sde_smf`, `render_gl`). DLL stems stay `Smt*` (`dll_stem`). C++ `Smt_*` ABI is unchanged.

## Five layers

| Layer | Tree | Role |
| --- | --- | --- |
| **app** | `app/`, `app/{app_core,views,webview2,winui}` | Product hosts. No `src/chrome/`. Namespace `app` (+ `detail`). |
| **content** | `content/public` | Stable map/session/view API. App/UI hosts include only this — not sdb or render devices. Local chrome tools: `ViewHost` / `LocalToolRouter` (Workspace + EventBus + EditSession); leftover IPC is the OOP adapter. |
| **sdb** | `sdb/{feature,layer,map,crs,datasource/*,model,scene}` | Spatial DB / GIS model + CPU models / World. GDAL/OGR-extendable. |
| **render** | `render/` + RHI + GPU scene | Unified 2D+3D via `render/rhi` (FlyCube DX12/Vulkan). `gpu/` is the process. |
| **base** | `base/` | Former `core` + envelope/style. Two DLLs: `//src/base:core` (`SmtCore`) and `//src/base:base` (`SmtBaseLib`). |

## OSS GIS ↔ this tree

| QGIS / GDAL / GEOS / PROJ | This repo |
| --- | --- |
| `providers/*`, OGR drivers | `src/sdb/datasource/{mem,smf,ws,gdal}` |
| `QgsFeature` / `QgsVectorLayer` / `QgsProject` | `OGRFeature` / `OGRLayer` / `SmtMap` (`sdb/feature` keeps `SmtFeatureType` only) |
| `QgsCoordinateReferenceSystem` | `src/sdb/crs` (id on the layer); transforms in `algorithm/proj` |
| GEOS predicates/ops | `src/algorithm/geo` (`SmtGeoCore`, `//src/algorithm:geom`); wrap `gdal_sdk` `geos_c`; do not vendor a second GEOS |
| PROJ transforms | `src/algorithm/proj` (PROJ 9 adapter only) |
| QgsMapRenderer / canvas | `src/render` + `render/rhi` |
| processing / analysis | `src/algorithm/` |
| `qgis_gui` | `src/ui/` |
| `qgis_app` | `src/app/` |
| libqgis_core embedder API | `src/content/public` (thin; not all of sdb) |
| QgsApplication / settings | `src/base` |
| WMS/WFS service | leftover `src/web/` server (not `sdb/map`) |
| mapd HTTP client | `src/web/mapd` (`web::MapdClient`, `:8020`) |
| PDAL / point I/O | future `sdb/datasource` driver; `render/pointcloud` is the 3D engine |

## Also

- **algorithm/** — `geo` (`SmtGeoCore`: math + math3d + geo3d sources), proj, tin, stat. Not dem (plugin + GDAL). Not orthogrid (plugin + Eigen Laplace). Chart UI is `ui/chart`.
- **plugin/** — host `plugin::Registry` + leftover `SmtAuxModule`; chrome talks through `content::PluginHost`; Python embed; zip / `plugins.json` store. Spec: `docs/superpowers/specs/2026-09-13-plugin-host-design.md`. Domain children keep leftover `dll_stem`.
- **ui/** — legacy MFC (including `ui/chart`) + `ui/views` toolkit
- **tool/** — leftover `SmtIATool` plus `//src/tool:dispatch` (Command / InputRouter / Workspace). Exclusive leftover tools (`select`, `append`, `view`, `view3d`, `input*`) wrap `make_*` Interactions; leftover `view3d` feeds `kKeyDown` / `kWheel` / pick drafts (camera stays leftover). `flash` start/stop is command-driven; leftover GDI blink honors those commands plus `SET_FLASH_DATA` / mode. Map writes stay on `sdb::EditSession`. Domain events: `content::EventBus`. Host composition is `content::ViewHost` (MapSession + leftover xview). Spec: `docs/superpowers/specs/2026-09-13-tool-event-dispatch-design.md`.
- **web/** — leftover map server/client/cgi; **mapd** HTTP client (`web::MapdClient`)
- **net/** — `SmtNetCore`: `pack/` (BinarySink + Pickle), `http/`, `rpc/`, `udp/`. Include `"net/http/http.h"`. No mogu POSIX `net/`.
- **gpu/** — `SmartGisRender.exe`
- **sys** — stays beside base

## GDAL seam

`src/sdb/datasource/gdal` (`SmtSDEGdalDevice`) registers the in-tree **SDBD** GDAL driver (`GDALOpenEx("SDBD:MEM:…")` / `SDBD:GPKG:…`). Layer management is `GDALDataset` / `OGRLayer` / `OGRFeature`, not `SmtDataSource` / `SmtVectorLayer` / `SmtFeature`. Missing GPKG/PostgreSQL drivers fail Open honestly. No second GDAL tree.

## RHI v1

`src/render/rhi`: Facade `Device` / `CommandList` / `Buffer` (null + leftover GDI/GL + FlyCube DX12/Vulkan). `World::attach_map` keeps `OGRLayer*` (vector) and leftover `SmtLayer*` (raster/tile); `attach_3d_geometry` keeps `Smt3DGeometry*`. `sdb::scene::tessellate_*` turns Point/LineString/Polygon and `Smt3DSurface` into GPU verts. `GpuScene::record` uploads those meshes and issues 2D then 3D on **one** CommandList. `SmtRenderDevice::Init(HWND)` calls `BindRhiPresent`. Debug does not link FlyCube. No D3D9/D3DX. Spec: `docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md`.

## Build

`build.bat` → `//src:src_all` (27 DLLs: math/math3d/geo3d in `SmtGeoCore`; no `SmtDemCore`). Hosts: `build.bat app` / `views` / `web` / `winui` / `render`.

---

**最后更新：** 2026-09-13
