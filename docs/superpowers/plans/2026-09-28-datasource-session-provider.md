<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# DataSession / Provider facade — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Land new-tree `gis::datasource::DataSession` + `ProviderRegistry` so product callers open data via `MapLayer` / `Feature` without depending on legacy `DataSourceMgr`.

**Architecture:** Facade + Adapter. `session/` owns RAII `DatasetHandle` and session entry; `provider/` registers Local/Remote adapters over existing `open_sdbd_dataset` / `open_provider_sdbd_dataset`. `mgr/` stays frozen for `legacy/*`.

**Tech Stack:** C++23, GN (`build.bat`), GDAL/OGR, gtest; types in `gis::datasource` (two-level namespace).

## Global Constraints

- Stay on `master`; snake_case functions; English comments; copyright year 2026.
- Product ABI: `gis::MapLayer` / `gis::Feature` (not raw `GDALDataset*` as the primary return for new APIs — `DatasetHandle::gdal()` is escape hatch only).
- `session` / `provider` must **not** depend on `datasource/mgr`.
- Do **not** change `DataSourceMgr` public API this plan.
- Spec §: `docs/superpowers/specs/2026-09-13-gdal-layer-management-design.md` § DataSession / Provider facade.
- Output only under `out/`.

---

### Task 1: `ConnectionSpec` + `DatasetHandle` + `DataSession` headers/impl

**Files:**
- Create `src/gis/datasource/session/connection_spec.h` / `.cc`
- Create `src/gis/datasource/session/dataset_handle.h` / `.cc`
- Create `src/gis/datasource/session/data_session.h` / `.cc`
- Create `src/gis/datasource/session/BUILD.gn` (`session_sources`)

- [x] Implement `ConnectionSpec::{to_info,from_info,kind_from_info}` (PROVIDER_SDBD → kRemoteSdbd)
- [x] Implement move-only `DatasetHandle` with `GDALClose` in dtor; `layer_at` / `layer_by_name` → `MapLayer::from_ogr`
- [x] `DataSession` holds `ProviderRegistry`; `open` / `create_mem_vector_layer` (MEM via local provider + CreateLayer + `MapLayer::adopt_dataset`)

### Task 2: `Provider` + registry + local/remote adapters

**Files:**
- Create `src/gis/datasource/provider/provider.h`
- Create `src/gis/datasource/provider/provider_registry.h` / `.cc`
- Create `src/gis/datasource/provider/local_sdbd_provider.h` / `.cc`
- Create `src/gis/datasource/provider/remote_sdbd_provider.h` / `.cc`
- Create `src/gis/datasource/provider/BUILD.gn` (`provider_sources`)

- [x] `LocalSdbdProvider::open` → `open_sdbd_dataset`
- [x] `RemoteSdbdProvider::open` → `open_provider_sdbd_dataset`
- [x] `ProviderRegistry::make_default()` registers both
- [x] No include of `datasource/mgr/*`

### Task 3: Wire into `gis` DLL + session unit test

**Files:**
- Modify `src/gis/BUILD.gn` — deps `session_sources` + `provider_sources`
- Create `src/gis/datasource/session/datasource_session_test.cc`
- Modify `src/gis/datasource/session/BUILD.gn` — `test("datasource_session_test")`
- Modify root `BUILD.gn` — add test to `test_all`
- Modify `docs/superpowers/src-layout.md` Datasource row
- Modify `docs/superpowers/README.md` Active table pointer

- [x] MEM open via `DataSession` asserts layer MapLayer name
- [x] `ConnectionSpec` round-trip name/url/provider
- [x] `build.bat` + `out\datasource_session_test.exe` green

### Task 4: Docs self-check

- [x] Living § Success criteria satisfied; no TBD in plan checkboxes left open for landed work
- [x] Confirm no new-tree call site added to `DataSourceMgr` in this change

---

### Task 5: `gis/model` leftover-only ABI out + OGR Map/Layer/Feature seam

**Spec:** living umbrella `docs/superpowers/specs/2026-09-13-gdal-layer-management-design.md` **§ gis/model product surface vs leftover + OGR Map/Layer/Feature**.  
**Diagram:** `docs/superpowers/diagrams/gis-model-ogr-layers.html`.  
Do **not** invent product→legacy deps. Stay on `master`. One phase per change.

- [x] Product new code uses only `gis::Feature` / `gis::MapLayer` / `DataSession` snake_case + `ogr()`; no new `SmtLayer` virtuals
- [x] Remove `#include "legacy/…"` from `gis/feature/feature_api.h`; keep OGR `copy_layer`/`append_cloned_feature`; MOVE `SmtLayer*`/`SmtRasterLayer*` overloads to leftover
- [x] `ConnectionSpec` public header does not include `gis/map/layer_kind.h` / `SmtDataSourceInfo`
- [x] `MapLayer` product seam: `GetLayerDefn`, spatial/attribute filter, feature iteration (thin OGR wrap)
- [x] `Feature` product seam: `GetField`/`SetField` for full `OGRFieldType` set (not a string map)
- [x] Rename product `SmtMap` → `gis::Map`; leftover alias/wrapper until catalog/rhi2d migrate
- [ ] MOVE `SmtLayer` / `SmtRasterLayer` / `SmtTileLayer` virtual bases + `SmtGQueryDesc` (partial: `SmtDataSource` / `Smt*Info` / `leftover_layer_feature_type` already in `legacy/gis/layer/layer.h`)
- [x] MOVE `Feature` PascalCase leftover names + `leftover_append_feature` out of product `gis/model`
- [ ] Delete `MapLayer::from_leftover` from product (leftover uses `from_ogr`)
- [x] `gis` tests green (`feature_test`, `select_query_test`, `datasource_session_test`); no new product `#include "legacy/…"`
