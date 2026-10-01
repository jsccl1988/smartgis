<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# src_render Map2d equal-profile optimize — Implementation Plan

> Checklist hung off living [`../specs/2026-09-13-render-rhi-scene-design.md`](../specs/2026-09-13-render-rhi-scene-design.md) §src_render Map2d equal-profile optimize.

**Goal:** Under the same china 1280×720 profile as leftover rhi2d matrix, bring **warm** `src_render` paint cost into the same order of magnitude as leftover `execute_ms` (~50–80 ms), without dropping hillshade / casing richness.

**Architecture:** Keep Views `Map2dPresenter` → `Map2dFrameCache` → software `export_bmp` / FlyCube `present_gpu` (`effect/map` + `render::graph`). Optimize by **fair phase timing**, **warm-path reuse**, **DEM/hillshade cache**, and **layout/upload incrementalism** — not by stripping carto.

**Tech Stack:** C++23, existing content/map2d + effect/map + render/rhi; harness `run_parallel_port_matrix.py`.

## Baseline (2026-10-01 matrix, Debug)

| Path | Metric | ms | Notes |
| --- | --- | ---: | --- |
| leftover serial/tile/layer × gdi | `execute_ms_max` | ~48–50 | IR replay only; no DEM hillshade |
| src_render software | `export_ms` | ~244 | `clear_present_cache` + full `paint` + BMP IO |
| src_render FlyCube | `present_gpu_ms` | ~1521 | cold first present after invalidate (layout+upload) |

Artifacts: `out/Debug/captures/map2d/matrix/parallel_port_matrix_with_src_render.csv`, `src_render-china.inspect.png`.

## Tasks

### Task 1: Equal-profile phase clocks (fair compare)

- [x] Split logs: `layout_ms` / `hillshade_ms` / `software_paint_ms` / `bmp_io_ms` / `gpu_upload_ms` / `gpu_present_ms`
- [x] Matrix CSV columns for those phases; document leftover `execute_ms` ≠ export_ms
- [x] Report **warm** present (2nd+ frame, StaticReuse) separately from cold first frame
- [x] Gate: matrix row still green; phases sum ≈ export / present within ±15% (src_render `phase_gate_export` / `phase_gate_cold`)

### Task 2: Cold present_gpu crash diet

- [x] Showcase / matrix: reuse one `rhi::Device` across present samples (do not create/shutdown per matrix cell if already warm)
- [x] Avoid `invalidate_frame_cache()` immediately before timed present when size/extent unchanged
- [x] Keep Pass uploads on StaticReuse / InteractiveReuse (already partially done)
- [ ] Target: warm `present_gpu_ms` ≤ **80** on china 1280×720 Debug (**met**: warm=0); cold first ≤ **400** after layout cache warm (**open**: ~2.6–2.8s dominated by `gpu_upload_ms` → Task 5)

### Task 3: export_bmp / software path

- [x] Optional `SMT_MAP2D_EXPORT_REUSE=1`: allow present-cache blit when cam+size match (bench only; default export stays faithful)
- [x] Time-exclude BMP file write from paint metric (`paint_ms` vs `export_ms`)
- [x] Hillshade: cache `shade_dem_rgba` by DEM path + illumination + viewport LOD; overview uses downsampled DEM
- [x] Target: `paint_ms` ≤ **100**; `export_ms` (incl IO) ≤ **150** on same profile (**met** with `SMT_MAP2D_EXPORT_REUSE=1`: paint≈3 / export≈9)

### Task 4: Layout / MapFrame cost

- [ ] Profile `Map2dFrameCache` rebuild vs reuse on china frame; cut duplicate feature walks
- [ ] Overview LOD: skip or simplify low-importance roads/labels earlier (fblc / zoom already partially there)
- [ ] Optional: parallel prep of layout batches (same grain idea as leftover prep pipeline; serial emit)
- [ ] Target: `layout_ms` ≤ **60** on cold china; near-zero on StaticReuse

### Task 5: GPU upload / effect/map

- [ ] Batch small line/fill uploads; prefer one hillshade texture + instanced vectors
- [ ] Async / double-buffer upload where FlyCube allows without tearing product API
- [ ] Target: warm `gpu_upload_ms` ≈ 0 on StaticReuse; cold upload ≤ **200** after Task 3 hillshade cache

### Task 6: Matrix acceptance

- [ ] `run_parallel_port_matrix.py` prints leftover vs src_render phase table
- [ ] Visual: `src_render-china` still shows hillshade + labels (score / inspect)
- [ ] Doc as-built one-liner in `docs/build/ui-testing.md` when landed

## Non-goals

- Matching leftover “IR-only” ms by deleting hillshade / MapFrame
- Porting MapLibre Native shaders
- Making leftover the product default again
