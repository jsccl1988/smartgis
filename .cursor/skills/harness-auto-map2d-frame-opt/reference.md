<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# harness-auto-map2d-frame-opt — reference

Progressive disclosure. Read when parsing logs, picking a hot phase, or wiring SMT_TRACE.

## Frame model

```
SmartGIS.exe --map2d-showcase=china
        │
        ├─ seed / layout (Map2dFrameCache)
        │     layout_ms · hillshade_ms (lump; sub-phases = HillshadeBakeSample / cat=bake)
        │
        ├─ software export_bmp (optional)
        │     software_paint_ms · bmp_io_ms · paint_ms
        │
        ├─ FlyCube present_gpu
        │     cold: upload + present
        │     warm StaticReuse: present_gpu_warm_ms · gpu_skip vs gpu_full
        │
        └─ optional FPS bench (SMT_MAP2D_FPS_BENCH_MS)
              mean/peak fps · layout_builds_delta · action_r/i/s/st
```

## Phase clocks (normative)

Owned by `content::Map2dPhaseSample` (`map2d_phase_profile.*`). Showcase logs:

| Tag | Meaning |
| --- | --- |
| `phase_export` | After software export |
| `phase_cold_present` | First FlyCube present after reset |
| `phase_warm_present` | Second present (StaticReuse expected) |

Gate idea (matrix): phase sum ≈ export / cold present within ±15%.

## FPS bench fields (`map2d-fps-bench.txt`)

| Field | Meaning |
| --- | --- |
| `mean_fps` / `peak_fps` | HUD samples after 250 ms settle |
| `layout_builds` / `layout_builds_delta` | Frame-cache rebuilds during bench window |
| `gpu_skip` / `gpu_full` / `skip_pct` | `Map2dGpuPresentProfile` |
| `action_rebuild` / `interactive` / `settle` / `static` | Dual-speed present actions |

Warm past dual-speed settle (~200–350 ms pump) before timed samples — already in `fps_bench.cc`.

## Env knobs

| Env | Role |
| --- | --- |
| `SMT_MAP2D_SHOWCASE_W/H` | `1280` / `720` (locked) |
| `SMT_MAP2D_SHOWCASE_GPU` | `1` = FlyCube present |
| `SMT_MAP2D_FPS_BENCH_MS` | >0 enables FPS sample loop |
| `SMT_MAP2D_EXPORT_REUSE` | `1` = bench-only warm paint/blit |
| `SMT_TRACE` / `SMT_BAKE_PROFILE` | Chrome-trace / RenderTrace; bake spans use **`cat=bake`** (not `startup`) |
| `SMT_BAKE_BACKEND` | `auto` \| `cpu` \| `cuda` — equal-profile bake bench |
| `SMT_BAKE_BENCH` | `1` = `dem_raster_test` / `land_mask_test` write `captures/analysis/hillshade_bake/` |
| `SMT_MAP_FPS_LOG` | Extra map FPS logging when wired |

## Fix heuristics

| Symptom | Likely root |
| --- | --- |
| Warm present still hundreds of ms | Invalidating frame cache every present; device recreate; Pass upload not kept |
| `layout_builds_delta` >> 0 during FPS bench | Overlay / identity sync churn; shell paint forcing rebuild |
| High `hillshade_ms` every frame | Shade RGBA / DEM overview not cached by path+LOD — split with `HillshadeBakeSample` (`cat=bake`) |
| High `gpu_upload_ms` on warm | StaticReuse not taken; graph rebuild every frame |
| Low FPS but warm present_ms tiny | Main-thread pump / shell paint outside present (compare wall vs phase) |

## vs matrix skill

| Concern | This skill | `harness-auto-map2d-opt` |
| --- | --- | --- |
| Leftover×port grid | Optional A/B only | Primary |
| Every-frame StaticReuse | Primary | Secondary (warm column) |
| FPS bench | Required when optimizing warm | Optional |
| Done bar | Equal-profile phase budgets | Screenshots + comparison table |

## Example reply skeleton

```markdown
## Map2d 逐帧 profile

配置：Debug · china · 1280×720 · GPU=1 · FPS_BENCH_MS=3000

### Before → After
| metric | before | after |
| --- | ---: | ---: |
| present_gpu_warm_ms | | |
| paint_ms | | |
| layout_ms | | |
| mean_fps | | |
| layout_builds_delta | | |

### Hot phase
`gpu_upload_ms` — hypothesis: …

### Next
rebuild → re-bench → …
```
