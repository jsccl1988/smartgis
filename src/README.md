# SmartGIS `src/`

Five locked layers plus algorithm / plugin / ui. Nesting: `src/<layer>/<module>` (datasource drivers nest as `sdb/datasource/<driver>` like QGIS `providers/*`).

GN targets keep short names (`sde_smf`, `render_gl`). DLL stems stay `Smt*` (`dll_stem`). C++ `Smt_*` ABI is unchanged.

## Five layers

| Layer | Tree | Role |
| --- | --- | --- |
| **app** | `app/`, `app/{app_core,views,webview2,winui}` | Product hosts. No `src/chrome/`. Namespace `app` (+ `detail`). |
| **content** | `content/public` | Stable map/session/view API. App/UI hosts include only this — not sdb or render devices. |
| **sdb** | `sdb/{feature,layer,map,crs,datasource/*,model,scene}` | Spatial DB / GIS model + CPU models / World. GDAL/OGR-extendable. |
| **render** | `render/` + RHI + GPU scene | Unified 2D+3D via `render/rhi` (FlyCube DX12/Vulkan). `gpu/` is the process. |
| **base** | `base/` | Former `core` + envelope/style. Two DLLs: `//src/base:core` (`SmtCore`) and `//src/base:base` (`SmtBaseLib`). |

## OSS GIS ↔ this tree

| QGIS / GDAL / GEOS / PROJ | This repo |
| --- | --- |
| `providers/*`, OGR drivers | `src/sdb/datasource/{ado,mem,smf,ws,gdal}` |
| `QgsFeature` / `QgsVectorLayer` / `QgsProject` | `src/sdb/feature`, `layer`, `map` |
| `QgsCoordinateReferenceSystem` | `src/sdb/crs` (id on the layer); transforms in `algorithm/proj` |
| GEOS predicates/ops | `src/algorithm/geo` (`SmtGeoCore`, `//src/algorithm:geom`); wrap `gdal_sdk` `geos_c`; do not vendor a second GEOS |
| PROJ transforms | `src/algorithm/proj` (PROJ 9 adapter only) |
| QgsMapRenderer / canvas | `src/render` + `render/rhi` |
| processing / analysis | `src/algorithm/` |
| `qgis_gui` | `src/ui/` |
| `qgis_app` | `src/app/` |
| libqgis_core embedder API | `src/content/public` (thin; not all of sdb) |
| QgsApplication / settings | `src/base` |
| WMS/WFS service | `src/web/` (not `sdb/map`) |
| PDAL / point I/O | future `sdb/datasource` driver; `render/pointcloud` is the 3D engine |

## Also

- **algorithm/** — `geo` (`SmtGeoCore`: math + math3d + geo3d sources), proj, tin, baogrid, stat. Not dem (plugin + GDAL). Chart UI is `ui/chart`.
- **plugin/** — domain modules
- **ui/** — legacy MFC (including `ui/chart`) + `ui/views` toolkit
- **tool/** — leftover `SmtIATool` plus `//src/tool:dispatch` (Command / InputRouter / Workspace). Domain events: `content::EventBus`. Document writes: `sdb/edit`. Spec: `docs/superpowers/specs/2026-09-13-tool-event-dispatch-design.md`.
- **web/** — map server/client/cgi
- **net/** — `SmtNetCore`: standalone ASIO sockets + cpp-httplib HTTP; FnRPC later
- **gpu/** — `SmartGisRender.exe`
- **ado** — leftover SmtAdoCore sources (not in `src_all`; DB path is OGR)
- **sys** — stays beside base

## GDAL seam

`src/sdb/datasource/gdal` (`SmtSDEGdalDevice`) is the OGR database provider. Device manager constructs `OgrDataSource` for `DS_DB_ADO`. SMF uses the shared `ogr_codec` for file features. This `gdal_sdk` build often has no GPKG/PostgreSQL driver; file create then falls back to an ESRI Shapefile directory. No second GDAL tree.

## RHI v1

`src/render/rhi`: Facade `Device` / `CommandList` (null + leftover GDI/GL + FlyCube DX12/Vulkan stubs). `SmtRenderDevice::Init(HWND)` calls `BindRhiPresent`. Logical models/World are `sdb/model` + `sdb/scene`; GPU cache is `render/scene`. No D3D9/D3DX. Spec: `docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md`.

## Build

`build.bat` → `//src:src_all` (27 DLLs: math/math3d/geo3d in `SmtGeoCore`; no `SmtDemCore`). Hosts: `build.bat app` / `views` / `web` / `winui` / `render`.

---

**最后更新：** 2026-09-13
