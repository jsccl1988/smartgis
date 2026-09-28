<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Legacy `rhi2d` layout Implementation Plan


> **Status: landed** (2026-09-28 merge). Layout landed; see archive spec. Do not revise here except mechanical link fixes.

**Status:** landed (awaiting human build verify)  
**Date:** 2026-09-28  
**Spec:** [`../specs/2026-09-28-legacy-rhi2d-layout-design.md`](../specs/2026-09-28-legacy-rhi2d-layout-design.md)

> Work on **master**. Do **not** commit unless the user asks. Agents do **not** run `build.bat` / ninja.

## Goal

Land the tree in the spec: `rhi2d/public/device` + `rhi2d/impl/gdi`; leftover → `rhi/public/bridge`; delete tops `bridge/` + `gdi/`.

## Tasks

### Task 1: Move trees (`git mv`) — done

- [x] `bridge/renderdevice.*` `renderer.*` → `rhi2d/public/device/`
- [x] `gdi/**` → `rhi2d/impl/gdi/`
- [x] `bridge/leftover_*` (+ tests) → `rhi/public/bridge/`
- [x] Remove empty `bridge/`

### Task 2: GN — done

- [x] `rhi2d/BUILD.gn` (`rhi2d_sources`)
- [x] `rhi2d/impl/gdi/BUILD.gn`
- [x] leftover source_sets on `rhi/BUILD.gn`
- [x] Root + scene3d / gl / d3d / tool / xview / `test_all` deps

### Task 3: Includes — done

- [x] Product includes / GN labels under `src/` + root `BUILD.gn`

### Task 4: Docs — done

- [x] `src/legacy/render/README.md`, dual-run landing note, src-layout / abi-rename-map

### Task 5: Human verify

```bat
.\build.bat legacy_render
.\build.bat te leftover_mesh_test
.\build.bat te leftover_record_test
.\build.bat te leftover_session_test
.\build.bat te map_carto2d_test
.\build.bat te gdi_map_paint_test
```
