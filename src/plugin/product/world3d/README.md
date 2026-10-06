<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# smartgis.world3d

True-3D Earth browse (完整真3D) plus DEM trimesh/heightmap loaders, former `model3d`
scene commands, **2D boundary-adapted orthogrid**, and **3D hex lattice**. Package
id stays `smartgis.world3d` (no parallel `earth3d` / `baogrid` / `orthogrid3d`
product packages). Layout:

| Dir | Responsibility |
| --- | --- |
| `commands.h` / `commands.cc` | Public façade; `register_world3d` wires `grid/` + `scene/` |
| `grid/` | DEM + 2D orthogrid + 3D hex (terrain / structured-mesh generation; not `gis/geo/grid` solvers) |
| `grid/register.*` | `register_world3d_grid` — calls the three domain registers |
| `grid/dem/` | Heightmap/trimesh `loader/` + Views `dialog/` (+ markup) + `tests/` |
| `grid/orthogrid/` | 2D `lattice/` `session/` `solve/` |
| `grid/hexgrid/` | 3D `lattice/` `sample/` `io/` `solve/` |
| `scene/` | True-Earth looks + globe fly + leftover `model3d.*` (`register.cc`) |
| `resources/data/` | Data stub README (copy → `out/plugins/world3d/data`) |
| `detail/contribute.h` | Command/processing alias helper |

Shell includes only `plugin/product/world3d/commands.h`.

## Product face (Google Earth–class)

| Command / processing | Role |
| --- | --- |
| `world3d.open_earth` | Scene3D tab + China DEM framing + atmosphere + **contour suite** (stacked WaveHs TIN / isolines / side color scale) |
| `world3d.load_global_dem` | JSON `{path?}` — custom/global DEM GeoTIFF; empty → resolve defaults then China stand-in |
| `world3d.set_satellite_cloud` | JSON `{path?,enabled?}` — satellite cloud cover field or procedural deck |
| `world3d.set_atmosphere` | JSON `{sky?,ocean?,cloud?,fog?}` — explicit atmosphere pass toggles |
| `world3d.apply_look` | JSON `{mode}` — `land` / `ocean` / `full` / `coast` / `globe` / `legacy` / `east_china` (former `--atmosphere-showcase` looks) |
| `world3d.fly_globe` | JSON `{t?}` — cinematic globe fly-in beat in `[0,1]` (space→clouds→DEM→ocean) |
| `world3d.fly_to` | JSON `{lon,lat,distance?,span_deg?}` — local orbit reframe |
| `world3d.attach_city_tileset` | JSON `{path?}` — 3D Tiles JSON (empty → `m3_city_tileset.json`) |
| `world3d.add_pointcloud` / LAS | Existing pointcloud hook |
| `world3d.trimesh_from_xyz` / `heightmap_from_raster` | DEM surface → MapScene triangles |
| `baogrid.*` / `orthogrid.*` | Same 2D handlers; `detail::contribute_prefixed_commands` |
| `orthogrid3d.*` | 3D hex lattice + `.vts` (`register_world3d_hexgrid`) |

Contour suite entry: `scene/present/contour.*` →
`AtmosphereSession::apply_contour_suite_defaults()` (also on China Scene3D
product seed / plugin-showcase world3d).

Resources copy to `out/plugins/world3d/`. Shell `--plugins-dir` defaults to
`<exe>/../plugins`.

## Data paths

| Asset | Preferred locations (exe is `out/<cfg>/`) |
| --- | --- |
| Global DEM | `out/data/global_dem.tif`, `out/plugins/world3d/data/global_dem.tif` |
| Global terrain albedo | `out/data/global_terrain.tif` (build: `py -3 testing/data/build_globe_terrain.py [--download-blue-marble]`) |
| China DEM (shipped sample) | `out/data/china_dem.tif` |
| Satellite cloud cover | `out/data/satellite_cloud.tif`, `out/plugins/world3d/data/satellite_cloud.tif` (single-band → `cloud_cover`) |
| City 3D Tiles fixture | `out/data/m3_city_tileset.json` |

Missing global DEM / satellite GeoTIFF is **not** a hard failure: commands
return structured JSON (`source":"china_standin"` / `mode":"procedural"`) plus
a `hint` path. Place files at the preferred locations to upgrade the stand-in.

DEM override seam: `gis::set_sample_dem_path_override` (cleared when falling
back to China stand-in). Scene3d terrain rebuild honors the override so
non-China extents are not forced back to the China box.

## Viz seams (shell host bridges)

`Browser::install_plugin_host_bridges` sets `PluginHost::gis_document()` and
`Scene3dSink` (`set_bridges` + `set_earth_bridges` for
`open_earth` / `load_global_dem` / `set_satellite_cloud` / `set_atmosphere` /
`fly_to` / `attach_tileset`; `set_look_bridges` for `apply_look` / `fly_globe`).
Unset → `no_scene_device`. Orthogrid / hexgrid commit through `gis_document()` +
`orthogrid.present_frame` / `orthogrid3d.present_frame`.

Satellite cloud with a path calls
`AtmosphereSession::load_fields("<path>:cloud_cover")`.

## Demo

```bat
build.bat debug SmartGisViews
out\Debug\SmartGIS.exe --plugin-showcase=world3d
REM capture: out\Debug\captures\plugin\plugin-showcase-world3d.bmp

REM interactive Earth + optional global DEM / cloud (processing JSON):
REM world3d.open_earth
REM world3d.load_global_dem  {"path":"C:/data/global_dem.tif"}
REM world3d.set_satellite_cloud  {"enabled":true}
REM world3d.set_atmosphere  {"sky":true,"ocean":true,"cloud":true,"fog":true}
```

Interactive: Tools → **打开真三维地球** / **加载全球DEM** / **卫星云图** /
**大气层开关**.

## Honest gaps vs Google Earth

- Default browse is China orbit-normalized DEM; full WGS84 sphere globe mesh is
  still a gap (global DEM uses planar orbit framing of the raster envelope).
- No worldwide Ion / Photorealistic 3D / street view.
- Satellite cloud without a GeoTIFF uses procedural atmosphere clouds, not
  live meteorological imagery streaming.
- City tiles = local fixture stream, not production city coverage.
- Fly-to is extent reframe. Cinematic globe fly lives on `world3d.fly_globe`
  (`scene/fly/globe_fly.*`); `--atmosphere-showcase=globe` is a harness HWND
  capture of that product path.
