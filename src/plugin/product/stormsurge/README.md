<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# smartgis.stormsurge

DEM + coast/ocean seeds + surge levels → inundation mask / depth via
`native.storm_surge`, then **map2d** mask-on-DEM polygons and a **water-surface
triangle mesh** that also installs into **scene3d** (`Scene3dPresenter` overlay
TIN — same seam as mine stratum).

## Viz seams (product present on `PluginHost`)

| Present | Payload |
| --- | --- |
| `present_stormsurge_mask` | Byte mask frames + geotransform + water_level → map2d polygons; frame 0 clears stale scene3d water TIN |
| `present_stormsurge_water_mesh` | Per-frame interleaved xyz + triangle indices → map2d `add_triangle_layer` **and** scene3d overlay TIN (lon/lat/elev) |
| `stormsurge.present_frame` | ResultPlayback re-present for scrub index |

Missing `gis_document()` → structured `no_stormsurge_seam` JSON.

**Playback:** `Browser::apply_plugin_frame` runs `stormsurge.present_frame`.

**Scene3D tab:** Chrome `select_map_tab(2)` seeds atmosphere **before** lazy
Vista attach and `abandon()`s leftover stereo (no `destroy_` under Vista) so
overlay TIN sessions do not AV / heap-corrupt on tab switch.

Mesh builder: `gis::detail::build_storm_surge_water_mesh` (shared-vertex grid,
downsampled like flood paint). P2 metrics: `native.storm_surge_stats` /
`stormsurge.stats`.

## Commands

- `stormsurge.run` / `stormsurge.export` / `stormsurge.load_coast` /
  `stormsurge.stats` / `stormsurge.about`

## Verify

```bat
build.bat debug analysis_storm_surge_test
build.bat debug stormsurge_run_test
REM Suite (GPU HWND BMP + playback-water-tin + tab3d-chrome); prefer --no-build when exe fresh
py -3 testing/tools/loop_runner.py --suite plugin.stormsurge --no-build
```

`--plugin-showcase=stormsurge` runs `run_stormsurge_scene3d` (owned present, peer of
world3d/mine). Required marks include `playback-water-tin` and `tab3d-chrome`.
Optional map2d script: `testing/tools/harness/plugin/plugin.stormsurge/plugin.stormsurge.il`.
