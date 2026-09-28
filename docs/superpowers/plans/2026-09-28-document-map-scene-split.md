<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Document `MapScene` internal split — Implementation Plan

> **For agentic workers:** Execute on `master`. Public API must stay `MapScene*`.

**Goal:** Split the ~1.9k-line `map_scene.cc` into composable `content::detail` units under shallow `document/` subdirs, with `MapScene` as a thin façade.

**Architecture:** `MapScene` owns `LayerStore` + `StyleBind`. Ingest / query / edit / write / seed are detail helpers that mutate or read the store (and style when needed). Callers and `map_scene_test` keep the existing public surface.

**Tech Stack:** C++23, GN `:map_scene`, GDAL/OGR, existing `gis::style` / tile types.

## Global Constraints

- Work on `master` only; no feature branch.
- Public namespace stays two layers (`content`); internals in `content::detail`.
- No shim headers; no Qt; no paint/behavior change.
- Living design: `docs/superpowers/specs/2026-09-27-views-desktop-shell-design.md` §Document internal split.
- Snake_case for new functions; English comments.

## File map

| Path | Role |
| --- | --- |
| `document/store/map_layer.h` | `detail::GeomKind`, `Vertex`, `MapFeature`, `MapLayer`, field helpers |
| `document/store/layer_store.*` | layers / CRUD / selection / ids / sample features |
| `document/style/style_bind.*` | StyleDocument, basemap, resolve colors |
| `document/ingest/ogr_ingest.*` | OGR→features, china clip, kind split, ingest_path |
| `document/ingest/geojson_write.*` | write_path |
| `document/ingest/seed_paths.*` | china + style seed relative paths, path_stem |
| `document/query/extent_query.*` | envelopes, china test, land rings, fit box |
| `document/query/inspector.*` | feature info / attribute rows |
| `document/edit/feature_edit.*` | draft, vertex move, TIN, hit_test, copy_xy |
| `document/map_scene.*` | façade forwards |
| `src/content/BUILD.gn` | `:map_scene` sources list |

## Tasks

- [x] T1 — Add `store/map_layer.h` + `layer_store.*`; wire `MapScene` aliases + ownership
- [x] T2 — Extract `style/style_bind.*`
- [x] T3 — Extract `ingest/{ogr_ingest,geojson_write,seed_paths}.*`
- [x] T4 — Extract `query/{extent_query,inspector}.*` + `edit/feature_edit.*`
- [x] T5 — Slim `map_scene.cc` to forwards; update GN
- [x] T6 — `build.bat map_scene_test` green (`map_scene_test ok`)

## Done when

- `map_scene.cc` is a thin façade (roughly &lt;250 lines of forwards).
- Public methods and `china_seed_relative_paths` still compile for existing callers.
- `map_scene_test` passes.
