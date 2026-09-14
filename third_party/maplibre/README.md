<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# MapLibre Native pin (optional)

Do **not** vendor the MapLibre Native tree into git. Same discipline as Skia / FlyCube: local checkout or directory symlink under `third_party/.src/`.

## Expected layout

```
third_party/.src/maplibre-native/include/mbgl/map/map.hpp
```

or

```
third_party/.src/maplibre-native/include/mln/map.hpp
```

Enable with `smt_enable_maplibre=true`. Default remains **off**.

## What this repo does today

- GPU Track A (`SMT_MAP_BACKEND=a`) paints a real shared-surface frame from `StyleDocument` + `TileProvider` **without** requiring the pin.
- When the pin headers exist, `src/gpu/maplibre_runtime.cc` includes them. A live `mln::Map` / `mbgl::Map` + HeadlessFrontend still-image is **not** linked on this Windows MSVC tree until a matching `maplibre-native` lib is provided.

## What is not `mln::Map`

Style background + one raster uploaded through `PresentTarget` is an adapter, not the MapLibre Native renderer. Do not advertise it as `mln::Map`.
