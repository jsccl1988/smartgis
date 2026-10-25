<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# src_render + vista parallel accelerate (终�? �?Implementation Plan

> Hung off living [`../specs/2026-09-13-render-rhi-scene-design.md`](../specs/2026-09-13-render-rhi-scene-design.md) **§src_render + vista parallel accelerate**.  
> **Directory tighten (landed, not this plan):** living **§Vista subdirectory tighten**.  
> **Upgrade U0–U4 (landed):** living **§Vista logical/physical lanes**. Follow-on **M1–M4:** living **§Vista map/frame scenic-peer** · [`../diagrams/vista-map-frame-scenic-peer.html`](../diagrams/vista-map-frame-scenic-peer.html). �?Scenic 脝区 / 帧拝 / prep 语义；丝�?HDC〝D3D11 deferred〝焊死的 `MapPainter`�? 
> **IR/GPU 目录（权娝）�?* living **§Vista IR/Pass lanes**。逻辑/物睆话汇退役。目标磝盘是 `vista/component/map`（`MapIR`�? `vista/pass/map`（`MapPass`），`vista/component/world`（`World` + `Instance`�? `vista/pass/world`（`WorldPass`）。下文文件表和任务勾选仝用勾选当时的 `vista/frame` / `vista/scene` 坝字，丝改写�? 
> **Diagram (normative visual):** [`../diagrams/render-accelerate-topology.html`](../diagrams/render-accelerate-topology.html) �?**A×B 深度整坈**；Topology A �?§2–�?（`stage_frame` �?`build_layout_parallel` �?`prep_cull_parallel` �?`record_and_present`）�? 
> **CPU boundary:** [`gis-vista-architecture.html`](../diagrams/gis-vista-architecture.html).  
> **Related:** [`legacy-render-architecture.html`](../diagrams/legacy-render-architecture.html) · [`ui-views-shell-architecture.html`](../diagrams/ui-views-shell-architecture.html)  
> **Present C/G/E:** living **§content present accelerate** · [`../diagrams/content-present-accelerate.html`](../diagrams/content-present-accelerate.html) · this plan **Task 7**.  
> **Present dual-track ceiling:** living **§Present dual-track ceiling** · [`../diagrams/content-present-dual-track.html`](../diagrams/content-present-dual-track.html) · this plan **Task 9** (T1 GPU damage ∥ T2 Scene3d FrameJob).  
> **GPU-process checklist (do not duplicate here):** [`2026-09-27-gpu-rhi-accelerate.md`](2026-09-27-gpu-rhi-accelerate.md) **Task 8: Bridge to in-process L0–L3**.

**Goal:** Product-track parallel end state for `src/vista` (CPU MapFrame / World + GPU map / GpuScene / atmosphere in **vista.dll**) + `src/render` / `content` (present), reusing leftover rhi2d/rhi3d **semantics** �?not GDI HDC or D3D11 deferred. Living § now locks **code-level** Types / Call graph / Grains / GPU / Env / API sketches.

**Architecture (L0–L3):** UI never joins �?content FrameJob schedule �?vista / GpuScene `parallel_for` �?Display sole Device + **one** CommandList. Gen / cancel / StaticReuse gate publish.

**Tech Stack:** C++23, `base::execution`, `gis::vista::Layout` / `MapFrame`, `render::{rhi,graph}`, `vista::{GpuScene,map pass,atmosphere pass}`, content presenters, Views Display mailbox.

## Design summary (locked �?see living § §§1�?)

| Layer | Owner | One-liner |
| --- | --- | --- |
| L0 UI | Views HWND / Commit | `stage_frame` only · �?join |
| L1 Schedule | `content` Map2d/Scene3d | gen / cancel / clocks / wake |
| L2 CPU | `gis::vista` (+ prep) | `build_layout_parallel` / `prep_cull_parallel` |
| L3 GPU | Display · `graph` / `vista` GPU passes | `record_and_present` �?`graph::present` · 1 CL |

Env (product): `VISTA_LAYOUT_PARALLEL`; `GPUSCENE_PREP_PARALLEL` **default off** until frustum cull honesty (`=0` �?N=1; opt-in `=1` after `SCENE3D_FRUSTUM_CULL`) �?see Scene3d equal-profile **M3** [`2026-10-01-src-render-scene3d-equal-profile-optimize.md`](2026-10-01-src-render-scene3d-equal-profile-optimize.md). Leftover `RHI2D_*` / `RHI3D_*` must not drive product path.

## Global Constraints

- Work on **`master`** only.
- `gis` �?`render/rhi` / HWND; `render` �?`legacy_render`.
- UI / HWND **never** `join`s layout or GPU workers.
- Sole pool: `base::execution`. No second product pool.
- Comments English; helpers `snake_case`; two-layer namespaces.
- Design HTML stays **light technical** (`.cursor/rules/repo/design-html-diagrams.mdc`).
- **Do not** `git commit` unless the user asks.

## File map (as-built anchors �?hooks)

| Path | Role | API / symbol |
| --- | --- | --- |
| `vista/component/map/frame.h` | Types | `LayoutInput` · `LayerBatch` · `MapFrame` · `Layout` |
| `vista/component/map/layout.cc` | CPU orchestrator | `Layout::build` |
| `vista/component/map/layout/paint/fill.cc` � `line.cc` | Tess grain | `emit_fills` / `emit_lines` + `parallel_for` |
| `vista/component/map/layout/paint/{point,symbol}.cc` | Unify V1 | `emit_circles` / `emit_heatmap` / `emit_symbols` |
| `vista/component/map/layout/policy/tess_grain.h` | Thresholds | `kParallelTessMinGeoms` · `kParallelTessGrain` |
| `vista/component/world/terrain/mesh/*` | TLS scratch | tess helpers |
| `content/.../map2d/frame/map2d_frame_cache.*` | L1 cache | `prepare_for_present` · `rebuild_layout` �?V2 `stage_frame` |
| `content/.../map2d/frame/map2d_batches.cc` | Layer jobs | `batches_via_parallel_for` · `merge_parts` |
| `content/.../map2d/gpu/map2d_gpu_present.cc` | Warm/cold GPU | `present` · `present_frame` �?`graph::present` |
| `content/.../map2d/map2d_phase_profile.*` | Clocks | `layout_ms` · `gpu_upload_ms` · `present_ms` |
| `content/.../scene3d/gpu/scene3d_gpu_present.cc` | 3D present | `Scene3dGpuPresent::present` |
| `vista/frame/pass.*` | Upload + record | `FramePass::record` · `invalidate_uploaded` |
| `vista/scene/scene_draw.cc` · `detail/draw_pass.cc` | 3D draw | `GpuScene::record_draws` · `record_kind` · frustum |
| `vista/pass/world/atmosphere/ocean/gpu_fields.cc` | Compute FFT | Device-thread only |
| `render/graph/frame_graph.*` | 1 CL contract | `render::graph::present` |
| `base/execution/parallel/for.h` | Pool grain | `parallel_for` · latch |
| `docs/superpowers/diagrams/render-accelerate-topology.html` | Normative SVG（A×B�?| Named stages + GPU process |
| `src/render/README.md` | Env pointer | Parallel / GPU (planned) |

## Tasks

### Task 1: Contract doc + env surface �?V0

- [x] Living § locked (L0–L3 + **代砝�?* §§1�?)
- [x] Light HTML diagram (layering + named pipeline stages)
- [x] Document env pointer in `src/render/README.md`（接线代砝仝�?V1�?- [x] Explicit non-goal: no GDI tile HDC / no D3D11 deferred on product path

### Task 2: Vista layout grain (Map2d CPU) �?V1

- [x] Fill / line tess `parallel_for` + per-worker scratch (as-built)
- [x] Wire `VISTA_LAYOUT_PARALLEL` helper（`vista_layout_parallel_enabled()`）in `tess_grain.h` / emitters；`=0` �?N=1
- [x] Unify circle / heatmap / extrusion under job→merge (preserve painter z). Symbol/label stay serial (`LabelGrid`)
- [x] Optional viewport **tile layout** grain (device-pixel AABB) for full-damage china �?peer of leftover `tile`; GPU compose = `vista/component/map` (not TransparentBlt). N1: `enumerate_layout_tiles` + layer×tile `cache_key` splice.
- [x] Collision / label resolve **serial** after parallel emit (document + assert in `emit_symbols` / `Layout::build`)
- [x] Default on when `jobs >= kParallelTessMinGeoms`

### Task 3: Content FrameJob semantics �?V2

- [x] Map2d: `stage_frame` posts hillshade on executor; `snapshot_present` mailbox. Full tess still joins on the Display tick that needs a new MapFrame (first / settle)
- [x] Dual slot: rebuild does not drop `published_` until gen-matched publish
- [x] Scene3d: GPU record/present on Display；CPU mesh/cull may fan out before record (`prep_cull_meshes`)
- [x] Stale `layout_gen` never publishes；destroy cancels without HWND join (`mark_published`)
- [x] Phase clocks: `layout_ms` / `prep_ms` / `upload_ms` / `record_ms` / `present_ms`（align equal-profile §§�?
### Task 4: GpuScene / 3D prep �?V3

- [x] Extract `prep_cull_parallel`（`vista/scene` + `frustum_aabb`）；workers write `visible[]` only
- [x] Gate behind `SCENE3D_FRUSTUM_CULL=1`（as-built default off�? `GPUSCENE_PREP_PARALLEL`
- [x] Product **default off** for `GPUSCENE_PREP_PARALLEL` until cull honesty（world3d matrix: `prep_par_on` slower �?equal-profile **M3**�?- [x] When enabled: `=0` �?serial；`=1` �?clamp 2�?
- [x] Workers must **not** touch `rhi::Device` / CommandList
- [x] Serial `GpuScene::record_draws` / `graph::present` on GPU thread only
- [ ] Acceptance cross-link: [`2026-10-01-src-render-scene3d-equal-profile-optimize.md`](2026-10-01-src-render-scene3d-equal-profile-optimize.md) M3

### Task 5: GPU record (P3 peer) �?V5 deferred

- [ ] Default: **single** CommandList �?no multi-thread record until cold `record_ms` hot after V3
- [ ] Optional P3b: Vista multi-CL bundles (DX12)；never import leftover D3D11 deferred TLS
- [ ] Ocean FFT compute stays Device-thread（`OceanGpuFields::record`�?
### Task 6: Acceptance harness

- [ ] Map2d china 1280×720: warm StaticReuse per equal-profile；layout parallel cut vs `VISTA_LAYOUT_PARALLEL=0`
- [ ] Scene3d `--atmosphere-showcase=legacy`: warm leftover-order；prep scales with N when cull on
- [ ] Visual gates unchanged (hillshade / labels / legacy landish)
- [ ] Keep HTML diagram in sync when phases land (revise in place)

### Task 7: content present accelerate (C/G/E) �?living § 2026-10-04

Hung off **§content present accelerate**. Do not open a twin plan.

> **Checkbox truth (2026-10-13):** [x] rows below are historical hints only. C0 / G0 / E1 (and peers) **land status follows code + living §content present accelerate** (and §Content present ↔ Chromium mapping). Do not treat a checked box as proof of ship; especially C1 is **dead / do not revive** in the living § even if listed [x] here.

- [x] C0/E0: shrink `Map2dFrameCache` mutex off `graph::present`; dual published/building slots; hillshade prefetch on `base::execution` (drop `std::thread` detach)
- [x] G0: `kInteractiveReuse` never `invalidate_uploaded`; pan uses `Pass` DrawCache encode
- [x] C1: per-layer `DrawItem` cache (`cache_key` / `layer_slices_`); settle reuses world items instead of drop-whole-frame tess
- [x] E1: `stage_frame` mailbox posts layout on `base::execution`; Display presents previous published (no settle join). One-time cv wait only when there is no published MapFrame (not HWND paint; WaitFirstMapPresent / tests). No same-tick `async`+`get`.
- [x] G1: `Pass::UploadPolicy::kIncremental` reuses DrawCache when world+overlay identity hashes match; mismatch is one Display-thread place+upload (no split overlay Device objects �?prior split path AVed). Optional CPU pack `parallel_for` not wired
- [x] G2: raster/hillshade already textured quads via `load_raster` / `emit_hillshade`. Topology B CF emit **skipped** (would be multiprocess `--type=gpu`; do not dual-compose A+B in-process)
- [x] E2: circle / heatmap / extrusion job→`parallel_for`→ordered merge; `vista_layout_parallel_enabled()` (`VISTA_LAYOUT_PARALLEL=0` serial). Scene3d `prep_cull_meshes` already gated on frustum honesty + `GPUSCENE_PREP_PARALLEL` default OFF
- [x] Keep [`../diagrams/content-present-accelerate.html`](../diagrams/content-present-accelerate.html) in sync
- [x] Map2d present layers (2026-10-04): split `frame/` mailbox + `layout_build` + `map2d_carto`; `std::mutex`; hillshade `shared_ptr`; delete dead C1 / `mutex()` / `frame()`; Scenic sources under `map2d/scenic/` (Presenter product path does not include `scenic/engine.h`)

### Task 8: Vista logical/physical upgrade �?U0–U4

Hung off **§Vista logical/physical lanes**. Move order and acceptance live in that §. Each step stays compilable. No forwarding headers.

Borrow from Scenic: frame-beat discard (already `stage_frame`), prep-before-draw (already `prep_cull`), dirty-slice emit. Leave on Scenic: private HDC, D3D11 deferred TLS, fused `MapPainter` / `Scene::Render`, in-engine present.

- [x] U0 tradeoff + upgrade table in the living § and both generation diagrams
- [x] U1 `map_sources` `assert_no_deps` render；`place` �?CPU 集。`scene_cpu_sources` 仝坯冝收
- [x] U2 `vista/component/map` 逻辑〝`vista/frame` 物睆〝`FramePass`
- [x] U3 `domain/atmosphere` �?`atmosphere/session` (`session_sources`). GPU atmosphere target does not depend on it. `domain.h` stays
- [x] U4 re-emit `DrawItem` slices whose `cache_key` missed (`retained_slices`). `FramePass::upload_keyed_slices` on the device thread. No HDC

Follow-on M1–M4: living **§Vista map/frame scenic-peer**（AABB tile dirty〝prep 波〝DrawItem coalesce〝emit abort）�?
**M4 host wiring (2026-10-05):** `Map2dFrameCache::rebuild_layout` sets `LayoutInput::layout_gen` + `live_layout_gen` (`std::atomic<uint64_t>` on the cache; mailbox `request_gen_` is the same atomic on the E-lane path). After `Layout::build`, if live gen �?the passed gen the host does not `absorb_layer_slices` / does not publish (`cached_frame_` / `published_` stay). `retained_slices` unchanged. No HWND join.

U5 / multi-CL stays Task 5. Do not start it from this task.

### Task 9: present dual-track ceiling（T1 GPU 脏区 ∥ T2 Scene3d 并行）— living § 2026-10-25

Hung off **§Present dual-track ceiling**. Do **not** open a twin plan. Path-partitioned: T1 and T2 may land in parallel.

**Diagram:** [`../diagrams/content-present-dual-track.html`](../diagrams/content-present-dual-track.html)

#### T1 — GPU 脏区（Map2d）

- [x] T1.0: `PresentDamageReasons` bitset + fill in `prepare_for_present`; phase probe `damage_reasons`; PresentAction behavior **equivalent** to today
- [x] T1.1: Selection / shell-only non-empty `DamageRect` → MapPass encode scissor; union previous rect; CameraOnly stays full-frame G0
- [ ] T1.2: (optional) partial present / swapchain path when backend supports; else full-frame fallback
- [ ] Keep dual-track HTML in sync; no `DamageTracker` layer tree; no C1 revive

#### T2 — Scene3d FrameJob 并行

- [x] T2.0: Scene3d mailbox `stage` / `drain` (Map2d-shaped); phase probe `prep_overlap_ms`
- [x] T2.1: terrain CPU remesh on `base::execution` via mailbox `stage_async` + staging hub; overlap previous Display present; no Device on workers (atmosphere POD prepare remains Display — residual)
- [ ] T2.2: warm `rebuild_count=0` skips `prepare_gpu` + remesh; orbit-only encode
- [ ] T2.3: frustum `prep_cull_parallel` only after `SCENE3D_FRUSTUM_CULL` honesty (default still off)
- [ ] Keep dual-track HTML in sync; no worker FFT/SRV; no Map2d DrawCache edits from T2

## Non-goals

- Port `Rhi2dTileGraphRunner` / per-thread HDC into Vista
- Shared GL multi-thread Draw
- Second thread pool in `gis` / `render`
- N cameras / N CLs per `graph::present` (except optional P3b)
- Making leftover the product default again
- Inventing parallel type trees beside `Layout` / `Map2dFrameCache` / `GpuScene`

## Relation

- **Task 8** is the logical/physical upgrade (U1 compile wall, U2 rename, U3 atmosphere session, U4 dirty-slice emit). Scenic stays the compare/GDI lane
- **Task 9** is the present ceiling dual-track (T1 Map2d damage/scissor ∥ T2 Scene3d FrameJob); hung off **§Present dual-track ceiling**
- Product-track reuse of §rhi2d tile-raster / §rhi3d parallel-frame **semantics**
- Complements §Vista Map2d / §src_render Scene3d equal-profile (budgets vs architecture；world3d **M1–M4** + `GPUSCENE_PREP_PARALLEL` default-off)
- Views §compositor thread owns L0/L3 thread roles
- **§GPU-process accelerate** (Topology B): when shell runs `--type=gpu`, L3 remaps from Display `graph::present` to **submit IR / DrawRequest → GPU process** (`GpuDeviceHub` + `FrameComposer`). Open bridge checkboxes live only in [`2026-09-27-gpu-rhi-accelerate.md`](2026-09-27-gpu-rhi-accelerate.md) **Task 8**.
