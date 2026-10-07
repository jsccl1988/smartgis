---
name: harness-auto-scene3d-frame-opt
description: >-
  Profiles every Scene3d / src_render frame (phase clocks + timed presents)
  then loop-drives optimizations in content present, effect/scene, GpuScene,
  and src/render until equal-profile budgets. Use when the user invokes
  /harness-auto-scene3d-frame-opt, or says scene3d 逐帧 profile, 每一帧性能,
  src/render 3d 优化 loop, ms_per_present, rebuild_count, atmosphere-showcase
  perf, ocean_prep_ms / record_ms, or asks to profile-then-optimize Scene3d
  frame by frame.
---

<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Harness auto scene3d frame opt（逐帧 profile → loop 优化）

**对 Scene3d / `src/render` 端到端渲染的每一帧做性能 profile，并 loop 驱动优化直至 equal-profile 预算。**

Sibling of `harness-auto-world3d-opt` (backend **matrix**). This skill owns the **per-frame** profile + fix loop on the product Vista path (`--atmosphere-showcase=legacy` locked profile), not the GL/D3D leftover grid.

Closed loop: build → locked-profile timed presents → parse hot phase → CBM → root-cause fix → rebuild → re-bench → until done bar / hard stop.

## Authorization

When this skill is invoked, attached (`@harness-auto-scene3d-frame-opt` / `/harness-auto-scene3d-frame-opt`), or followed, the agent **MUST** run the frame bench, iterate fixes, and re-bench — do not defer the loop to the user.

## Hard rules

1. **Equal profile (locked):** `--atmosphere-showcase=legacy` (`kLegacyStereo`, ocean on, sky off), viewport **640×480**. Do **not** strip ocean / hypsometric DEM to fake ms.
2. **Fair metrics:** leftover HWND wall ≠ product `ms_per_present` work. Compare **phase columns** + `rebuild_count`; product default remains Vista/DX12.
3. Prefer **`build.bat debug //src/app/views:views`**. Compile lock stays **OFF**. Stay on **`master`**.
4. CBM first (`user-codebase-memory-mcp`, project `smartgis`) before repo-wide Grep.
5. Prefer `*.inspect.png` for visual gate (`Read`).
6. Hypothesis first — one hot phase per iteration; no shotgun edits.
7. Timed benches: `ATMOSPHERE_SHOWCASE_LINGER_MS=0` and `PRESENT_COUNT>3` (pump_ms forced **0** — do not reintroduce PeekMessage/map2d paint into the timed window).

## Locked profile + budgets (Debug)

| Axis | Value |
| --- | --- |
| Entry | `SmartGIS.exe --atmosphere-showcase=legacy` |
| Viewport | 640×480 (`kAtmosphereShowcaseW/H`) |
| GPU | `ATMOSPHERE_SHOWCASE_GPU=1` |
| Timed frames | `ATMOSPHERE_SHOWCASE_PRESENT_COUNT=30` |
| Linger | `ATMOSPHERE_SHOWCASE_LINGER_MS=0` |
| Warm `ms_per_present` | leftover order **~10 ms** (Debug); current bar ~**14 ms** acceptable while Gerstner on |
| `rebuild_count` | **0** on timed frames |
| Visual | legacy landish / black-clear gate still PASS |

Plan / §: `docs/superpowers/plans/2026-10-01-src-render-scene3d-equal-profile-optimize.md` · living `docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md` §src_render Scene3d equal-profile optimize.

## Entry commands

From repo root:

```bat
.\build.bat debug //src/app/views:views
set ATMOSPHERE_SHOWCASE_PRESENT_COUNT=30
set ATMOSPHERE_SHOWCASE_GPU=1
set ATMOSPHERE_SHOWCASE_LINGER_MS=0
.\out\Debug\SmartGIS.exe --atmosphere-showcase=legacy
type .\out\Debug\captures\atmosphere\atmosphere-showcase-perf.json
```

Optional product world3d path (not the locked equal-profile; use after legacy is green):

```bat
.\out\Debug\SmartGIS.exe --plugin-showcase=world3d
```

M4 full-materials matrix (product atmo on; **separate** from bare world3d peer ranking):

```bat
py -3 testing/tools/harness/plugin/run_world3d_full_materials_matrix.py --row flycube
type .\out\Debug\captures\analysis\world3d_opt\matrix\full_materials\MATRIX.md
```

Phase fields (`ocean_prep_ms` / `record_ms` / `present_swap_ms` / `rebuild_count`) match this skill’s atmosphere JSON — reuse the same hot-phase pick order when optimizing full-materials rows.
Deep spans:

```bat
set TRACE=1
```

Artifacts:

| Artifact | Role |
| --- | --- |
| `out/Debug/captures/browser/atmosphere-showcase-perf.json` | `ms_per_present` + last-frame phases |
| `out/Debug/captures/browser/*.bmp` (+ `.inspect.png`) | Visual gate |
| stderr `atmosphere-showcase:` marks | Warmup / present / fail |

## Workflow

Copy and track:

```
Scene3d frame-opt progress:
- [ ] 1. Build SmartGIS
- [ ] 2. Baseline: legacy 640×480 GPU · PRESENT_COUNT=30 · LINGER=0
- [ ] 3. Parse atmosphere-showcase-perf.json → pick hottest phase
- [ ] 4. CBM → root-cause in scene3d present / effect/scene / GpuScene / render
- [ ] 5. Fix (one hypothesis) → rebuild → re-bench
- [ ] 6. Repeat until budgets or hard stop
- [ ] 7. Emit before/after phase table + inspect note
```

### Step 1 — Build

```bat
.\build.bat debug //src/app/views:views
```

### Step 2 — Baseline bench

Run the entry commands. Read `atmosphere-showcase-perf.json` and stderr.

If multi-backend A/B is needed, run sibling `harness-auto-world3d-opt` — do **not** treat leftover GL/D3D wall as the product done bar.

### Step 3 — Parse hot phase

From JSON:

| Metric | Baseline | After |
| --- | ---: | ---: |
| `ms_per_present` | | |
| `mesh_ms` | | |
| `sync_ms` | | |
| `rebuild_ms` | | |
| `rebuild_count` | | |
| `ocean_prep_ms` | | |
| `record_ms` | | |
| `present_swap_ms` | | |

**Hot phase pick order:** if `rebuild_count>0` on timed frames → remesh/dirty first; else largest phase ms on the warm path.

### Step 4 — Fix targets (by phase)

| Hot phase | Prefer code under |
| --- | --- |
| `rebuild_count` / `rebuild_ms` / `mesh_ms` | `vista/scene/**` (`GpuScene`), `content/.../scene3d/**` remesh dirty flags |
| `sync_ms` | DEM / overlay sync; avoid clear-before-cache |
| `ocean_prep_ms` | OceanPass prepare_gpu; one-shot after DEM sync |
| `record_ms` | Pass record / Gerstner step; `render/graph/**` |
| `present_swap_ms` | `Scene3dGpuPresent`, RHI present |
| Wall >> phase sum | Accidental message pump / map2d paint in timed window |

Non-goals: delete ocean/DEM; force leftover as product default; fake Null-RHI wins as GPU green.

### Step 5 — Loop

1. State hypothesis (one sentence) tied to the hot phase.
2. Patch root cause.
3. Rebuild Views.
4. Re-run the same locked-profile bench.
5. Update before/after table.
6. Stop on **done bar** or **hard stop**.

### Done bar

Debug legacy 640×480 GPU:

- timed frames `rebuild_count=0`
- warm `ms_per_present` in leftover **~10 ms** order (Debug; ~14 ms OK while animated ocean)
- visual legacy BMP still PASS (landish / black-clear)

### Hard stops

- Same hot phase unchanged **3+** iterations
- Visual gate broken after a “perf” change
- Crash / AV / hang → `auto-diagnose-fix`, then return here

## Communication

- Progress and tables in **简体中文**
- Paths / env / metrics in **English** identifiers
- Lead with before/after phase table + hottest phase + next hypothesis

## Related

- Matrix sibling: `.cursor/skills/harness-auto-world3d-opt/SKILL.md`
- Map2d frame sibling: `.cursor/skills/harness-auto-map2d-frame-opt/SKILL.md`
- Visual review: `.cursor/skills/harness-visual-review/SKILL.md`
- Timed present: `src/plugin/product/world3d/scenario/atmosphere/present/present_run.cc`
- Phase clocks: `src/content/browser/present/scene3d/scene3d_phase_profile.*`
- Detail: [reference.md](reference.md)
