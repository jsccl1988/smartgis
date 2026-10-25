<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Point cloud I/O (`vista/assets/pointcloud`)

Production load path for shell / `world3d` / Browser: **`load_point_cloud`** dispatches by extension.

| Extension | Backend | Notes |
| --- | --- | --- |
| `.las` | In-tree **ASPRS LAS 1.2/1.4** reader (`las_io`) | Default product path; no PDAL required |
| `.laz` | **LASzip** (`laz_io` → `load_las_via_laszip`) | Same point payload as LAS after decompress |
| `.txt` / `.xyz` | Legacy comma RGB dialect (`text_io`) | Leftover-compatible `x,z,y,r,g,b` lines |

## Large clouds (≥1M points)

After load, **`World::set_pointcloud_points`** (in `vista/component/world/world.cc`) applies:

1. **Auto-thin** when count **> 500 000** — uniform stride via `select_point_cloud_lod` (`max_points = 500000`).
2. **Chunk partition** — `build_point_cloud_chunks` (8³ grid, ≤50 000 points/chunk cap) for per-chunk AABB cull in `WorldPass` / `GpuScene`.

Synthetic regression (no multi‑MB LAS on disk): `pointcloud_test` builds **1.1M** in-memory XYZ and asserts thin + chunks.

## PDAL (optional, `has_pdal`)

| | In-tree LAS/LAZ | PDAL (`pdal_io`) |
| --- | --- | --- |
| Build flag | Always on (LASzip for LAZ) | `has_pdal` when `third_party/.install/include/pdal/PipelineManager.hpp` exists (`build.bat t pdal`) |
| Entry | `load_point_cloud(path)` | `run_pdal_read` / `run_pdal_pipeline_json`; `pdal_is_available()` |
| Processing | — | `world3d.pdal_read`, `world3d.pdal_pipeline` |
| Stub | — | Returns `pdal_not_built` when PDAL not installed (`pdal_io_test`) |

Use **in-tree LAS/LAZ** for normal open-file UX; PDAL is for filters/pipelines when the optional install is present.

## Verify

```bat
build.bat debug pointcloud_test
out\Debug\pointcloud_test.exe
```
