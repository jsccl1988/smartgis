---
name: harness-auto-scene3d-phase-loop
description: >-
  Product Scene3d phase-budget loop: profile every frame (phase clocks +
  timed presents) and fix content present / effect/scene / GpuScene /
  src/render until warm ms_per_present and rebuild budgets. Fixes must
  preserve current Scene3d visuals and stay scene-agnostic (no atmosphere /
  legacy / world3d / flycube / DEM-model specialized shortcuts). Not the
  world3d port matrix. Use when the user invokes
  /harness-auto-scene3d-phase-loop (formerly /harness-auto-scene3d-frame-opt),
  or says scene3d 逐帧 profile, 每一帧性能, src/render 3d 优化 loop,
  ms_per_present, rebuild_count, atmosphere-showcase perf, ocean_prep_ms /
  record_ms, or asks to profile-then-optimize Scene3d frame by frame.
---

<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Harness auto scene3d phase loop（产品路径逐帧相位 → 修到预算）

**对产品 Scene3d 每一帧做相位 profile，并 loop 驱动优化直至相位预算。** 优化不得改变当前场景展现效果；修法必须走通用路径，禁止针对某 showcase / 引擎 / DEM 模型的特化。

Sibling of `harness-auto-world3d-port-matrix` (Scenic GL / Scenic D3D11 / Vista **matrix**). This skill owns the **per-frame** profile + fix loop on the product Vista path (`--atmosphere-showcase=legacy` as **bench fixture only**). Port-grid screenshots and bare `ms_per_present` ranks belong to the matrix skill.

Closed loop: build → locked-profile timed presents → parse hot phase → CBM → **generic** root-cause fix → rebuild → re-bench (+ visual gate) → until done bar / hard stop.

## Authorization

When this skill is invoked, attached (`@harness-auto-scene3d-phase-loop` / `/harness-auto-scene3d-phase-loop`, formerly `harness-auto-scene3d-frame-opt`), or followed, the agent **MUST** run the frame bench, iterate fixes, and re-bench — do not defer the loop to the user.

## Hard rules

1. **Equal profile (locked):** `--atmosphere-showcase=legacy` (`kLegacyStereo`, ocean on, sky off), viewport **640×480**. Do **not** strip ocean / hypsometric DEM to fake ms. `legacy` / atmosphere is only the **bench fixture**, not a license to specialize product code for that showcase or DEM.
2. **Visual fidelity (non-negotiable):** Perf fixes must **not** change Scene3d presentation — same ocean / Gerstner / landish / DEM hypsometric / lighting / clear policy, and inspect pixels within the visual gate. No thinning materials, dropping ocean/sky passes, lowering mesh LOD or wave quality, or “good enough” looks to win ms.
3. **Generic only — no scene-/model-specialized opts:** Optimize **shared** Scene3d / present / effect/scene / GpuScene / render paths that apply to **any** scene document, camera, and engine. Forbidden: hard-coded `atmosphere-showcase` / `legacy` / `world3d` / `flycube` / named DEM / envelope shortcuts, `if (showcase==legacy)`, fixture-only caches, or sample-path branches that other Scene3d content would not take.
4. **Fair metrics:** leftover HWND wall ≠ product `ms_per_present` work. Compare **phase columns** + `rebuild_count`; product default remains Vista/DX12.
5. Prefer **`build.bat debug //src/app/views:views`**. Compile lock stays **OFF**. Stay on **`master`**.
6. CBM first (`user-codebase-memory-mcp`, project `smartgis`) before repo-wide Grep.
7. Prefer `*.inspect.png` for visual gate (`Read`).
8. Hypothesis first — one hot phase per iteration; no shotgun edits.
9. Timed benches: `ATMOSPHERE_SHOWCASE_LINGER_MS=0` and `PRESENT_COUNT>3` (pump_ms forced **0** — do not reintroduce PeekMessage/map2d paint into the timed window).

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
| Visual | legacy landish / black-clear gate still PASS; **pixel/present parity** vs pre-fix baseline (no intentional visual change) |

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

Phase fields (`ocean_prep_ms` / `record_ms` / `present_swap_ms` / `rebuild_count`) match this skill’s atmosphere JSON — reuse the same hot-phase pick order when optimizing full-materials rows. Fixes on those rows must still be **scene-agnostic** (shared stack only).
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
- [ ] 5. Fix (one hypothesis, path-generic) → rebuild → re-bench + inspect
- [ ] 6. Repeat until budgets or hard stop
- [ ] 7. Emit before/after phase table + inspect note
```

### Step 1 — Build

```bat
.\build.bat debug //src/app/views:views
```

### Step 2 — Baseline bench

Run the entry commands. Read `atmosphere-showcase-perf.json` and stderr. Keep a baseline inspect BMP for visual parity.

If multi-backend A/B is needed, run sibling `harness-auto-world3d-port-matrix` — do **not** treat leftover GL/D3D wall as the product done bar.

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

Non-goals: delete ocean/DEM; force leftover as product default; fake Null-RHI wins as GPU green; **any atmosphere-/legacy-/world3d-/flycube-/DEM-model-specialized fast path**; intentional visual quality downgrades for ms.

### Step 5 — Loop

1. State hypothesis (one sentence) tied to the hot phase; confirm the fix is **path-generic** (would help a different Scene3d document / engine equally).
2. Patch root cause — algorithm / cache / dirty flags / reuse / upload / remesh churn on the **shared** stack only.
3. Rebuild Views.
4. Re-run the same locked-profile bench; **re-check inspect** (landish / ocean / black-clear + no obvious visual regress vs baseline).
5. Update before/after table.
6. Stop on **done bar** or **hard stop**.

### Done bar

Debug legacy 640×480 GPU (**fixture only**; fixes remain scene-agnostic):

- timed frames `rebuild_count=0`
- warm `ms_per_present` in leftover **~10 ms** order (Debug; ~14 ms OK while animated ocean)
- visual legacy BMP still PASS (landish / black-clear); **no intentional present/material change** vs baseline
- Diff review: no showcase-/legacy-/world3d-/flycube-/DEM-gated product branches added for the win

### Hard stops

- Same hot phase unchanged **3+** iterations
- Visual gate broken (landish / ocean / clear / material regress) after a “perf” change
- Proposed fix is scene-/showcase-/model-specialized (revert; find a generic root cause)
- Crash / AV / hang → `auto-diagnose-fix`, then return here

## Communication

- Progress and tables in **简体中文**
- Paths / env / metrics in **English** identifiers
- Lead with before/after phase table + hottest phase + next hypothesis
- Explicitly note that the fix is **通用路径** and that inspect parity held

## Related

- Matrix sibling: `.cursor/skills/harness-auto-world3d-port-matrix/SKILL.md`
- Map2d phase-loop sibling: `.cursor/skills/harness-auto-map2d-phase-loop/SKILL.md`
- Visual review: `.cursor/skills/harness-visual-review/SKILL.md`
- Timed present: `src/content/browser/present/scene3d/profile/scene3d_phase_profile.*` (suite `export_bmp` / `camera_fly`; no harness present runner)
- Phase clocks: `src/content/browser/present/scene3d/profile/scene3d_phase_profile.*`
- Detail: [reference.md](reference.md)
