<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# smartgis.world3d

True-3D Earth browse (完整真3D) plus DEM TIN/grid loaders and former `model3d`
scene commands. Package id stays `smartgis.world3d` (no parallel `earth3d`).

## Product face (Google Earth–class MVP)

| Command / processing | Role |
| --- | --- |
| `world3d.open_earth` | Scene3D tab + China DEM framing + atmosphere (sky/ocean/cloud/fog) |
| `world3d.fly_to` | JSON `{lon,lat,distance?,span_deg?}` — local orbit reframe |
| `world3d.attach_city_tileset` | JSON `{path?}` — 3D Tiles JSON (empty → `m3_city_tileset.json`) |
| `world3d.add_pointcloud` / LAS | Existing pointcloud hook |
| `world3d.tin_from_xyz` / `grid_from_heightmap` | DEM surface → MapScene triangles |

Resources copy to `out/plugins/world3d/`.

## Viz seams (Browser installs in `Browser::init`)

`set_world3d_surface_writer` + `set_world3d_scene_writer` including
`open_earth` / `fly_to` / `attach_tileset`. Unset → `no_scene_device`.

## Demo

```bat
build.bat debug SmartGisViews
out\Debug\SmartGisViews.exe --plugin-showcase=world3d
REM capture: out\Debug\captures\plugin-showcase-world3d.bmp
```

Interactive: Tools → **打开真三维地球**.

## Honest gaps vs Google Earth

- China orbit-normalized DEM, not a full WGS84 sphere globe.
- No worldwide Ion / Photorealistic 3D / street view.
- City tiles = local fixture stream, not production city coverage.
- Fly-to is extent reframe, not cinematic path animation.
