<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

> **Status: superseded** (2026-09-28 merge B). Merged into [`../../specs/2026-09-14-base-root-hybrid-design.md`](../../specs/2026-09-14-base-root-hybrid-design.md) — §execution / sync / concurrency (folded). Do not revise here except mechanical link fixes; revise the living umbrella in place.


# `src/base/execution` — mogu-aligned execution stack

**Date:** 2026-09-28  
**Status:** superseded (2026-09-28 merge B)
**Updated:** 2026-09-28 — stroke/GDI/ring-fan opts landed; remeasure note under Decision.  
**Considered living file:** [`2026-09-14-base-root-hybrid-design.md`](2026-09-14-base-root-hybrid-design.md) — covers foundation boundaries but not the execution tree; a dedicated living row is required (new subsystem).  
**Related:** [`../../build/mogu-mapping.md`](../../build/mogu-mapping.md), plan [`../plans/2026-09-28-base-execution.md`](../plans/2026-09-28-base-execution.md)

## Locked decisions

| Topic | Choice |
| --- | --- |
| Scope | Full mogu execution tree (L1–L5 + `parallel/` + `pipeline/` + `map_reduce/`) |
| Prerequisites | Proper modules: `synchronization` + `concurrency` (+ `memory` singleton / allocator subset as needed); **not** inlined into `execution/detail` |
| Mutex | **Do not** port `base::mutex`; use `std::mutex` / `std::shared_mutex` |
| Concurrent queue | Vendor moodycamel `concurrentqueue` under `third_party` |
| Linux-only backends | `io_uring` / epoll-shaped IO / unavailable GPU device: **explicit OS gate or stub**; portable `IOExecutor` rewrite (worker thread + task queue) |
| L5 umbrella | `execution.h` = L1..L4 only (same as mogu); parallel / pipeline / map_reduce stay explicit includes |
| Adoption | Both `parallel_for` (data parallel) and futures / Pipeline / portable IO (async) in first wave |
| GN | `//src/base/execution:execution` (+ sync/concurrency) → `//src/base:foundation` |
| Include | `#include "base/execution/..."` via `//src` |

## Layout

```text
src/base/
  synchronization/   # align, latch, barrier, future, spin_*, semaphore, waiter (no mutex.h)
  concurrency/       # queue (+ partition / peers required by execution)
  memory/            # + singleton.h; allocator/arena as needed by pool/pipeline
  execution/         # mogu directory shape
third_party/concurrentqueue/  # header-only facade → .src or include/
```

## Dependency invariants (from mogu)

| Area | Must not depend on |
| --- | --- |
| `executor/` | async, futures, interop, parallel, pipeline, map_reduce |
| `futures/` | async, pipeline, map_reduce, parallel |
| `async/` | futures, pipeline, map_reduce |
| `pipeline/` | executor, parallel, map_reduce |
| `map_reduce/` | pipeline, parallel capability layer (may use executor) |
| `parallel/` | pipeline, map_reduce, async, futures |

## Adoption targets (first wave)

| Area | How | Landed |
| --- | --- | --- |
| `plugin::ProcessingPool` | Prefer `ThreadPoolExecutor` / global pool + futures vs ad-hoc `std::thread` | `NThreadPoolExecutor` in `processing.cc` |
| CPU-heavy GIS loops (`tessellate_*`, field ingest, index builds) | `parallel_for` over `GlobalNThreadPoolExecutor` / local pool | `field_ingest`; `Layout` fill/line (≥2 jobs; cross-layer group) |
| Map2d visible layer → batch build | `Pipeline` (≥4 layers) or `parallel_for` (2–3); serial for 1 | `visible_layer_batches` |
| Map2d layout rebuild | `async` batches; `Layout::build` on caller (avoids nested pool) | `Map2dFrameCache::rebuild_layout` |
| Batch file / record pipelines | `Pipeline` where backpressure fits | `execution_test` Pipeline smoke; **`load_ogr_layer_pipeline`** |
| Dedicated IO callbacks | Portable `IOExecutor` (not io_uring on Windows) | open |

### Trace-driven next wave (2026-09-28) — landed

Source: `map_scene_test` + `SMT_TRACE=1` / `SMT_TRACE_DUMP` (nested `map2d.layout` spans).

| Change | Rationale |
| --- | --- |
| `Layout::build` on caller after `async` batches | Nested `parallel_for` on the same global pool from a pool worker wastes a worker / risks stall |
| Cross-layer `emit_fills` / `emit_lines` | Consecutive carto fill (land+water) and line (river/admin/road*) share one `parallel_for`; z-order preserved via `layer_ord` |
| Flatten MultiLineString → line jobs | One feature with many parts no longer stays serial inside `for_each_line` |
| `kParallelTessMinGeoms = 2` | Match batch parallel floor; small feature counts still parallelize |
| Do **not** expand MultiPolygon → per-leaf DrawItems | Measured regression (~1.6s → ~7s fill) from item explosion |

| Span (Debug, after) | Notes |
| --- | --- |
| `layout` ~1.57 s (was ~1.69 s) | ~7% wall drop on `map_scene_test` seed path |
| `emit_fill` ~0.58 s / `emit_line` ~0.87 s | Cross-layer + flatten + grain |
| `batches` ~75 ms | Leave alone |
| `gdi` ~2 s | Outside execution scope |

Follow-ups (not this change): parallel **merge** of MultiPolygon parts into one mesh; Release-build A/B; portable `IOExecutor` product call sites.

### Fine-span profile (2026-09-28, next cut)

Instrument: tess CPU buckets (atomic, flush once per Layout::build) + GDI ensure / rame_paint / gdi_fill|line|text.  
Source: map_scene_test + SMT_TRACE=1 (Debug). Tess *us = **CPU sum** across parallel_for (may exceed wall).

| Bucket | baseline | stroke/GDI/fill | line2 (stride+skip join) | Notes |
| --- | --- | --- | --- | --- |
| emit_line wall | ~0.94 s | ~0.19 s | **~0.11 s** | |
| 	ess_line_solid CPU | ~13.1 s | ~1.71 s | **~0.49 s** | ~27× vs baseline |
| 	ess_line_points CPU | ~0.36 s | ~0.38 s | **~0.02 s** | OGR stride |
| emit_fill wall | ~0.63 s | ~0.25 s | ~0.29 s | |
| 	ess_poly_fan CPU | ~6.3 s | ~0.41 s | ~0.39 s | |
| gdi.frame_paint | ~0.45 s | ~0.04 s | ~0.04 s | |
| gdi_line | ~0.24 s | ~0.03 s | **~0.014 s** | fewer quads |

**Decision (do / don't):**

1. **Landed — stroke**: wupp edge drop; thin → bevel/butt; **line2**: OGR stride + max 128; min_seg 1.5×wupp; px≤2.5 skip joins.
2. **Landed — GDI** prepare_for_present dual-speed.
3. **Landed — fill**: FillTessOptions + wupp; skip sub-px; stride; max 64.
4. **Don't — more layout-level execution**.
5. **Next if still needed:** remaining 	ess_line_solid (~0.5 s CPU) / warm-pan GDI; Release A/B.

### Feature load Pipeline (OGR / mogu table shape)

**Status:** landed (first consumer: Views map open via `ogr_ingest`); **Batch3a** ordered window in [`../plans/2026-09-28-gis-memory-load.md`](../plans/2026-09-28-gis-memory-load.md).

| Stage | Workers | Role |
| --- | --- | --- |
| produce | 1 | `OGRLayer::GetNextFeature` (same layer is not thread-safe); blocks when in-flight ≥ `ordered_window` |
| decode | N (≤8) | Caller `decode(OGRFeature*, Out*)` — e.g. `features_from_ogr` |
| sink | 1 + `CONSUMED` | Ordered flush of ready slots into caller `sink(Out&&)` when `ordered_window > 0` |

API: `gis/datasource/pipeline/feature_load_pipeline.h`.  
`FeatureLoadOptions::ordered_window` default **256** (0 = legacy buffer-all then sink-after-wait).  
`Pipeline::set_context_hooks` recycles `Ctx*` (freelist); `CONSUMED`/`COMPLETE`/`FAILED` release via hooks — handlers must not `delete &ctx`. Layers with `GetFeatureCount < 64` stay serial. Follow-ups: `SmtMap` / SDBD reuse.

## Out of scope

- Replacing RHI / GPU-process compose with CPU executors
- Porting mogu `base::mutex` / futex mutex
- Claiming io_uring / CUDA device lanes work on Windows without a real backend
- Parallel `GetNextFeature` on one `OGRLayer`

## Testing

- `execution_test` (pool execute, `parallel_for`, futures `when_all`/`then`, pipeline smoke)
- `map_scene_test` / open-path ingest (Pipeline decode path)
- Existing `build.bat` / `build.bat te` stay green

---

**最后更新：** 2026-09-28
