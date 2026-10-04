<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Sky / Fog / LOD DEM — Implementation Plan

> **For agentic workers:** Execute task-by-task. Steps use checkbox (`- [ ]`) syntax.

**Status:** active  
**Date:** 2026-09-27  
**Goal:** Land minimal recordable `SkyPass` / `FogPass` on the RHI atmosphere path, wire `AtmosphereFrame` hooks, and add distance-driven LOD for DEM terrain meshes so sky/fog/ocean share depth with real elevation.

**Architecture:** GPU passes stay in `render::atmosphere` (POD in → CommandList out). Session toggles / sun / fog knobs live in `gis::atmosphere::AtmosphereParams`; Views projects POD. DEM LOD policy and mesh generation stay in `gis::DemRaster`; GpuScene draws `kTerrain` as today (`kLitSolid` / textured). No second RHI backend; no render→legacy.

**Tech Stack:** C++23, FlyCube/Null RHI Facade, existing `DemRaster::build_mesh(max_edge)`, `AtmosphereFrame` pre/post opaque hooks.

**Related specs:**  
[`2026-09-27-atmosphere-subdirectory-layout-design.md`](../archive/specs/2026-09-27-atmosphere-subdirectory-layout-design.md) (layout, landed) ·  
[`2026-09-19-atmosphere-ocean-cloud-design.md`](../specs/2026-09-13-render-rhi-scene-design.md) (capability) ·  
[`2026-09-19-scene3d-world-gpuscene-design.md`](../archive/specs/2026-09-19-scene3d-world-gpuscene-design.md) (DEM → World)

## Global Constraints

- Public headers at `src/render/atmosphere/*.h`; impl under `sky/` `fog/`.
- Namespaces two layers + `detail`; functions `snake_case`.
- Reuse `PipelineId::kSolid` (+ blend/depth modes) for MVP sky/fog draws; analytical tint on CPU. Full `kSky`/`kFog` HLSL deferred (gap documented).
- Prefer extending `DemRaster` / `seed_*` over a parallel terrain engine.
- Null-path unit tests; `build.bat` related test targets green.
- No commit unless user asks; work on `master` only.

---

## File map

| Path | Role |
| --- | --- |
| `src/render/atmosphere/sky/sky_pass.h` | Public SkyPass + SkyDrawParams |
| `src/render/atmosphere/sky/sky_pass.cc` | Dome mesh + analytical sample + record |
| `src/render/atmosphere/fog/fog_pass.h` | Public FogPass + FogDrawParams |
| `src/render/atmosphere/fog/fog_pass.cc` | Fullscreen fog quad + factor helpers |
| `src/render/atmosphere/frame/atmosphere_frame.h/.cc` | set_sky/fog_pass; record hooks |
| `src/render/atmosphere/*_test.cc` | Null record smoke |
| `src/gis/.../dem_raster.h/.cc` | `dem_lod_max_edge` + tests |
| `src/app/views/scene3d_controller.*` | Project params; LOD rebuild; Frame wires |
| `src/gis/.../atmosphere_params.h` | sky/fog session fields |
| Docs / `src/render/README.md` | Living updates |

---

## Task 1: SkyPass (TDD)

- [x] Add `sky_pass_test.cc` (Null): analytical helpers + `record` success
- [x] Implement `sky_pass.h` + `sky/sky_pass.cc` (hemisphere / far dome, sun-tinted zenith→horizon)
- [x] Wire `AtmosphereFrame::set_sky_pass`; `record_pre_opaque` calls sky first (clear); ocean then load if both on
- [x] Update `atmosphere_frame_test` + BUILD.gn

## Task 2: FogPass (TDD)

- [x] Add `fog_pass_test.cc` (Null): height/distance factor + `record`
- [x] Implement fog pass (fullscreen, `kSrcAlpha`, depth load / test-only)
- [x] Wire `record_post_opaque` after cloud
- [x] Update Frame test + BUILD.gn

## Task 3: LOD DEM

- [x] Add `dem_lod_max_edge(distance, …)` (near/mid/far → edge counts)
- [x] Test: closer distance → larger max_edge / more verts
- [x] `Scene3dController::rebuild_local_mesh` picks LOD from `distance_`; rebuild when level changes
- [x] Document gap vs clipmap / screen-space error tessellation

## Task 4: Host projection + docs

- [x] `AtmosphereParams` sky/fog enabled + fog density/visibility POD
- [x] Controller: prepare + Frame enable/pass pointers
- [x] Revise layout §4 status rows; capability note; README; checkboxes

## Task 5: Verify

- [x] `build.bat` / ninja: `sky_pass_test` `fog_pass_test` `atmosphere_frame_test` `dem_raster_test` `scene3d_controller_test` — all PASS (2026-09-27)
- [x] 2026-09-28: dedicated `sky/hlsl.h` + `fog/hlsl.h` (pixel sky + view-ray fog); DEM `vert_exag` → leftover `0.09` (clipmap / fog depth-sample still Deferred)

## Non-goals

- Bruneton/Hillaire LUT, leftover GL fog, second terrain engine, Qt, render→legacy.

## Gap vs full GIS prod

| Item | This plan (2026-09-28) | Full prod |
| --- | --- | --- |
| Sky | Dedicated HLSL dome PS (zenith→horizon + sun glow) | Physically based LUT + multi-scatter |
| Fog | View-ray distance×height HLSL FS; shared depth Deferred | Depth-sampled volumetric / aerial perspective |
| Terrain LOD | Discrete `max_edge` buckets from camera distance | Clipmaps / CDLOD / GPU tessellation |
