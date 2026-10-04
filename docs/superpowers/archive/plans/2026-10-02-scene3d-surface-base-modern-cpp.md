<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# scene3d surface base + modern C++ — Implementation Plan

**Status:** landed (archived 2026-10-03 — checkboxes complete)

> **For agentic workers:** checkbox tracking. Spec § in [`../../specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md`](../../specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md) (§12d).

**Goal:** `SmtSurfaceObject` base; modernize + hot-path optimize `SmtTerrain` / `Smt3DPointCloud`. Scheme C, no shim.

## Tasks

- [x] Add `surface_base.h` / `surface_base.cc` (`SmtSurfaceObject`)
- [x] Refactor `SmtTerrain` onto base; color ramp `std::array`; single-pass attribs; drop POINTLIST draw
- [x] Refactor `Smt3DPointCloud` onto base; `unordered_map` buckets; snake_case members
- [x] Update GN + module README + umbrella §12d
- [x] `build.bat debug dem_stereo_test` (+ `pointcloud_load_test`) green
- [x] Per-object leftover showcase: `--scene3d-showcase` modes + harness suites (`legacy.scene3d.{terrain,cube,sphere,water,pointcloud,northarray}`); `surface_base` covered via terrain/pointcloud render path
