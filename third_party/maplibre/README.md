<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# MapLibre Native pin (optional)

Do **not** vendor the MapLibre Native tree into git. Same discipline as Skia / FlyCube: fetch into `third_party/.src/`.

## Enable

```bat
python third_party\tools\fetch.py --package maplibre-native
gn gen out --args="is_debug=true smt_enable_maplibre=true"
ninja -C out render_backend_test
```

`smt_enable_maplibre` stays **false** by default and is **not** in `src_all` / `all`.

## Pin

| Field | Value |
| --- | --- |
| Package | `maplibre-native` in `third_party/manifest.json` |
| Ref | `ios-v6.30.0` (`0ffe6336b4e7266c75c13337b4aa1d0f2b0877d5`) |
| Dest | `third_party/.src/maplibre-native` |
| Vendor tarball cache | `third_party/.src/_cache/maplibre-vendor` |
| CMake binary dir | `third_party/.build/maplibre-native` |
| Installed lib | `out/third_party/maplibre` (`mbgl-core.lib`) |
| Linked lib | `//third_party/maplibre:maplibre_native` (`mln::Map` still-image) |

Record the resolved commit after fetch in `PIN.txt` (written by the fetch/overlay step). Do not `rmtree` CEF / FlyCube junctions. Do not clone Native into `out/` — `out/` is Ninja/CMake output only.

A full Windows OpenGL core build uses the pin tree plus `vendor/` (from the cache above) and vcpkg under `platform/windows/vendor/vcpkg`. Pass `-S third_party/.src/maplibre-native -B third_party/.build/maplibre-native` so CMake does not recreate `build-windows-opengl` inside `.src`.

## What links today

When `smt_enable_maplibre=true`, GPU Track A (`SMT_MAP_BACKEND=a`) constructs `mln::Map`, loads style JSON, and paints one BGRA frame into `PresentTarget`. `maplibre_map_linked()` is true only in that binary.

`mln::Map` in `third_party/maplibre/include/mln/map.hpp` is the Windows still-image facade linked into GPU Track A.

The pin's real type is `include/mln/map/map.hpp` (`mln::Map(RendererFrontend&, MapObserver&, MapOptions, ResourceOptions, …)` + `renderStill`). HeadlessFrontend lives under `platform/default`; Windows backends are EGL/WGL. A full core build needs vendor/vcpkg, style codegen, and those GPU backends — not produced in this tree.

## Fallback

Without the flag (default), Track A still paints style background + one XYZ raster through the adapter. Pin-off tests stay green (`mln_map=0`).
