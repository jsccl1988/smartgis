---
name: harness-auto-map2d-phase-loop
description: >-
  Product Map2d phase-budget loop: profile every frame (phase clocks + FPS
  bench) and fix content present / effect/map / src/render until warm
  present_gpu and paint budgets. Fixes must preserve map visuals and stay
  map-agnostic (no china/showcase-specialized shortcuts). Not the Scenic
  port matrix. Use when the user invokes /harness-auto-map2d-phase-loop
  (formerly /harness-auto-map2d-frame-opt), or says map2d 逐帧 profile,
  每一帧性能, src/render 2d 优化 loop, present_gpu warm, StaticReuse,
  layout_ms / hillshade_ms, MAP2D_FPS_BENCH_MS, or asks to
  profile-then-optimize Map2d frame by frame.
---

<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Harness auto map2d phase loop（产品路径逐帧相位 → 修到预算）

**对产品 Map2d 每一帧做相位 profile，并 loop 驱动优化直至相位预算。** 优化不得改变地图展现效果；修法必须走通用路径，禁止针对某张样例图的特化。

Sibling of `harness-auto-map2d-port-matrix` (Scenic rhi2d GDI/GDI+/Skia × parallel **matrix**). This skill owns the **per-frame** profile + fix loop on the product path (`SmartGIS.exe --map2d-showcase=china` as **bench fixture only**). Port-grid screenshots and `execute_ms` belong to the matrix skill.

Closed loop: build → locked-profile bench → parse hot phase → CBM → **generic** root-cause fix → rebuild → re-bench (+ visual gate) → until done bar / hard stop.

## Authorization

When this skill is invoked, attached (`@harness-auto-map2d-phase-loop` / `/harness-auto-map2d-phase-loop`, formerly `harness-auto-map2d-frame-opt`), or followed, the agent **MUST** run the frame bench, iterate fixes, and re-bench — do not defer the loop to the user.

## Hard rules

1. **Equal profile (locked):** China mainland `[80,16]–[128,52]`, viewport **1280×720**, same sample / style richness (hillshade when DEM present). Do **not** strip carto to fake ms. `china` is only the **bench fixture**, not a license to specialize product code for that map.
2. **Visual fidelity (non-negotiable):** Perf fixes must **not** change map presentation — same layers, symbology, hillshade, labels, LOD, and inspect pixels within the visual gate. No thinning style, dropping furniture, lowering DEM/shade quality, or “good enough” carts to win ms.
3. **Generic only — no map-specialized opts:** Optimize **shared** Map2d / present / effect / render paths that apply to **any** document and viewport. Forbidden: hard-coded china / showcase / envelope / CRS shortcuts, `if (showcase==china)`, fixture-only caches, or sample-path branches that other maps would not take.
4. **Fair metrics:** Scenic rhi2d `execute_ms` ≠ `export_ms` / `present_gpu_*`. Optimize against **phase columns** + warm StaticReuse, never claim IR ≡ product paint.
5. Prefer **`build.bat debug //src/app/views:views`**. Compile lock stays **OFF**. Stay on **`master`**.
6. CBM first (`user-codebase-memory-mcp`, project `smartgis`) before repo-wide Grep.
7. Prefer `*.inspect.png` for visual gate (`Read`).
8. Hypothesis first — one hot phase per iteration; no shotgun edits.

## Locked profile + budgets (Debug)

| Axis | Value |
| --- | --- |
| Entry | `MAP2D_PHASE_BENCH=china` + `SmartGIS.exe` (not retired `--map2d-showcase=`) |
| Viewport | `MAP2D_SHOWCASE_W/H=1280/720` |
| GPU | `MAP2D_SHOWCASE_GPU=1` |
| Warm FPS window | `MAP2D_FPS_BENCH_MS=3000` (optional) |
| Warm `present_gpu_ms` | ≤ **80** |
| `paint_ms` | ≤ **100** |
| Cold first present | ≤ **400** after layout warm |
| Visual | hillshade + city labels still on inspect; **pixel/present parity** vs pre-fix baseline (no intentional visual change) |

Plan / §: `docs/superpowers/plans/2026-10-01-src-render-map2d-equal-profile-optimize.md` · living `docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md` §src_render Map2d equal-profile optimize.

## Entry commands

From repo root. `--map2d-showcase=` is a **retired** argv extra (does not select a
suite). Prefer **product cold start** + `MAP2D_PHASE_BENCH` so china is seeded
before FlyCube attach (honest GPU StaticReuse). Debug has no CPython embed;
the C++ showcase runner (`map2d_showcase_run`) does seed → export_bmp →
present_gpu → fps.

```bat
.\build.bat debug src/app/views:views
set MAP2D_PHASE_BENCH=china
set MAP2D_SHOWCASE_W=1280
set MAP2D_SHOWCASE_H=720
set MAP2D_SHOWCASE_GPU=1
set MAP2D_FPS_BENCH_MS=3000
set MAP2D_FORCE_HILLSHADE=1
set FORCE_GDI_MAP_OVERLAY=0
.\out\Debug\SmartGIS.exe
```

CI harness (`HARNESS_SUITE=browser.map2d.china`) still works for marks/BMP; use
`FORCE_GDI_MAP_OVERLAY=1` there. Phase-loop FPS/`gpu_skip` prefer `MAP2D_PHASE_BENCH`.

Optional fair cold/warm export (bench-only reuse):

```bat
set MAP2D_EXPORT_REUSE=1
```

Deep spans (when phase clocks are not enough):

```bat
set TRACE=1
rem chrome-trace dump under out/Debug/captures/ (see base::trace / RenderTrace panel)
```

Artifacts (typical):

| Artifact | Role |
| --- | --- |
| `out/Debug/captures/browser/map2d-showcase-china.bmp` (+ `.inspect.png`) | Visual gate |
| `out/Debug/captures/browser/map2d-fps-bench.txt` | mean/peak FPS · layout_builds · gpu_skip/full · action_* |
| stderr `map2d-showcase: phase_*` / `present_gpu_warm_ms` | Phase clocks |
| Matrix CSV (optional A/B) | `out/Debug/captures/browser/matrix/parallel_port_matrix_with_src_render.csv` |

## Workflow

Copy and track:

```
Map2d phase-loop progress:
- [ ] 1. Build SmartGIS
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

Missing `out\Debug\SmartGIS.exe` → rebuild; do not skip.

### Step 2 — Baseline bench

Run the entry commands above. Capture stderr + `map2d-fps-bench.txt` + BMP.

If only the port-grid table is needed without an FPS loop, run sibling skill `harness-auto-map2d-port-matrix`. Matrix wall and Scenic `execute_ms` are a different measurement from every-frame StaticReuse.

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
| `hillshade_ms` | `vista/component/map/**`, DEM shade cache |
| `software_paint_ms` | map2d software export / paint path |
| `gpu_upload_ms` | `content/.../map2d/gpu/**`, `render/graph/**`, `render/rhi/**` |
| `gpu_present_ms` / low `skip_pct` | `Map2dGpuPresent`, StaticReuse / dual-speed settle |
| Frame graph / RHI | `src/render/{graph,rhi}/**` |

Non-goals: delete / thin hillshade or MapFrame; strip labels or carto furniture; MapLibre Native port; touch `src/legacy/` (frozen); **any china-/showcase-/envelope-specialized fast path**.

### Step 5 — Loop

1. State hypothesis (one sentence) tied to the hot phase; confirm the fix is **path-generic** (would help a different map equally).
2. Patch root cause — algorithm / cache / reuse / upload / layout churn on the **shared** stack only.
3. `build.bat debug //src/app/views:views` (or focused target if known).
4. Re-run the same locked-profile bench; **re-check inspect** (hillshade + labels + no obvious visual regress).
5. Update the before/after table.
6. Stop when **done bar** or **hard stop**.

### Done bar

Debug china 1280×720 (**fixture only**; fixes remain map-agnostic):

- warm `present_gpu_ms` ≤ **80**
- `paint_ms` ≤ **100**
- cold first present ≤ **400** after layout warm
- FPS bench: `layout_builds_delta` near **0** on StaticReuse window
- Visual: hillshade + labels on inspect; **no intentional present/style change** vs baseline
- Diff review: no showcase-/china-/bbox-gated product branches added for the win

### Hard stops

- Same failure / same hot phase unchanged **3+** iterations
- Visual gate broken (no hillshade / labels / style regress) after a “perf” change
- Proposed fix is map- or showcase-specialized (revert; find a generic root cause)
- Crash / AV / hang → hand off `auto-diagnose-fix`, then return here

## Communication

- Progress and tables in **简体中文**
- Paths / env / metrics in **English** identifiers
- Lead with before/after phase table + hottest phase + next hypothesis

## Related

- Port-matrix sibling: `.cursor/skills/harness-auto-map2d-port-matrix/SKILL.md`
- Scene3d phase-loop sibling: `.cursor/skills/harness-auto-scene3d-phase-loop/SKILL.md`
- Visual review: `.cursor/skills/harness-visual-review/SKILL.md`
- FPS bench: `src/content/browser/present/map2d/showcase/map2d_fps.*`
- Phase clocks: `src/content/browser/present/map2d/profile/map2d_phase_profile.*`
- Bake profile (CPU vs Thrust): `testing/tools/harness/browser/run_hillshade_bake_bench.py`
- Detail: [reference.md](reference.md)
