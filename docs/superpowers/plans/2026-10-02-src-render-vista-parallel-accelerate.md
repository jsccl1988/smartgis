<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# src_render + vista parallel accelerate (终态) — Implementation Plan

> Hung off living [`../specs/2026-09-13-render-rhi-scene-design.md`](../specs/2026-09-13-render-rhi-scene-design.md) **§src_render + vista parallel accelerate**.  
> **Directory tighten (not this plan):** living **§Vista subdirectory tighten** · [`../diagrams/vista-subdirectory-layers.html`](../diagrams/vista-subdirectory-layers.html). Do not duplicate move tables here.  
> **Diagram (normative visual):** [`../diagrams/render-accelerate-topology.html`](../diagrams/render-accelerate-topology.html) — **A×B 深度整合**；Topology A 见 §2–§3（`stage_frame` → `build_layout_parallel` → `prep_cull_parallel` → `record_and_present`）。  
> **CPU boundary:** [`gis-vista-architecture.html`](../diagrams/gis-vista-architecture.html).  
> **Related:** [`legacy-render-architecture.html`](../diagrams/legacy-render-architecture.html) · [`ui-views-shell-architecture.html`](../diagrams/ui-views-shell-architecture.html)  
> **GPU-process checklist (do not duplicate here):** [`2026-09-27-gpu-rhi-accelerate.md`](2026-09-27-gpu-rhi-accelerate.md) — **Task 8: Bridge to in-process L0–L3**.

**Goal:** Product-track parallel end state for `src/vista` (CPU MapFrame / World + GPU map / GpuScene / atmosphere in **vista.dll**) + `src/render` / `content` (present), reusing leftover rhi2d/rhi3d **semantics** — not GDI HDC or D3D11 deferred. Living § now locks **code-level** Types / Call graph / Grains / GPU / Env / API sketches.

**Architecture (L0–L3):** UI never joins → content FrameJob schedule → vista / GpuScene `parallel_for` → Display sole Device + **one** CommandList. Gen / cancel / StaticReuse gate publish.

**Tech Stack:** C++23, `base::execution`, `gis::vista::Layout` / `MapFrame`, `render::{rhi,graph}`, `vista::{GpuScene,map pass,atmosphere pass}`, content presenters, Views Display mailbox.

## Design summary (locked — see living § §§1–7)

| Layer | Owner | One-liner |
| --- | --- | --- |
| L0 UI | Views HWND / Commit | `stage_frame` only · 禁 join |
| L1 Schedule | `content` Map2d/Scene3d | gen / cancel / clocks / wake |
| L2 CPU | `gis::vista` (+ prep) | `build_layout_parallel` / `prep_cull_parallel` |
| L3 GPU | Display · `graph` / `vista` GPU passes | `record_and_present` ≡ `graph::present` · 1 CL |

Env (product): `SMT_VISTA_LAYOUT_PARALLEL`; `SMT_GPUSCENE_PREP_PARALLEL` **default off** until frustum cull honesty (`=0` → N=1; opt-in `=1` after `SMT_SCENE3D_FRUSTUM_CULL`) — see Scene3d equal-profile **M3** [`2026-10-01-src-render-scene3d-equal-profile-optimize.md`](2026-10-01-src-render-scene3d-equal-profile-optimize.md). Leftover `SMT_RHI2D_*` / `SMT_RHI3D_*` must not drive product path.

## Global Constraints

- Work on **`master`** only.
- `gis` ↛ `render/rhi` / HWND; `render` ↛ `legacy_render`.
- UI / HWND **never** `join`s layout or GPU workers.
- Sole pool: `base::execution`. No second product pool.
- Comments English; helpers `snake_case`; two-layer namespaces.
- Design HTML stays **light technical** (`.cursor/rules/repo/design-html-diagrams.mdc`).
- **Do not** `git commit` unless the user asks.

## File map (as-built anchors → hooks)

| Path | Role | API / symbol |
| --- | --- | --- |
| `vista/frame/frame.h` | Types | `LayoutInput` · `LayerBatch` · `MapFrame` · `Layout` |
| `vista/frame/layout.cc` | CPU orchestrator | `Layout::build` |
| `vista/frame/detail/layout/fill.cc` · `line.cc` | Tess grain | `emit_fills` / `emit_lines` + `parallel_for` |
| `vista/frame/detail/layout/{point,symbol}.cc` | Unify V1 | `emit_circles` / `emit_heatmap` / `emit_symbols` |
| `vista/frame/detail/layout/geom_mesh.h` | Thresholds | `kParallelTessMinGeoms` · `kParallelTessGrain` |
| `vista/world/terrain/mesh/*` | TLS scratch | tess helpers |
| `content/.../map2d/frame/map2d_frame_cache.*` | L1 cache | `prepare_for_present` · `rebuild_layout` → V2 `stage_frame` |
| `content/.../map2d/frame/map2d_batches.cc` | Layer jobs | `batches_via_parallel_for` · `merge_parts` |
| `content/.../map2d/gpu/map2d_gpu_present.cc` | Warm/cold GPU | `present` · `present_frame` → `graph::present` |
| `content/.../map2d/map2d_phase_profile.*` | Clocks | `layout_ms` · `gpu_upload_ms` · `present_ms` |
| `content/.../scene3d/gpu/scene3d_gpu_present.cc` | 3D present | `Scene3dGpuPresent::present` |
| `vista/map/pass.*` | Upload + record | `Pass::record` · `invalidate_uploaded` |
| `vista/scene/scene_draw.cc` · `detail/draw_pass.cc` | 3D draw | `GpuScene::record_draws` · `record_kind` · frustum |
| `vista/atmosphere/ocean/gpu_fields.cc` | Compute FFT | Device-thread only |
| `render/graph/frame_graph.*` | 1 CL contract | `render::graph::present` |
| `base/execution/parallel/for.h` | Pool grain | `parallel_for` · latch |
| `docs/superpowers/diagrams/render-accelerate-topology.html` | Normative SVG（A×B） | Named stages + GPU process |
| `src/render/README.md` | Env pointer | Parallel / GPU (planned) |

## Tasks

### Task 1: Contract doc + env surface — V0

- [x] Living § locked (L0–L3 + **代码级** §§1–7)
- [x] Light HTML diagram (layering + named pipeline stages)
- [x] Document env pointer in `src/render/README.md`（接线代码仍属 V1）
- [x] Explicit non-goal: no GDI tile HDC / no D3D11 deferred on product path

### Task 2: Vista layout grain (Map2d CPU) — V1

- [x] Fill / line tess `parallel_for` + per-worker scratch (as-built)
- [ ] Wire `SMT_VISTA_LAYOUT_PARALLEL` helper（`vista_layout_parallel_enabled()`）in `geom_mesh.h` / emitters；`=0` → N=1
- [ ] Unify symbol / circle / heatmap / extrusion under job→merge (preserve painter z)
- [ ] Optional viewport **tile layout** grain (device-pixel AABB) for full-damage china — peer of leftover `tile`; GPU compose = `vista/map` (not TransparentBlt) → may slip to V4
- [ ] Collision / label resolve **serial** after parallel emit (document + assert in `emit_symbols`)
- [ ] Default on when `jobs >= kParallelTessMinGeoms`

### Task 3: Content FrameJob semantics — V2

- [ ] Map2d: `stage_frame(FrameRequest)` → worker `build_layout_parallel` writes `cached_frame_`；UI / HWND waits publish only（Display mailbox）
- [ ] Split today's sync `rebuild_layout` off the present critical path when gen advanced
- [ ] Scene3d: GPU record/present on Display；CPU mesh/cull may fan out before record
- [ ] Stale `layout_gen` never publishes；destroy cancels without HWND join
- [ ] Phase clocks: `layout_ms` / `prep_ms` / `upload_ms` / `record_ms` / `present_ms`（align equal-profile §§）

### Task 4: GpuScene / 3D prep — V3

- [ ] Extract `prep_cull_parallel`（`vista/scene` + `frustum_aabb`）；workers write `visible[]` only
- [ ] Gate behind `SMT_SCENE3D_FRUSTUM_CULL=1`（as-built default off）+ `SMT_GPUSCENE_PREP_PARALLEL`
- [ ] Product **default off** for `SMT_GPUSCENE_PREP_PARALLEL` until cull honesty（world3d matrix: `prep_par_on` slower — equal-profile **M3**）
- [ ] When enabled: `=0` → serial；`=1` → clamp 2–4
- [ ] Workers must **not** touch `rhi::Device` / CommandList
- [ ] Serial `GpuScene::record_draws` / `graph::present` on GPU thread only
- [ ] Acceptance cross-link: [`2026-10-01-src-render-scene3d-equal-profile-optimize.md`](2026-10-01-src-render-scene3d-equal-profile-optimize.md) M3

### Task 5: GPU record (P3 peer) — V5 deferred

- [ ] Default: **single** CommandList — no multi-thread record until cold `record_ms` hot after V3
- [ ] Optional P3b: FlyCube multi-CL bundles (DX12)；never import leftover D3D11 deferred TLS
- [ ] Ocean FFT compute stays Device-thread（`OceanGpuFields::record`）

### Task 6: Acceptance harness

- [ ] Map2d china 1280×720: warm StaticReuse per equal-profile；layout parallel cut vs `SMT_VISTA_LAYOUT_PARALLEL=0`
- [ ] Scene3d `--atmosphere-showcase=legacy`: warm leftover-order；prep scales with N when cull on
- [ ] Visual gates unchanged (hillshade / labels / legacy landish)
- [ ] Keep HTML diagram in sync when phases land (revise in place)

## Non-goals

- Port `Rhi2dTileGraphRunner` / per-thread HDC into FlyCube
- Shared GL multi-thread Draw
- Second thread pool in `gis` / `render`
- N cameras / N CLs per `graph::present` (except optional P3b)
- Making leftover the product default again
- Inventing parallel type trees beside `Layout` / `Map2dFrameCache` / `GpuScene`

## Relation

- Product-track reuse of §rhi2d tile-raster / §rhi3d parallel-frame **semantics**
- Complements §Vista Map2d / §src_render Scene3d equal-profile (budgets vs architecture；world3d **M1–M4** + `SMT_GPUSCENE_PREP_PARALLEL` default-off)
- Views §compositor thread owns L0/L3 thread roles
- **§GPU-process accelerate** (Topology B): when shell runs `--type=gpu`, L3 remaps from Display `graph::present` to **submit IR / DrawRequest → GPU process** (`GpuDeviceHub` + `FrameComposer`). Open bridge checkboxes live only in [`2026-09-27-gpu-rhi-accelerate.md`](2026-09-27-gpu-rhi-accelerate.md) **Task 8**.
