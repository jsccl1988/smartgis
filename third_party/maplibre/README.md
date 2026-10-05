<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# MapLibre Native (example + StyleDocument align)

**Product pin:** still **deferred** (2026-09-27). Do not link mln/mbgl from
`src/app`, `src/gpu`, or `src/effect/map`. Wire name `maplibre` on the tile
backend means StyleDocument + TileProvider, **not** MapLibre Native.

**Align Style (2026-09-29):** shared Style JSON over the **same china_city
pack** the main app opens (`out/data/china_city.gpkg` / `.geojson` via
`//testing/data:china_map_samples`). Product loads StyleDocument only.

| Path | Role |
| --- | --- |
| `example/style_align.json` | China carto subset (area / river / road / point / text) |
| `example/style_background.json` | Headless smoke (solid background only) |
| `$root_out_dir/maplibre/example/china_city.geojson` | Copied china_city pack for Native `sources` |
| `out/data/china_city.*` | Same pack for product Open (Views) |

GN copies styles + geojson to `$root_out_dir/maplibre/example/`
(`//third_party/maplibre:maplibre_example_assets`). Style `sources.china.data`
is `asset://china_city.geojson` (resolved via `mbgl-render -a` asset root).

## Product still (no Native link)

```bat
build.bat debug //src/app/views:views
out\Debug\SmartGIS.exe --map2d-showcase=align
```

Opens `china_city.gpkg` (same candidates as china showcase), loads
`style_align.json`, writes `out\Debug\map2d-showcase-align.bmp` (640Ã—480,
extent 80â€?28Â°E / 20â€?8Â°N).

## Dual align (product + optional Native)

```bat
python testing\tools\harness\_shared\case\align\maplibre_align.py
python testing\tools\harness\_shared\case\align\maplibre_align.py --skip-native
```

Outputs under `out\Debug\maplibre\align\`: `product.bmp`, optional
`native.png`, `report.json`.

## Enable Native examples

```bat
gn gen out/Debug --root=./ --args="is_debug=true is_build_third_party=false enable_maplibre_example=true"
ninja -C out/Debug maplibre_examples
```

Or:

```bat
build.bat debug //third_party/maplibre:maplibre_examples
```

(with `enable_maplibre_example=true` already in the gen args).

Outputs (after successful CMake):

| Binary | Role |
| --- | --- |
| `out/Debug/maplibre/maplibre_headless_example.exe` | Upstream `mbgl-render` (HeadlessFrontend + WGL) |
| `out/Debug/maplibre/maplibre_glfw_example.exe` | Upstream `mbgl-glfw` |

## Smoke (headless, offline)

```bat
out\Debug\maplibre\maplibre_headless_example.exe -s third_party\maplibre\example\style_background.json -o out\Debug\maplibre\still.png -w 256 -h 256
```

Align Style + china_city (run after china samples are in `out/data`):

```bat
out\Debug\maplibre\maplibre_headless_example.exe -s out\Debug\maplibre\example\style_align.json -o out\Debug\maplibre\align\native.png -w 640 -h 480 -a out\Debug\maplibre\example --bounds 48 80 20 128
```

## What remains unused

| Path | Role |
| --- | --- |
| `include/mln/map.hpp`, `src/mln_map.cc` | Former thin still-image facade; not a GN source |
| `src/native_map_probe.cc` | Compile probe only; not linked |

## Reconsider product later

A future product pin needs a clear owner module **outside** `src/gpu` paint
core and **outside** `src/effect/map` Pass â€?this example / align path is not
that pin.
