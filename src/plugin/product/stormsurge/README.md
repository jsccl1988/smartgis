<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# smartgis.stormsurge

DEM + coast/ocean seeds + surge levels → inundation mask / depth via
`native.storm_surge`, then **map2d** mask-on-DEM polygons and a **water-surface
triangle mesh** that also installs into **scene3d** (`Scene3dPresenter` overlay
TIN — same seam as mine stratum).

## Viz seams (Browser installs in `Browser::init`)

| Writer | Payload |
| --- | --- |
| `set_stormsurge_mask_writer` | Byte mask frames + geotransform + water_level → map2d polygons; clears stale scene3d water TIN on frame 0; levels stored per frame on `AnalysisPlayback` |
| `set_stormsurge_water_mesh_writer` | Per-frame interleaved xyz + triangle indices → map2d `add_triangle_layer` **and** `scene3d->set_overlay_tin_mesh` (lon/lat/elev, no map Y flip); meshes stored for ResultPlayback scrub |

Unset writers → structured `no_stormsurge_seam` / `no_stormsurge_water_mesh` JSON.

**Playback:** `Browser::apply_analysis_frame` re-paints the mask and re-pushes the
stored water TIN for the scrub index (not mask-only).

**Scene3D tab:** Chrome `select_map_tab(2)` seeds atmosphere **before** lazy
FlyCube attach and `abandon()`s leftover stereo (no `destroy_` under FlyCube) so
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
