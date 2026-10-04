<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/gis`

Product GIS library (`dll_stem = gis`). Public namespaces stay two layers (`gis`, `gis::datasource`, helpers in `gis::detail` / `gis::datasource`). `gis::style` / `gis::tile` are grouping namespaces still to hoist.

## Layout (no `model/` parent; no one-file `layer/` / `crs/`)

| Dir | Role |
| --- | --- |
| `feature/` | `gis::Feature` (OGRFeature + `geometry_type()`); copy helpers |
| `map/` | `gis::Map` / `MapLayer`; `layer_kind.h` (`LayerType`, `VectorSchema`); `query.h` |
| `edit/` | Mutation types, `UndoLog`, command / memory / map sessions, optimistic store |
| `envelope.h` | Header-only MBR |
| `datasource/` | `session/` · `provider/` · `pipeline/` · `ogr/` · `sdbd/` · `gdal/` |
| `carto/{style,tile}` | StyleDocument + tiles (tile files colocated; no cache/protocol/provider wrappers) |
| `geo/` `stat/` `analysis/` | Spatial kernels (`stat/` files colocated) |

Leftover catalog / PascalCase Feature / carto POD: `src/legacy/gis/{layer,feature,datasource,present/carto}`. Leftover may include product; product must not include leftover. Leftover TUs still compile into `gis.dll` (singleton StyleManager / DataSourceMgr ABI) — strangler debt.

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
