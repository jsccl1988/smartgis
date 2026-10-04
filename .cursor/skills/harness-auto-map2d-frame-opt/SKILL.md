---
name: harness-auto-map2d-frame-opt
description: >-
  Profiles every Map2d / src_render frame (phase clocks + FPS bench) then
  loop-drives optimizations in content present, effect/map, and src/render
  until equal-profile budgets. Use when the user invokes
  /harness-auto-map2d-frame-opt, or says map2d 逐帧 profile, 每一帧性能,
  src/render 2d 优化 loop, present_gpu warm, StaticReuse, layout_ms /
  hillshade_ms, SMT_MAP2D_FPS_BENCH_MS, or asks to profile-then-optimize
  Map2d frame by frame.
---

<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Harness auto map2d frame opt（逐帧 profile → loop 优化）

**对 Map2d / `src/render` 端到端渲染的每一帧做性能 profile，并 loop 驱动优化直至 equal-profile 预算。**

Sibling of `harness-auto-map2d-opt` (backend×parallel **matrix**). This skill owns the **per-frame** profile + fix loop on the product path, not the leftover×port grid.

Closed loop: build → locked-profile bench → parse hot phase → CBM → root-cause fix → rebuild → re-bench → until done bar / hard stop.

## Authorization

When this skill is invoked, attached (`@harness-auto-map2d-frame-opt` / `/harness-auto-map2d-frame-opt`), or followed, the agent **MUST** run the frame bench, iterate fixes, and re-bench — do not defer the loop to the user.

## Hard rules

1. **Equal profile (locked):** China mainland `[80,16]–[128,52]`, viewport **1280×720**, same sample / style richness (hillshade when DEM present). Do **not** strip carto to fake ms.
2. **Fair metrics:** leftover `execute_ms` ≠ `export_ms` / `present_gpu_*`. Optimize against **phase columns** + warm StaticReuse, never claim IR ≡ product paint.
3. Prefer **`build.bat debug //src/app/views:views`**. Compile lock stays **OFF**. Stay on **`master`**.
4. CBM first (`user-codebase-memory-mcp`, project `smartgis`) before repo-wide Grep.
5. Prefer `*.inspect.png` for visual gate (`Read`).
6. Hypothesis first — one hot phase per iteration; no shotgun edits.

## Locked profile + budgets (Debug)

| Axis | Value |
| --- | --- |
| Entry | `SmartGisViews.exe --map2d-showcase=china` |
| Viewport | `SMT_MAP2D_SHOWCASE_W/H=1280/720` |
| GPU | `SMT_MAP2D_SHOWCASE_GPU=1` |
| Warm FPS window | `SMT_MAP2D_FPS_BENCH_MS=3000` (optional) |
| Warm `present_gpu_ms` | ≤ **80** |
| `paint_ms` | ≤ **100** |
| Cold first present | ≤ **400** after layout warm |
| Visual | hillshade + city labels still on inspect |

Plan / §: `docs/superpowers/plans/2026-10-01-src-render-map2d-equal-profile-optimize.md` · living `docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md` §src_render Map2d equal-profile optimize.

## Entry commands

From repo root:

```bat
.\build.bat debug //src/app/views:views
set SMT_MAP2D_SHOWCASE_W=1280
set SMT_MAP2D_SHOWCASE_H=720
set SMT_MAP2D_SHOWCASE_GPU=1
set SMT_MAP2D_FPS_BENCH_MS=3000
.\out\Debug\SmartGisViews.exe --map2d-showcase=china
```

Optional fair cold/warm export (bench-only reuse):

```bat
set SMT_MAP2D_EXPORT_REUSE=1
```

Deep spans (when phase clocks are not enough):

```bat
set SMT_TRACE=1
rem chrome-trace dump under out/Debug/captures/ (see base::trace / RenderTrace panel)
```

Artifacts (typical):

| Artifact | Role |
| --- | --- |
| `out/Debug/captures/map2d/map2d-showcase-china.bmp` (+ `.inspect.png`) | Visual gate |
| `out/Debug/captures/map2d/map2d-fps-bench.txt` | mean/peak FPS · layout_builds · gpu_skip/full · action_* |
| stderr `map2d-showcase: phase_*` / `present_gpu_warm_ms` | Phase clocks |
| Matrix CSV (optional A/B) | `out/Debug/captures/map2d/matrix/parallel_port_matrix_with_src_render.csv` |

## Workflow

Copy and track:

```
Map2d frame-opt progress:
- [ ] 1. Build SmartGisViews
- [ ] 2. Baseline: china 1280×720 GPU + FPS bench
- [ ] 3. Parse phases + fps-bench.txt → pick hottest phase
- [ ] 4. CBM → root-cause in present / effect/map / render/{rhi,graph}
- [ ] 5. Fix (one hypothesis) → rebuild → re-bench
- [ ] 6. Repeat until budgets or hard stop
- [ ] 7. Emit before/after phase table + inspect note
```

### Step 1 — Build

```bat
.\build.bat debug //src/app/views:views
```

Missing `out\Debug\SmartGisViews.exe` → rebuild; do not skip.

### Step 2 — Baseline bench

Run the entry commands above. Capture stderr + `map2d-fps-bench.txt` + BMP.

If only matrix A/B is needed without FPS loop, also run sibling skill `harness-auto-map2d-opt` — do **not** confuse matrix wall with every-frame StaticReuse.

### Step 3 — Parse hot phase

From stderr / files, fill:

| Metric | Baseline | After |
| --- | ---: | ---: |
| `layout_ms` | | |
| `hillshade_ms` | | |
| `software_paint_ms` / `paint_ms` | | |
| `bmp_io_ms` | | |
| `gpu_upload_ms` | | |
| `gpu_present_ms` / `present_gpu_warm_ms` | | |
| mean_fps / peak_fps | | |
| `layout_builds_delta` | | |
| `gpu_skip` / `gpu_full` / `skip_pct` | | |
| `action_rebuild` / `interactive` / `settle` / `static` | | |

**Hot phase pick order:** largest ms that is still on the warm path (StaticReuse). If `layout_builds_delta` climbs during FPS bench → layout churn is the bug, not GPU present.

### Step 4 — Fix targets (by phase)

| Hot phase | Prefer code under |
| --- | --- |
| `layout_ms` / builds | `content/browser/present/map2d/**` (`Map2dFrameCache`, presenter) |
| `hillshade_ms` | `vista/map/**`, DEM shade cache |
| `software_paint_ms` | map2d software export / paint path |
| `gpu_upload_ms` | `content/.../map2d/gpu/**`, `render/graph/**`, `render/rhi/**` |
| `gpu_present_ms` / low `skip_pct` | `Map2dGpuPresent`, StaticReuse / dual-speed settle |
| Frame graph / RHI | `src/render/{graph,rhi}/**` |

Non-goals: delete hillshade/MapFrame; MapLibre Native port; make leftover the product default.

### Step 5 — Loop

1. State hypothesis (one sentence) tied to the hot phase.
2. Patch root cause.
3. `build.bat debug //src/app/views:views` (or focused target if known).
4. Re-run the same locked-profile bench.
5. Update the before/after table.
6. Stop when **done bar** or **hard stop**.

### Done bar

Debug china 1280×720:

- warm `present_gpu_ms` ≤ **80**
- `paint_ms` ≤ **100**
- cold first present ≤ **400** after layout warm
- FPS bench: `layout_builds_delta` near **0** on StaticReuse window; visual hillshade + labels on inspect

### Hard stops

- Same failure / same hot phase unchanged **3+** iterations
- Visual gate broken (no hillshade / labels) after a “perf” change
- Crash / AV → hand off `windbg-crash-diagnose`, then return here

## Communication

- Progress and tables in **简体中文**
- Paths / env / metrics in **English** identifiers
- Lead with before/after phase table + hottest phase + next hypothesis

## Related

- Matrix sibling: `.cursor/skills/harness-auto-map2d-opt/SKILL.md`
- Scene3d frame sibling: `.cursor/skills/harness-auto-scene3d-frame-opt/SKILL.md`
- Visual review: `.cursor/skills/harness-visual-review/SKILL.md`
- FPS bench: `src/app/views/shell/harness/showcase/map2d/present/fps_bench.*`
- Phase clocks: `src/content/browser/present/map2d/map2d_phase_profile.*`
- Detail: [reference.md](reference.md)
