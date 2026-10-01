<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Plan: orthogrid3d HexGrid + VTK (M0)

**Umbrella:** `docs/superpowers/specs/2026-09-13-plugin-host-design.md` §orthogrid3d  
**Status:** active

## Files

| Path | Role |
| --- | --- |
| `src/gis/kernel/geo/mesh/geometry.h` + `hex_grid.cpp` | `geo::HexGrid` |
| `src/plugin/product/orthogrid3d/**` | Laplace3d, 8-corner solve, `.vts`, commands |
| `src/app/views/shell/browser/plugin/plugin_shell.cc` + `BUILD.gn` | Register builtin |

## Tasks

- [x] Living § + this plan
- [x] `HexGrid` type + unit accessors
- [x] 7-point Laplace + 8-corner boundary solve + cell orthogonality
- [x] VTK `.vts` writer
- [x] `register_orthogrid3d` + processing
- [x] `orthogrid3d_laplace_test` + wire build

## Done when

`orthogrid3d_laplace_test` green; processing creates hex mesh and optional `.vts`.
**M0 landed 2026-09-30** — test OK; `orthogrid3d_views` builds.
