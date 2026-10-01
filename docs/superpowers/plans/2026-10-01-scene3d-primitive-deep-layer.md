<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# scene3d primitive deep layer + legacy/gis/feature — Implementation Plan

> **For agentic workers:** checkbox tracking. Spec § in [`../specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md`](../specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md) (§12c).

**Goal:** Deep-layer `scene3d/primitive` under `legacy/` only; merge geoobjects; device-free tess in `legacy/gis/feature`. Scheme C, no shim.

## Tasks

- [x] Layout: `host/` + `primitive/{mesh,feature,surface}/`
- [x] `legacy/gis/feature` (`mesh.h`, `tess_map`, `tess_world`) → `gis.dll`
- [x] Merge `Smt2DGeoObject`/`Smt3DGeoObject` → `SmtGeoObject` (Scheme C)
- [x] Update includes + GN + module README + umbrella §12c
- [x] `build.bat debug dem_stereo_test` (+ `pointcloud_load_test` if touched) green
