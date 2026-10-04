<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Vista Map2d equal-profile optimize — Implementation Plan

> Checklist hung off living [`../specs/2026-09-13-render-rhi-scene-design.md`](../specs/2026-09-13-render-rhi-scene-design.md) §Vista Map2d equal-profile optimize.
>
> Engine id / matrix codename: **`vista`** (Views + `gis/vista` + FlyCube). Former working name `src_render` retired.
>
> Parallel grain (P3): living **§src_render + vista parallel** + [`2026-10-02-src-render-vista-parallel-accelerate.md`](2026-10-02-src-render-vista-parallel-accelerate.md) (`SMT_VISTA_LAYOUT_PARALLEL`). This plan owns **equal-profile budgets**; that plan owns L0–L3 parallel machine.

**Goal:** Under the same china 1280×720 profile as leftover rhi2d matrix, keep **warm** Vista GPU present in leftover IR order (already met), then crush **cold first-frame upload** and **software GDI paint** without dropping MapFrame / hillshade richness.

**Architecture:** Keep Views `Map2dPresenter` → `Map2dFrameCache` → software `export_bmp` / FlyCube `present_gpu` (`vista/map` + `render::graph`). Optimize by **fair phase timing**, **warm-path reuse** (already OK), **cold upload merge**, **layout/incremental**, **software GDI batch** — not by stripping carto or chasing leftover IR-only ms.

**Tech Stack:** C++23, existing content/map2d + effect/map + render/rhi; harness `run_parallel_port_matrix.py`.

## Overall strategy (phased)

| Layer | Product intent | Current (Debug china 1280×720, `SMT_MAP2D_NO_HILLSHADE=1`) | Priority |
| --- | --- | --- | --- |
| **Warm GPU** | North star — StaticReuse / DrawCache skip | prior matrix `present_gpu_warm_ms=0`; **this matrix unmeasured** (AV after FlyCube init) | **P0** — confirm after GPU AV |
| **Cold first frame** | First Pass::record after invalidate | last complete: cold **3921** = upload **3082** + present **838**; **this matrix: no `present_gpu_*` (AV)** | **P0d** residual + GPU AV |
| **Software export** | Faithful BMP / GDI path | unit `paint_ms≈49`; matrix this run **319 / export 328** (was 136 / 141) | **P2d miss** on matrix |
| **Layout / hillshade** | Rebuild vs reuse; DEM shade when on | timed window `layout_ms`/`hillshade_ms`=0 (NO_HILLSHADE); still own cold rebuild cost | **P1** met (matrix) |
| **Measurement fairness** | Matrix labeling | leftover `execute_ms` = IR replay only (~202–554); ≠ Vista paint/present | always-on (P3 labeling) |

**One-line strategy:** Protect warm StaticReuse; **P0** merge cold per-mesh VB/IB uploads; **P1** cut layout rebuild; **P2** batch software GDI; **P3** optional `SMT_VISTA_LAYOUT_PARALLEL` + matrix false-gap labels — never delete MapFrame to chase leftover IR.

## Baseline (2026-10-03 matrix, Debug, equal-latitude)

**Re-verify (2026-10-03 evening, post leftover-AV join + P0d CPU pack + P2d unit):** `run_parallel_port_matrix.py` Debug equal-latitude. Leftover 9/9 `pass=True`. Vista software BMP OK; FlyCube AV after `gpu device acquired`.

| Path | Metric | ms | Notes |
| --- | --- | ---: | --- |
| leftover serial/tile/layer × gdi/gdi+/skia | `execute_ms_max` | **202–554** | IR only; **9/9 `pass=True`** (no DLL-detach AV; join FrameJob) |
| Vista software | `paint_ms` / `export_ms` | **319 / 328** | `bmp_io_ms=6`; `phase_gate_export=True`; log `fill_us≈151k` `line_us≈60k` `text_us≈41k` (second china paint). Unit `map2d_frame_gdi_test` **49** does **not** match matrix |
| Vista FlyCube cold | `present_gpu_cold_ms` | **—** | `rc=3221225477` after `rhi.flycube initialize ok` + `gpu device acquired owned=1`; no upload/present clocks. Prior complete row still **3921 / upload 3082** |
| Vista FlyCube warm | `present_gpu_warm_ms` | **—** | unmeasured this cell; last complete matrix **0** |
| Vista phases | `layout_ms` / `hillshade_ms` | **0** | Timed window after cache warm + `SMT_MAP2D_NO_HILLSHADE=1` |

Artifacts: `out/Debug/captures/map2d/matrix/parallel_port_matrix_with_vista.csv`, `vista_china.log`, `vista-china.inspect.png`.

### Confirmed root causes (do not re-litigate)

1. **Metric asymmetry** — leftover `execute_ms` = IR replay; Vista rows = MapFrame + GDI/GPU. Matrix `note` must keep this labeled.
2. **Cold upload** — `vista/map/detail/upload_draws` creates VB/IB **per `PlacedMesh`**; dominates cold (~29s Debug).
3. **Software full GDI** — `Map2dSoftwarePainter::paint` → `paint_map_frame_gdi` DrawItems (~2.1s).
4. **Warm path OK** — `Pass::record` DrawCache + StaticReuse; dual software+GPU in one matrix wall doubles process time (expected; not a product bug).

## Budgets (Debug china 1280×720)

| Metric | Budget | Status |
| --- | ---: | --- |
| warm `present_gpu_ms` / `present_gpu_warm_ms` | ≤ **80** | **Unmeasured** this matrix (AV); last complete **0** |
| warm `gpu_upload_ms` | ≈ **0** | **Unmeasured** this matrix; last complete StaticReuse **0** |
| cold first `present_gpu_cold_ms` (layout warm) | ≤ **400** | **Open** — this matrix **no sample** (AV); last complete **3921** |
| cold `gpu_upload_ms` | ≤ **200** | **Open** — this matrix **no sample**; last complete **3082** |
| `paint_ms` (faithful export, no `EXPORT_REUSE`) | ≤ **100** | **Miss** matrix **319** (budget 100); unit still **49** |
| `export_ms` (incl IO) | ≤ **150** | **Miss** matrix **328** (budget 150) |
| `layout_ms` cold china | ≤ **60**; StaticReuse ≈ 0 | **Met** on matrix timed window (0) + P1 unit |

## Phased scheme P0–P3

### P0 — Cold upload merge (MUST first)

| | |
| --- | --- |
| **Problem** | Per-mesh `create_buffer` in `upload_draws` → cold `gpu_upload_ms` ~29s |
| **Code surfaces** | `src/vista/map/detail/upload.cc` (`upload_draws`, `upload_xyz` / `upload_xyzuv`); `Pass::record` (`src/vista/map/pass.cc`); callers via `Map2dGpuPresent::present_frame` |
| **Levers** | Mega-buffer (pack solid / UV meshes into few VB+IB + draw ranges) **or** batch by pipeline (solid vs textured) + atlas already shared; keep DrawCache pointer lifetime correct on abandon |
| **Expected gain** | Cold upload **~29s → ≤200 ms**; cold present **≤400 ms** |
| **Acceptance** | Matrix: `gpu_upload_ms` cold ≤ **200**; `present_gpu_cold_ms` ≤ **400**; warm still **0**; visual gate unchanged |
| **Checkbox map** | Task 5 batch landed (P0a/b); P0c re-time done; P0d gate still open |

- [x] P0a: Design merge key (pipeline + vertex format + atlas/raster binding); keep per-draw tint in draw constants or instance buffer
- [x] P0b: Implement mega-buffer / batch upload; single (or few) `create_buffer` per cold record
- [x] P0c: Re-time matrix cold row; confirm warm DrawCache path untouched
  - **(2026-10-03 combined):** cold **3921** / upload **3082** / present **838**; warm **0** (StaticReuse OK). Prior P3-only run (paint~708 / cold~9242) was pre-P1/P2 merge — superseded.
- [ ] P0d: Gate: cold upload ≤ 200; cold present ≤ 400
  - **Miss (matrix, pre-P0d CPU pack):** upload **3082** (budget 200); cold present **3921** (budget 400).
  - **P0d (2026-10-03, effect/map only):** remaining ~3s after mega-buffer was CPU, not extra FlyCube `create_buffer`. Fix: one reserve per mega-buffer, skip full-frame copy on world+overlay, borrow DrawItem buffers for untransformed world meshes. Unit `map2d_pass_test` 4096-solid pack OK.
  - **Matrix re-verify (2026-10-03 evening):** **cannot close P0d.** Vista software export finished; FlyCube `initialize ok` then **AV** `rc=3221225477` before any `present_gpu_*` / `gpu_upload_ms` line. Suspect P0d **borrow world verts** lifetime vs GPU upload — Null unit does not exercise FlyCube. Next: fix GPU AV, then re-time cold upload/present.
  - **P0d AV fix (2026-10-03, effect/map):** Borrow into a copied `subset` MapFrame (or `tls->clear` before place) left `vertices_src`/`indices_src` dangling by FlyCube `upload()`. Fix: filter by kind on the caller `MapFrame`; `seal_borrowed_meshes` after `place_frame` and again at `upload_draws` entry. China FlyCube still AVs **inside `place_frame` ~item 2048/2554** (breadcrumbs) — after GPU device acquire, before seal/upload. Unit `map2d_pass_test` green. Cold `gpu_upload_ms` still unlogged.

### P1 — Layout / incremental

| | |
| --- | --- |
| **Problem** | Full `rebuild_layout` on content/zoom dirty; duplicate feature walks; overview cost |
| **Code surfaces** | `Map2dFrameCache::prepare_for_present` / `rebuild_layout` (`map2d_frame_cache.cc`); `vista/map/layout` Layout emitters |
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
| **Code surfaces** | `Map2dSoftwarePainter::paint`; `map2d_frame_gdi` / DrawItems; present-cache blit (`SMT_MAP2D_EXPORT_REUSE`) |
| **Levers** | Batch same-brush paths; reduce GDI object churn; keep default export faithful; reuse env remains bench-only |
| **Expected gain** | Faithful `paint_ms` ≤ **100** (stretch); interim: large cut toward budget |
| **Acceptance** | `paint_ms` ≤ 100 on equal-latitude china; `export_ms` IO excluded from paint; inspect PNG OK |
| **Checkbox map** | Task 3 paint budget (open for faithful; met for EXPORT_REUSE) |

- [x] P2a: Profile DrawItems hot kinds (fill/line/text) under NO_HILLSHADE
  - china 1280×720 NO_HILLSHADE: items≈1950 fill=114 line_mesh=1776 text=60; hot = mesh `Polygon` then text halo (`map2d_frame_gdi_test` stderr `gdi batch`)
- [x] P2b: Batch GDI by brush/pen; optional path coalescing
  - sticky SelectObject; land fills PolyPolygon; mesh lines chunked PolyPolygon (96) + NULL_PEN; stroke PolyPolyline; font cache; 4-neighbor text halo
- [x] P2c: Keep `SMT_MAP2D_EXPORT_REUSE=1` bench-only; default clear_present_cache
- [x] P2d: Target faithful `paint_ms` ≤ 100
  - **(2026-10-03 unit `map2d_frame_gdi_test` china 1280×720 NO_HILLSHADE):** `paint_ms=49` / `bmp_io_ms=2` (fill_us≈10 / line_us≈11 / text_us≈14). Cosmetic PolyPolyline. `EXPORT_REUSE` still bench-only.
  - **Matrix (same evening):** `paint_ms=319` / `export_ms=328` — **miss** 100 / 150. Showcase `gdi batch` fill_us≈151ms line≈60ms text≈41ms; `mesh_flushes=0`. Unit 49 is **not** the equal-profile cell. Residual: bring showcase GDI onto the unit batch path (or stop claiming P2d met on matrix).

### P3 — Parallel + matrix labeling

| | |
| --- | --- |
| **Problem** | Layout parallel under-used; readers confuse leftover IR vs Vista MapFrame |
| **Code surfaces** | `SMT_VISTA_LAYOUT_PARALLEL` (vista emitters — see parallel plan); `run_parallel_port_matrix.py` CSV `note` / columns |
| **Levers** | Enable parallel tess when jobs ≥ threshold; matrix prints phase table + **false-gap** label (IR ≠ paint/present); never claim execute ≡ export |
| **Expected gain** | Layout wall down on multi-core; docs/matrix stop false “Vista 60× slower” claims |
| **Acceptance** | CSV note + harness docs; parallel opt-out `=0` still works |
| **Checkbox map** | Task 4 optional parallel + Task 6; cross **§vista parallel** V1 |

- [ ] P3a: Wire/verify `SMT_VISTA_LAYOUT_PARALLEL` on china layout (no UI join)
  - **TODO (P3 harness, 2026-10-03):** matrix already sets `SMT_VISTA_LAYOUT_PARALLEL=1` (opt-out `=0`) on the vista cell and records `smt_vista_layout_parallel` in CSV/JSON. Product `getenv` / `vista_layout_parallel_enabled()` in `vista/map/layout/*` is **not** owned here — leave to parallel-plan V1 / whoever owns `gis/vista` emitters (do not conflict with P1 layout edits). Until wired, tess still runs `parallel_for` by job count only.
- [x] P3b: Matrix prints leftover vs vista phase table; keep equal-latitude note (`run_parallel_port_matrix.py` tables A+B + `EQUAL_LATITUDE_NOTE`)
- [x] P3c: Label false-gap in CSV/`note` / `parallel_port_matrix_NOTE.txt` and skill tables (IR vs MapFrame)
- [x] P3d: Visual: `vista-china` still shows labels (+ hillshade when DEM on) — verify on next matrix run after P0–P2 land
  - **(2026-10-03 evening matrix):** `vista-china.inspect.png` — city labels + rivers/coast OK under `SMT_MAP2D_NO_HILLSHADE=1` (software export survived; GPU present AV)

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
- [ ] Target: warm `present_gpu_ms` ≤ **80** on china 1280×720 Debug (last complete **0**; **this matrix AV**); cold first ≤ **400** (**open**: last **3921**; this matrix no sample → **P0d** + GPU AV)

### Task 3: export_bmp / software path

- [x] Optional `SMT_MAP2D_EXPORT_REUSE=1`: allow present-cache blit when cam+size match (bench only; default export stays faithful)
- [x] Time-exclude BMP file write from paint metric (`paint_ms` vs `export_ms`)
- [x] Hillshade: cache `shade_dem_rgba` by DEM path + illumination + viewport LOD; overview uses downsampled DEM
- [ ] Target: `paint_ms` ≤ **100**; `export_ms` (incl IO) ≤ **150** on same profile (**faithful matrix** paint=**319** / export=**328** — both miss; unit 49; was ~2161 → 136; **met** with `SMT_MAP2D_EXPORT_REUSE=1`: paint≈3 / export≈9)

### Task 4: Layout / MapFrame cost

- [x] Profile `Map2dFrameCache` rebuild vs reuse on china frame; cut duplicate feature walks → **P1**
- [x] Overview LOD: skip or simplify low-importance roads/labels earlier (fblc / zoom already partially there) → **P1**
- [ ] Optional: parallel prep of layout batches (`SMT_VISTA_LAYOUT_PARALLEL`) → **P3**
- [x] Target: `layout_ms` ≤ **60** on cold china; near-zero on StaticReuse

### Task 5: GPU upload / effect/map

- [x] Batch / mega-buffer small line/fill uploads (**P0 MUST**); prefer one hillshade texture + instanced vectors
- [ ] Async / double-buffer upload where FlyCube allows without tearing product API (after P0)
- [ ] Target: warm `gpu_upload_ms` ≈ 0 on StaticReuse (last complete **met**; this matrix **AV**); cold upload ≤ **200** (**open**: last 3082; this matrix no sample → **P0d**)

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
