<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Vista Map2d equal-profile optimize — Implementation Plan

> Checklist hung off living [`../specs/2026-09-13-render-rhi-scene-design.md`](../specs/2026-09-13-render-rhi-scene-design.md) §Vista Map2d equal-profile optimize.
>
> Engine id / matrix codename: **`vista`** (Views + `gis/vista` + Vista). Former working name `src_render` retired.
>
> Parallel grain (P3): living **§src_render + vista parallel** + [`2026-10-02-src-render-vista-parallel-accelerate.md`](2026-10-02-src-render-vista-parallel-accelerate.md) (`VISTA_LAYOUT_PARALLEL`). This plan owns **equal-profile budgets**; that plan owns L0–L3 parallel machine.

**Goal:** Under the same china 1280×720 profile as leftover rhi2d matrix, keep **warm** Vista GPU present in leftover IR order (already met), then crush **cold first-frame upload** and **software GDI paint** without dropping MapFrame / hillshade richness.

**Architecture:** Keep Views `Map2dPresenter` → `Map2dFrameCache` → software `export_bmp` / Vista `present_gpu` (`vista/component/map` + `render::graph`). Optimize by **fair phase timing**, **warm-path reuse** (already OK), **cold upload merge**, **layout/incremental**, **software GDI batch** — not by stripping carto or chasing leftover IR-only ms.

**Tech Stack:** C++23, existing content/map2d + effect/map + render/rhi; harness `run_parallel_port_matrix.py`.

## Overall strategy (phased)

| Layer | Product intent | Current (Debug china 1280×720, `MAP2D_NO_HILLSHADE=1`) | Priority |
| --- | --- | --- | --- |
| **Warm GPU** | North star — StaticReuse / DrawCache skip | matrix `present_gpu_warm_ms=0` | **met** |
| **Cold first frame** | First Pass::record after invalidate | cold **584** = upload **526** + present **58** | **P0d** residual |
| **Software export** | Faithful BMP / GDI path | matrix **paint 76 / export 83** (DIB scanline land fill) | **P2d met** |
| **Layout / hillshade** | Rebuild vs reuse; DEM shade when on | timed window `layout_ms`/`hillshade_ms`=0 (NO_HILLSHADE) | **P1** met |
| **Measurement fairness** | Matrix labeling | Scenic rhi2d `execute_ms` = IR only; ≠ Vista paint/present | always-on (P3 labeling) |

**One-line strategy:** Protect warm StaticReuse; **P0** merge cold per-mesh VB/IB uploads; **P1** cut layout rebuild; **P2** batch software GDI; **P3** optional `VISTA_LAYOUT_PARALLEL` + matrix false-gap labels — never delete MapFrame to chase leftover IR.

## Baseline (2026-10-03 matrix, Debug, equal-latitude)

**Re-verify (2026-10-03 evening, post leftover-AV join + P0d CPU pack + P2d unit):** `run_parallel_port_matrix.py` Debug equal-latitude. Leftover 9/9 `pass=True`. Vista software BMP OK; Vista AV after `gpu device acquired`.

| Path | Metric | ms | Notes |
| --- | --- | ---: | --- |
| leftover serial/tile/layer × gdi/gdi+/skia | `execute_ms_max` | **202–554** | IR only; **9/9 `pass=True`** (no DLL-detach AV; join FrameJob) |
| Vista software | `paint_ms` / `export_ms` | **319 / 328** | `bmp_io_ms=6`; `phase_gate_export=True`; log `fill_us≈151k` `line_us≈60k` `text_us≈41k` (second china paint). Unit `map2d_frame_gdi_test` **49** does **not** match matrix |
| Vista cold | `present_gpu_cold_ms` | **—** | `rc=3221225477` after `rhi.flycube initialize ok` + `gpu device acquired owned=1`; no upload/present clocks. Prior complete row still **3921 / upload 3082** |
| Vista warm | `present_gpu_warm_ms` | **—** | unmeasured this cell; last complete matrix **0** |
| Vista phases | `layout_ms` / `hillshade_ms` | **0** | Timed window after cache warm + `MAP2D_NO_HILLSHADE=1` |

Artifacts: `out/Debug/captures/browser/matrix/parallel_port_matrix_with_vista.csv`, `vista_china.log`, `vista-china.inspect.png`.

### Confirmed root causes (do not re-litigate)

1. **Metric asymmetry** — leftover `execute_ms` = IR replay; Vista rows = MapFrame + GDI/GPU. Matrix `note` must keep this labeled.
2. **Cold upload** — `vista/component/map/detail/upload_draws` creates VB/IB **per `PlacedMesh`**; dominates cold (~29s Debug).
3. **Software full GDI** — `Map2dSoftwarePainter::paint` → `paint_map_frame_gdi` DrawItems (~2.1s).
4. **Warm path OK** — `Pass::record` DrawCache + StaticReuse; dual software+GPU in one matrix wall doubles process time (expected; not a product bug).

## Budgets (Debug china 1280×720)

| Metric | Budget | Status |
| --- | ---: | --- |
| warm `present_gpu_ms` / `present_gpu_warm_ms` | ≤ **80** | **Met** **0** |
| warm `gpu_upload_ms` | ≈ **0** | **Met** StaticReuse **0** |
| cold first `present_gpu_cold_ms` (layout warm) | ≤ **400** | **Miss** **584** |
| cold `gpu_upload_ms` | ≤ **200** | **Miss** **526** |
| `paint_ms` (faithful export, no `EXPORT_REUSE`) | ≤ **100** | **Met** **76** |
| `export_ms` (incl IO) | ≤ **150** | **Met** **83** |
| `layout_ms` cold china | ≤ **60**; StaticReuse ≈ 0 | **Met** timed window **0** |

## Phased scheme P0–P3

### P0 — Cold upload merge (MUST first)

| | |
| --- | --- |
| **Problem** | Per-mesh `create_buffer` in `upload_draws` → cold `gpu_upload_ms` ~29s |
| **Code surfaces** | `src/vista/component/map/detail/upload.cc` (`upload_draws`, `upload_xyz` / `upload_xyzuv`); `Pass::record` (`src/vista/component/map/pass.cc`); callers via `Map2dGpuPresent::present_frame` |
| **Levers** | Mega-buffer (pack solid / UV meshes into few VB+IB + draw ranges) **or** batch by pipeline (solid vs textured) + atlas already shared; keep DrawCache pointer lifetime correct on abandon |
| **Expected gain** | Cold upload **~29s → ≤200 ms**; cold present **≤400 ms** |
| **Acceptance** | Matrix: `gpu_upload_ms` cold ≤ **200**; `present_gpu_cold_ms` ≤ **400**; warm still **0**; visual gate unchanged |
| **Checkbox map** | Task 5 batch landed (P0a/b); P0c re-time done; P0d gate still open |

- [x] P0a: Design merge key (pipeline + vertex format + atlas/raster binding); keep per-draw tint in draw constants or instance buffer
- [x] P0b: Implement mega-buffer / batch upload; single (or few) `create_buffer` per cold record
- [x] P0c: Re-time matrix cold row; confirm warm DrawCache path untouched
  - **(2026-10-03 combined):** cold **3921** / upload **3082** / present **838**; warm **0** (StaticReuse OK). Prior P3-only run (paint~708 / cold~9242) was pre-P1/P2 merge — superseded.
- [ ] P0d: Gate: cold upload ≤ 200; cold present ≤ 400
  - **(2026-10-05 matrix, post DIB paint + upload pack):** upload **526** (budget 200); cold present **584** (budget 400); warm **0**. Residual is still mega-pack + Vista `create_buffer` / texture, not per-mesh VB.

### P1 — Layout / incremental

| | |
| --- | --- |
| **Problem** | Full `rebuild_layout` on content/zoom dirty; duplicate feature walks; overview cost |
| **Code surfaces** | `Map2dFrameCache::prepare_for_present` / `rebuild_layout` (`map2d_frame_cache.cc`); `vista/component/map/layout` Layout emitters |
| **Levers** | Stronger fingerprint; skip redundant walks; overview LOD (fblc / zoom); settle debounce already present |
| **Expected gain** | Cold `layout_ms` ≤ **60**; StaticReuse stays ~0 |
| **Acceptance** | Phase clocks + no visual regression on settle/pan |
| **Checkbox map** | Task 4 (**P1 done**; parallel remains P3) |

- [x] P1a: Profile rebuild vs `kStaticReuse` / `kInteractiveReuse` on china
- [x] P1b: Cut duplicate feature walks; tighten ContentFingerprint when safe
- [x] P1c: Overview LOD — skip/simplify low-importance roads/labels earlier
- [x] P1d: Target `layout_ms` ≤ 60 cold; near-zero on StaticReuse

### P2 — Software GDI / batch

| | |
| --- | --- |
| **Problem** | Faithful export ~2.1s GDI DrawItems (fill ~114); product still needs export path |
| **Code surfaces** | `Map2dSoftwarePainter::paint`; `map2d_frame_gdi` / DrawItems; present-cache blit (`MAP2D_EXPORT_REUSE`) |
| **Levers** | Batch same-brush paths; reduce GDI object churn; keep default export faithful; reuse env remains bench-only |
| **Expected gain** | Faithful `paint_ms` ≤ **100** (stretch); interim: large cut toward budget |
| **Acceptance** | `paint_ms` ≤ 100 on equal-latitude china; `export_ms` IO excluded from paint; inspect PNG OK |
| **Checkbox map** | Task 3 paint budget (open for faithful; met for EXPORT_REUSE) |

- [x] P2a: Profile DrawItems hot kinds (fill/line/text) under NO_HILLSHADE
  - china 1280×720 NO_HILLSHADE: items≈1950 fill=114 line_mesh=1776 text=60; hot = mesh `Polygon` then text halo (`map2d_frame_gdi_test` stderr `gdi batch`)
- [x] P2b: Batch GDI by brush/pen; optional path coalescing
  - sticky SelectObject; land fills PolyPolygon; mesh lines chunked PolyPolygon (96) + NULL_PEN; stroke PolyPolyline; font cache; 4-neighbor text halo
- [x] P2c: Keep `MAP2D_EXPORT_REUSE=1` bench-only; default clear_present_cache
- [x] P2d: Target faithful `paint_ms` ≤ 100
  - **(2026-10-05 matrix):** `paint_ms=76` / `export_ms=83`; log `fill_us≈24ms` `dib_tris≈2470` `dib=1`. Land fill is 32bpp DIB scanline (not PolyPolygon / FillPath). Inspect: cream land, labels, no triangle wireframe.
  - Unit `map2d_frame_gdi_test` china `paint_ms=56`.

### P3 — Parallel + matrix labeling

| | |
| --- | --- |
| **Problem** | Layout parallel under-used; readers confuse leftover IR vs Vista MapFrame |
| **Code surfaces** | `VISTA_LAYOUT_PARALLEL` (vista emitters — see parallel plan); `run_parallel_port_matrix.py` CSV `note` / columns |
| **Levers** | Enable parallel tess when jobs ≥ threshold; matrix prints phase table + **false-gap** label (IR ≠ paint/present); never claim execute ≡ export |
| **Expected gain** | Layout wall down on multi-core; docs/matrix stop false “Vista 60× slower” claims |
| **Acceptance** | CSV note + harness docs; parallel opt-out `=0` still works |
| **Checkbox map** | Task 4 optional parallel + Task 6; cross **§vista parallel** V1 |

- [x] P3a: Wire/verify `VISTA_LAYOUT_PARALLEL` on china layout (no UI join)
  - **(2026-10-05):** `vista_layout_parallel_enabled()` in `tess_grain.h`; `emit_fills` / `emit_lines` gate `parallel_for`. Harness sets `=1`.
- [x] P3b: Matrix prints leftover vs vista phase table; keep equal-latitude note (`run_parallel_port_matrix.py` tables A+B + `EQUAL_LATITUDE_NOTE`)
- [x] P3c: Label false-gap in CSV/`note` / `parallel_port_matrix_NOTE.txt` and skill tables (IR vs MapFrame)
- [x] P3d: Visual: `vista-china` still shows labels (+ hillshade when DEM on) — verify on next matrix run after P0–P2 land
  - **(2026-10-05 matrix):** `vista-china.inspect.png` — cream land, city labels, rivers/coast; no triangle wireframe under `MAP2D_NO_HILLSHADE=1`.

## Tasks (legacy checklist — keep status)

### Task 1: Equal-profile phase clocks (fair compare)

- [x] Split logs: `layout_ms` / `hillshade_ms` / `software_paint_ms` / `bmp_io_ms` / `gpu_upload_ms` / `gpu_present_ms`
- [x] Matrix CSV columns for those phases; document leftover `execute_ms` ≠ export_ms
- [x] Report **warm** present (2nd+ frame, StaticReuse) separately from cold first frame
- [x] Gate: matrix row still green; phases sum ≈ export / present within ±15% (Vista `phase_gate_export` / `phase_gate_cold`)

### Task 2: Cold present_gpu crash diet

- [x] Showcase / matrix: reuse one `rhi::Device` across present samples (do not create/shutdown per matrix cell if already warm)
- [x] Avoid `invalidate_frame_cache()` immediately before timed present when size/extent unchanged
- [x] Keep Pass uploads on StaticReuse / InteractiveReuse (already partially done)
- [x] Target: warm `present_gpu_ms` ≤ **80** on china 1280×720 Debug (**0**); cold first ≤ **400** (**miss 584** → **P0d**)

### Task 3: export_bmp / software path

- [x] Optional `MAP2D_EXPORT_REUSE=1`: allow present-cache blit when cam+size match (bench only; default export stays faithful)
- [x] Time-exclude BMP file write from paint metric (`paint_ms` vs `export_ms`)
- [x] Hillshade: cache `shade_dem_rgba` by DEM path + illumination + viewport LOD; overview uses downsampled DEM
- [x] Target: `paint_ms` ≤ **100**; `export_ms` (incl IO) ≤ **150** on same profile (**2026-10-05 faithful matrix** paint=**76** / export=**83**)

### Task 4: Layout / MapFrame cost

- [x] Profile `Map2dFrameCache` rebuild vs reuse on china frame; cut duplicate feature walks → **P1**
- [x] Overview LOD: skip or simplify low-importance roads/labels earlier (fblc / zoom already partially there) → **P1**
- [x] Optional: parallel prep of layout batches (`VISTA_LAYOUT_PARALLEL`) → **P3a wired**
- [x] Target: `layout_ms` ≤ **60** on cold china; near-zero on StaticReuse

### Task 5: GPU upload / effect/map

- [x] Batch / mega-buffer small line/fill uploads (**P0 MUST**); prefer one hillshade texture + instanced vectors
- [ ] Async / double-buffer upload where Vista allows without tearing product API (after P0)
- [x] Target: warm `gpu_upload_ms` ≈ 0 on StaticReuse (**met**); cold upload ≤ **200** (**miss 526** → **P0d**)

### Task 6: Matrix acceptance

- [x] `run_parallel_port_matrix.py` prints leftover vs vista phase table → **P3** (tables A+B + FALSE-GAP banner)
- [x] Visual: `vista-china` still shows hillshade + labels (score / inspect; hillshade when DEM on)
  - labels OK on equal-latitude cell; hillshade deferred to DEM-on run
- [ ] Doc as-built one-liner in `docs/superpowers/ui-testing.md` when landed
- [x] Keep CSV `note` false-gap: leftover IR vs Vista paint/present (`matrix_note` / `NOTE.txt` / skill)

## Non-goals

- Matching leftover “IR-only” ms by deleting hillshade / MapFrame
- Porting MapLibre Native shaders
- Making leftover the product default again
- Treating dual software+GPU matrix **wall_ms** as a product regression (label it; optimize phases)
- New dated design twin — revise this plan + living § only
- New parallel architecture diagram — reuse [`../diagrams/render-accelerate-topology.html`](../diagrams/render-accelerate-topology.html) (§vista parallel)

## HTML diagram

**Skip new HTML:** P0–P3 do not lock a new architecture or parallel model; they phase existing surfaces under StaticReuse + `upload_draws` + L0–L3. Normative parallel visual remains `render-accelerate-topology.html`.
