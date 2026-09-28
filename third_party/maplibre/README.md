<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# MapLibre Native pin (deferred)

**Status (2026-09-27):** Product integration **removed**. Decision: delete the
mln/mbgl pin from the build graph; reconsider later. There is no
`smt_enable_maplibre`, no `//third_party/maplibre:maplibre_native` link target,
and no `src/gpu` / `src/render/maplibre` probe.

Do **not** treat GPU `ContentSource::kTile` / `SMT_MAP_BACKEND=maplibre` /
`view.backend.maplibre` as MapLibre Native — those wire names select the
StyleDocument + TileProvider tile path only.

## What remains on disk

| Path | Role |
| --- | --- |
| `third_party/maplibre/BUILD.gn` | Empty `group("maplibre")` — not linked |
| `third_party/maplibre/include/`, `src/` | Former thin `mln::Map` wrappers; unused by product GN |
| `third_party/manifest.json` → `maplibre-native` | Fetch metadata kept; `skip_fetch_all` |
| `third_party/.src/maplibre-native` | Optional prior checkout; not a product dep |

## Reconsider later

If product opts back in: reintroduce a thin GN target, an optional
`declare_args` flag, and a clear owner module — **not** under `src/gpu` paint
core, and **not** inside `src/effect/map` Pass. Until then, do not enable
Track A via gpu or claim still-image paint through `mln::Map`.
