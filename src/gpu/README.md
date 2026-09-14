<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/gpu`

`--type=gpu` payload. Chrome talks only to `content/public`; this tree owns paint.

## Render backends

| Track | How to select | What paints |
| --- | --- | --- |
| **B (default)** | unset `SMT_MAP_BACKEND` | Demo clear + GDI features; 3D stays here / `GpuScene` |
| **A** | `SMT_MAP_BACKEND=a` | 2D basemap: Style JSON `background-color` + one XYZ raster via a `TileFetchFn` (wrap `TileProvider` at the call site; do not add a second HTTP cache), uploaded into `PresentTarget`. |

3D (`ViewKind::kScene3d`) always stays Track B.

## MapLibre Native pin (optional)

GN `smt_enable_maplibre=false` by default (not in `src_all` / `all`).

```bat
gn gen out --args="is_debug=true smt_enable_maplibre=true"
```

Pin path: `third_party/.src/maplibre-native/include/` (`mbgl/map/map.hpp` or `mln/map.hpp`). See [`third_party/maplibre/README.md`](../../third_party/maplibre/README.md).

**Honest status:** this tree does **not** link `mln::Map`. Flag-on without a pin still compiles the adapter and a header probe. Still-image via HeadlessFrontend is wired only when those headers **and** a Windows lib exist.

## Tests

```bat
build.bat render_backend_test.exe
out\render_backend_test.exe
```
