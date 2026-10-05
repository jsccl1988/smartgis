<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/gis`

Product GIS library (`dll_stem = gis`). Public namespaces stay two layers:
`gis`, `gis::datasource`, `gis::style`, `gis::tile` (helpers in `…::detail`).
Spatial kernels use the sibling public namespace `geo` (not a third `gis::geo`
layer).

## Naming (product API)

| Kind | Convention |
| --- | --- |
| Types / enums | `PascalCase` (`Map`, `MapLayer`, `LayerType`) |
| Enum enumerators | `k` + `PascalCase` (`LayerType::kTile`) |
| Functions / methods | `snake_case` (`add_layer`, `cal_envelope`) |
| Members | `snake_case_` (`layers_`, `active_`) |
| Constants | `k_snake_case` (`k_map_name_max`) |
| Include guards | `GIS_<PATH>_H_` (not `SDB_`) |
| Sources | prefer `.cc` next to `.h` |

Do **not** keep Hungarian prefixes (`sz`, `p`, `m_`) on new or touched product
API. GDAL/OGR override names (`GetLayerCount`, `ResetReading`, …) stay as the
external ABI requires.

Anchor peers: `gis::Feature` and `gis::MapLayer` already follow this style;
`gis::Map`, raster/tile wrappers, and query POD are aligned to the same rules.

## Layout (no `model/` parent; no one-file `layer/` / `crs/`)

| Dir | Role |
| --- | --- |
| `feature/` | `gis::Feature` (OGRFeature + `geometry_type()`); copy helpers |
| `map/` | `gis::Map` / `MapLayer`; `layer_kind.h` (`LayerType`, `VectorSchema`); `query.h` |
| `edit/` | Mutation types, `UndoLog`, command / memory / map sessions, optimistic store |
| `envelope.h` | Header-only MBR |
| `datasource/` | `session/` · `provider/` · `pipeline/` · `ogr/` · `sdbd/` · `gdal/` |
| `style/` · `tile/` | StyleDocument + tiles (tile files colocated) |
| `geo/` `stat/` `analysis/` | Spatial kernels (`stat/` files colocated) |

Leftover catalog / PascalCase Feature / carto POD: leftover trees only.
Leftover may include product; product must not include leftover.

## `edit/` (composed, not nested `session/` + `map_edit/`)

| File | Concern |
| --- | --- |
| `mutation.h` | `EditOp`, `CommitStatus`, `FeatureMutation`, `FeatureGeom` |
| `session.h` | `EditSession` interface |
| `undo_log.*` | Done/undone stacks |
| `command_session.*` | Apply callback + `UndoLog` |
| `optimistic_store.*` | Version tokens |
| `memory_session.*` | In-memory log + optional store |
| `map_session.*` | Binds apply to `gis::Map` (composes `CommandEditSession`) |

Includes: `"gis/edit/session.h"`, `"gis/edit/memory_session.h"`, `"gis/edit/map_session.h"`.
