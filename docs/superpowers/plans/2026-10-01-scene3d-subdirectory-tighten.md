<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# scene3d subdirectory tighten + legacy/gis/vista — Implementation Plan

> **For agentic workers:** checkbox tracking. Spec § in [`../specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md`](../specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md) (§12b).

**Goal:** Merge thin scene3d dirs into `primitive/` + `seed/`; move device-free DEM/World adapters to `legacy/gis/vista` (`gis.dll`). Scheme C, no shim.

## Tasks

- [x] Create `legacy/gis/vista/` (`dem_height_field`, `dem_to_world`, `coord`) + `BUILD.gn` / README
- [x] Wire `vista_sources` into `//src/gis:gis`
- [x] Reshape scene3d → `scene/` `index/` `primitive/` `seed/` `test/`
- [x] Update all includes + `scene3d/BUILD.gn` + module README + `src-layout` + umbrella §12b
- [x] `build.bat debug dem_stereo_test` (+ `pointcloud_load_test` if touched) green
