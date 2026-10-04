<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# MapLibre pin out of `src/gpu`

**Status:** superseded  
**Date:** 2026-09-27  
**Superseded by (2026-09-27):** User decision — **删除 maplibre，将来再考虑**. Do **not** migrate the Native pin to `src/render/maplibre`. Remove product GN optional enable + link probe now; defer any future reconsideration. No half-move.  
**Plan:** [`../plans/2026-09-27-maplibre-out-of-gpu.md`](../plans/2026-09-27-maplibre-out-of-gpu.md) (cancelled)  
**As-built:** [`../../../../src/gpu/README.md`](../../../../src/gpu/README.md)；[`../../../../third_party/maplibre/README.md`](../../../../third_party/maplibre/README.md)

## Decision (normative)

1. Delete `src/gpu/maplibre_link.*`, `src/gpu/maplibre.gni`, `smt_enable_maplibre`, and product deps on `//third_party/maplibre:maplibre_native`.
2. Do **not** create `src/render/maplibre/`.
3. Keep `ContentSource::kTile` and wire aliases `SMT_MAP_BACKEND=a|track_a|maplibre` / `view.backend.maplibre` as the StyleDocument tile path (rename optional, out of this change).
4. `third_party/.src/maplibre-native` vendor checkout may remain from prior fetch; product BUILD does not link it. Thin `third_party/maplibre` wrappers stay on disk; GN target is an empty group documenting deferred.

## Historical body (obsolete — was “move probe to render/maplibre”)

The prior accepted approach (sibling under `src/render/maplibre`) is **cancelled**. Product Pass remains `src/vista/map`; tile compositing remains `gis/present` + gpu `raster/tile`. Native pin ownership is **none** until a future decision reopens the topic.
