<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# RHI / 3D Capability P0 — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Stay on `master`. No commit unless asked.

**Goal:** Product-readable 3D defaults (lit solid), stable style→GPU albedo for 3D nodes, CPU frustum cull at GpuScene scale, and greener optional FlyCube GPU smoke — without Shiva/two/PBR.

**Architecture:** Narrow Facade (`LightParams`, `PipelineId::kLitSolid`) + `GpuScene` bind for terrain/model/tileset; CPU AABB vs view frustum; Null counters + `SMT_RUN_FLYCUBE_GPU=1` smoke. Do **not** edit `src/render/atmosphere/**` or FieldStore (parallel atmosphere upgrade).

**Tech Stack:** C++23, `render::rhi` / FlyCube DX12, `gis::World` + `GpuScene`, `gis::style::ResolvedPaint`, GN/`build.bat`, Null RHI tests.

**Spec:** [`../specs/2026-09-20-rhi-3d-capability-p0-design.md`](../specs/2026-09-20-rhi-3d-capability-p0-design.md)

## Global Constraints

- Stay on `master`; path partition vs atmosphere: no `atmosphere/**`, no ocean/cloud PipelineId semantics changes.
- Public namespaces ≤2 layers; new APIs `snake_case`; English comments; Copyright 2026.
- No Qt; no bgfx/Shiva/two; no product `#include` of FlyCube headers.
- CI default Null; GPU optional skip-not-red.
- Build output only under `out/` via `build.bat`.

---

### Task 1: Spec cross-links + baseline inventory

**Files:**
- Modify: `docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md` (short pointer to this P0; optional one-line Deferred)
- Modify: `docs/superpowers/specs/2026-09-20-rhi-3d-capability-p0-design.md` / this plan if paths drift
- Read-only inventory: `src/render/scene/scene.h`, `leftover_record.cc` style paint, `rhi_test.cc` GPU gate

**Steps:**
1. [x] Confirm 2D `ResolvedPaint` → `set_instance_paint` / `record_map` style_document already green (`scene_gpu_test` / `leftover_record_test` P0 paint).
2. [x] List exact ninja targets: `rhi_test`, `scene_gpu_test` / `scene_test`, `unified_draw_test`, `leftover_record_test`.
3. [x] Add one-line Related link from RHI living spec → this P0 design (revise in place; no parallel empty shell).

**Baseline inventory (Task 1):**
- 2D paint: `src/render/scene/scene_test.cc` ~L281–307 (`ResolvedPaint` → `set_instance_paint`); `src/legacy/render/bridge/leftover_record_test.cc` ~L203–229 (`style_document` → `record_map`).
- RHI living Related already points at this P0 (no edit needed).
- Note: no `src/render/rhi/BUILD.gn` — `rhi_test` lives in `src/render/BUILD.gn`. No `scene_test` target — use `scene_gpu_test`.

**Verify:** Docs link resolve; inventory note in plan §Test commands below.

---

### Task 2: Facade `LightParams` + `PipelineId::kLitSolid` (TDD)

**Files:**
- Modify: `src/render/rhi/rhi.h`, `rhi.cc` (Null/Stub counters)
- Modify: `src/render/rhi/flycube_rhi.cc` (HLSL lit solid VS/PS + ColorCB/LightCB)
- Modify: `src/render/rhi/rhi_test.cc`
- Modify: `src/render/rhi/BUILD.gn` if needed

**Steps:**
1. [x] Write failing Null test: `set_light_params` increments stub counter; `set_pipeline(kLitSolid)` records id.
2. [x] Add `struct LightParams { float dir[3]; float ambient; float color[3]; float intensity; };` + `CommandList::set_light_params`.
3. [x] Add `PipelineId::kLitSolid` (value after `kCloud`, do not renumber ocean/cloud).
4. [x] FlyCube: compile lit solid shader (N·L Lambert + ambient); default light if unset (e.g. dir ≈ (−0.4, −0.8, −0.35) normalized, ambient 0.25).
5. [x] Run `rhi_test` Null path green.

**Verify:** `build.bat` target for `rhi_test` exit 0 without `SMT_RUN_FLYCUBE_GPU`.

---

### Task 3: GpuScene 3D lit + albedo from paint

**Files:**
- Modify: `src/render/scene/scene.h`, `scene.cc`
- Modify: `src/render/scene/scene_test.cc` (or `scene_gpu_test` TU)
- Touch only if needed: `src/app/views/scene3d_controller.*` (bind default light once) — **coordinate** if atmosphere Phase 3 owns views; prefer GpuScene defaults so views optional

**Steps:**
1. [x] Failing test: terrain or model mesh records `kLitSolid` (Null stub pipeline id) and applies `set_instance_paint` albedo to `GpuMesh.solid_*`.
2. [x] In `record_kind` / draw path for `kTerrain` / `kModel` / `kTileset`: `set_pipeline(kLitSolid)` + `set_light_params` + `set_solid_color` from mesh paint.
3. [x] 2D kinds unchanged (`kSolid` / `kTextured` / `kAuto`).
4. [x] Run scene Null tests green.

**Verify:** Lit path counters ≥1; paint albedo assertion; no atmosphere includes.

---

### Task 4: CPU frustum cull (GpuScene scale)

**Files:**
- Create: `src/render/scene/frustum_aabb.h`, `src/render/scene/frustum_aabb.cc`
- Modify: `src/render/scene/scene.cc` / `scene.h` (AABB on GpuMesh + cull in `record_kind`)
- Modify: `src/render/scene/scene_test.cc`, `src/render/scene/BUILD.gn`

**Steps:**
1. [x] Failing test: sync ≥64 AABB instances; set perspective camera whose frustum excludes half; `draw_indexed` call count ≈ visible half (via StubCommandList).
2. [x] Implement AABB vs frustum planes from `CameraMatrices` (or extract planes once per `record_draws`).
3. [x] Document: GPU BVH still Deferred (spec Out of scope).
4. [x] Run scene tests green.

**Verify:** Culled draws drop; full-frustum camera still draws all.

---

### Task 5: True GPU smoke greener

**Files:**
- Modify: `src/render/rhi/rhi_test.cc`
- Modify: `src/render/scene/scene_test.cc` and/or `unified_draw_test.cc`
- Modify: `src/render/README.md` (commands)
- Optional: this plan §Test commands

**Steps:**
1. [x] Under `SMT_RUN_FLYCUBE_GPU=1`: after present clear, also draw a tiny lit triangle or record GpuScene terrain once; log `lit ok` / existing ok lines.
2. [x] On `initialize` false: print skip, return success (no red).
3. [x] Document in `src/render/README.md`:

```bat
set SMT_RUN_FLYCUBE_GPU=1
ninja -C out rhi_test
ninja -C out scene_gpu_test
```

4. [x] Run once on a FlyCube machine if available; paste outcome in PR/notes (not required in CI).

**Verify:** Default env still green; with env=1 either ok or skip.

---

### Task 6: Style pathway regression + host seam note

**Files:**
- Modify: `src/legacy/render/bridge/leftover_record_test.cc` only if regression fails (prefer not churn)
- Modify: spec Success criteria checkboxes when done
- Optional doc note in `docs/build/src-layout.md` render row — only if README-worthy

**Steps:**
1. [x] Re-run `leftover_record_test` style_document P0 paint case.
2. [x] Confirm 3D paint path documented in spec (caller: `set_instance_paint` after `sync_from`; map host may set from `MapLayer::style_document`).
3. [x] Mark Success criteria in design spec `[x]`.

**Verify:** No style DLL cycles; render still only consumes `ResolvedPaint`.

**Task 6 regression (2026-09-20, Null default):** `rhi_test` / `scene_gpu_test` / `leftover_record_test` / `unified_draw_test` all exit 0 (`ok`; GPU paths skip without `SMT_RUN_FLYCUBE_GPU`). No code fix required.

---

## Test commands

Exact ninja output names (and GN labels):

| Ninja target | GN label | Defined in |
| --- | --- | --- |
| `rhi_test` | `//src/render:rhi_test` | `src/render/BUILD.gn` |
| `scene_gpu_test` | `//src/render/scene:scene_gpu_test` | `src/render/scene/BUILD.gn` |
| `unified_draw_test` | `//src/render/scene:unified_draw_test` | `src/render/scene/BUILD.gn` |
| `leftover_record_test` | `//src/legacy/render/bridge:leftover_record_test` | `src/legacy/render/bridge/BUILD.gn` |

There is **no** `scene_test` ninja target (sources live in `scene_test.cc`, binary is `scene_gpu_test`). There is **no** `src/render/rhi/BUILD.gn`.

```bat
REM CI / default (Null)
ninja -C out rhi_test
ninja -C out scene_gpu_test
ninja -C out unified_draw_test
ninja -C out leftover_record_test

REM Optional real GPU (skip-not-red if no adapter)
set SMT_RUN_FLYCUBE_GPU=1
ninja -C out rhi_test
ninja -C out scene_gpu_test
ninja -C out unified_draw_test
```

---

## Path ownership (parallel agents)

| This plan | Atmosphere upgrade (parallel) |
| --- | --- |
| `src/render/rhi/**` (LightParams, kLitSolid only) | Prefer not touch rhi except wind `PipelineId` — **serialize** Facade enum edits |
| `src/render/scene/**` | Avoid `atmosphere` insert points unless needed for load-op |
| Tests under render/scene + rhi | `src/render/atmosphere/**`, `src/gis/atmosphere/**` |
| Docs this design/plan | `2026-09-20-atmosphere-ocean-cloud-upgrade.md` |

If both need `rhi.h` enum changes: land lit solid first or merge enum values in one coordinated edit.

---

## Self-review

- [x] No TBD placeholders in tasks
- [x] Ocean/cloud Ids not renumbered
- [x] YAGNI: no PBR/shadows
- [x] Each task has failing test → implement → verify
