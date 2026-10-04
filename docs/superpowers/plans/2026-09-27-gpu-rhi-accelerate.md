<!--

Copyright (c) 2026 The Mogu Authors.

All rights reserved.

-->



# GPU process RHI accelerate — Implementation Plan



> **Design living:** [../specs/2026-09-13-render-rhi-scene-design.md](../specs/2026-09-13-render-rhi-scene-design.md) **§GPU-process accelerate**. This file is the **sole checklist** for Topology B (GPU process) + **Task 8 bridge** to in-process L0–L3.
> **Diagram (normative):** [`../diagrams/render-accelerate-topology.html`](../diagrams/render-accelerate-topology.html)（A×B 深度整合 · §4–§6 Topology B + Bridge）
> **In-process peer (Topology A, no duplicate GPU checkboxes):** [`2026-10-02-src-render-vista-parallel-accelerate.md`](2026-10-02-src-render-vista-parallel-accelerate.md)
> **Archive (superseded):** [`../archive/specs/2026-09-27-gpu-rhi-accelerate-design.md`](../archive/specs/2026-09-27-gpu-rhi-accelerate-design.md)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Keep `CompositorFrame` as IR in `--type=gpu`, introduce `GpuDeviceHub` + `AdapterId` (1 process × N devices), pluggable per-device `FrameComposer` (`kSoftware` | `kRhi`), and accelerate present/compose via `render::rhi` **only inside the GPU process**. Chrome / browser / renderer never perform final compose. Bridge in-process Views L3 (`graph::present`) to GPU-process submit when multiprocess is on.

**Architecture:** Raster (or later IPC) still produces `DrawQuad`s. `GpuDeviceHub` pins each `OutputSurface` to an `AdapterId`. `display` selects a `FrameComposer` for that adapter. Software path preserves today's CPU blend + `upload_bgra` as a **per-device** fallback inside gpu. RHI path grows from shared-surface blit → GPU compose → optional Frame Graph / `GpuScene` underlay on the same adapter. L0–L2 parallel semantics shared with §vista; L3 remaps under Topology B. Namespaces: `gpu` / `gpu::detail`, `render::rhi`, `render::graph` (two public levels).

**Tech Stack:** C++23, GN/`build.bat`, DXGI adapters + shared `OutputSurface`, FlyCube behind `render::rhi`, Null RHI for tests.

**Living §:** [`../specs/2026-09-13-render-rhi-scene-design.md`](../specs/2026-09-13-render-rhi-scene-design.md) §GPU-process accelerate



## Global Constraints



- Stay on `master`; no feature branch.

- Humans run `build.bat` / tests; agents do **not** compile or launch `out\*.exe`.

- **All final compose/present runs in `--type=gpu`.** Chrome / `app/` / `content/` / renderer must not blend the final frame.

- Chrome / `app/` / `content/` must not `#include` `render/rhi` or `gpu/compositor/`.

- Default topology: **one GPU process, N device slots** (not N gpu processes).

- Each `OutputSurface` / view is pinned to an `AdapterId`; unbound → `kAdapterPrimary`.

- ~~Default compose backend remains `kSoftware` until explicitly opted in (`SMT_GPU_COMPOSE=rhi` or test override). Fallback is **per adapter**.~~ **Superseded** by **Follow-up: default RHI + monitor LUID (A+C)** below — default is now `kRhi`; `SMT_GPU_COMPOSE=software` is the escape; sticky fallback remains **per adapter**.

- Wire command `view.backend.rhi` still means `ContentSource::kDirect` — do not overload it to mean FlyCube in this plan.

- No Qt; no second compositor IR; no commit unless the user asks.



---



## File map (planned)



| Path | Role |

| --- | --- |

| `src/gpu/device/adapter_id.h` | `AdapterId`, `AdapterInfo`, constants |

| `src/gpu/device/gpu_device_hub.h` / `.cc` | Process-wide hub; bind surface → adapter; device slots |

| `src/gpu/compositor/frame_composer.h` | `ComposeBackend`, `FrameComposer`, select/make APIs |

| `src/gpu/compositor/frame_composer.cc` | Selection + factory (software always; RHI when ready) |

| `src/gpu/compositor/software_renderer.*` | Implement software path (adapt to `FrameComposer` or wrap) |

| `src/gpu/compositor/rhi_composer.h` / `.cc` | RHI path (optional GN sources); per-adapter device |

| `src/gpu/display/display.cc` | Hub bind + `make_frame_composer` instead of hard-wired `SoftwareRenderer` |

| `src/gpu/display/output_surface.*` | `adapter_id` pin; DXGI create on bound adapter; RHI import hooks |

| `src/gpu/BUILD.gn` | device + composer sources; optional `gpu_rhi_composer` |

| `src/gpu/paint_frame_test.cc` / new `rhi_composer_test.cc` | Contract + hub/RHI tests |

| `src/gpu/README.md` | As-built notes when a phase lands |



---



### Task 1: M0 — `AdapterId` + `GpuDeviceHub` stub + `FrameComposer` seam



**Files:**

- Create: `src/gpu/device/adapter_id.h`, `src/gpu/device/gpu_device_hub.h`, `src/gpu/device/gpu_device_hub.cc`, `src/gpu/compositor/frame_composer.h`, `src/gpu/compositor/frame_composer.cc`

- Modify: `src/gpu/compositor/software_renderer.h`, `software_renderer.cc`, `src/gpu/display/display.cc`, `src/gpu/display/output_surface.h`, `src/gpu/BUILD.gn`

- Test: `src/gpu/paint_frame_test.cc` (extend if needed)



**Interfaces:**

- Consumes: existing `SoftwareRenderer::draw_frame`, `CompositorFrame`, `OutputSurface`

- Produces: `AdapterId`, `GpuDeviceHub`, `FrameComposer`, `ComposeBackend`, `select_compose_backend`, `make_frame_composer`



- [x] **Step 1:** Add `adapter_id.h` and `GpuDeviceHub` stub that always reports `kAdapterPrimary`, binds surfaces, and returns that id from `adapter_of`.



- [x] **Step 2:** Add `frame_composer.h` with `ComposeBackend { kSoftware, kRhi }`, abstract `FrameComposer` (`backend`, `adapter`, `draw_frame`), and selection/factory declarations matching the spec.



- [x] **Step 3:** Implement `SoftwareComposer` (or make `SoftwareRenderer` inherit `FrameComposer`) so `draw_frame` behavior is byte-identical to today. Store the `AdapterId` passed to the factory.



- [x] **Step 4:** Implement `select_compose_backend` / `set_compose_backend` / `clear_compose_backend_override`. Read `SMT_GPU_COMPOSE=software|rhi` (case-insensitive). Unknown / unset → `kSoftware`. Until Task 4, `make_frame_composer(kRhi, …)` returns software with a one-time log — **display must never get a null composer**.



- [x] **Step 5:** Change `display.cc` submit paths to use hub + `make_frame_composer`.



- [x] **Step 6:** Give `OutputSurface` an `AdapterId` field + accessors (`adapter_id` / `set_adapter_id`). Hub `bind_surface` sets it.



- [x] **Step 7:** List new sources in `gpu_backend`.



- [ ] **Step 8 (human):**



```bat

build.bat render_backend_test.exe

out\render_backend_test.exe

```



Expected: all existing cases PASS; default path still software; compose still only under `src/gpu`.



- [ ] **Step 9:** Commit only if the user asks.



---



### Task 2: M1a — Real adapter enumeration + DXGI on bound adapter



**Files:**

- Modify: `src/gpu/device/gpu_device_hub.*`, `src/gpu/display/output_surface.*`

- Test: unit coverage via hub tests / composer tests



**Interfaces:**

- Consumes: DXGI factory adapter list, `OutputSurface` resize

- Produces: real `AdapterInfo` list; `create_dxgi` uses the bound adapter (not always `D3D11CreateDevice(nullptr)`)



- [x] **Step 1:** Enumerate DXGI adapters into `GpuDeviceHub::enumerate_adapters`.



- [x] **Step 2:** `prefer_adapter_for_monitor(HMONITOR)` via DXGI output ↔ monitor (best-effort; fall back primary).



- [x] **Step 3:** `OutputSurface::create_dxgi` takes / reads bound `AdapterId` and creates the D3D11 device on that adapter.



- [x] **Step 4:** Expose narrow accessors for RHI import: `void* shared_texture_d3d11()`, `HANDLE share_handle()`, `bool import_ready() const` — **no** FlyCube types in this header.



- [x] **Step 5:** Keep `upload_bgra` as the software present; do not remove it.



- [x] **Step 6:** Resize / release clears any RHI import caches via generation bump already on `wire_.generation`.



- [ ] **Step 7 (human):** Re-run `render_backend_test` (software path unchanged). Dual-monitor hand pin if available.



---



### Task 3: M1b — `RhiComposer` — RHI present of CPU-blended buffer (per adapter)



**Files:**

- Create: `src/gpu/compositor/rhi_composer.h`, `src/gpu/compositor/rhi_composer.cc`

- Modify: `src/gpu/compositor/frame_composer.cc`, `src/gpu/device/gpu_device_hub.*`, `src/gpu/BUILD.gn`

- Optional: narrow GN target under `src/render/rhi`



**Interfaces:**

- Consumes: `render::rhi::Device` for the surface’s `AdapterId`, `blend_render_pass`, `OutputSurface`

- Produces: `RhiComposer::draw_frame` that blends on CPU then copies BGRA into the shared surface via RHI **or** falls back to `upload_bgra` if import fails **for that adapter**



- [x] **Step 1:** Spike: import existing NT shared handle into FlyCube DX12 — **landed** via `OpenSharedHandle` + `WrapSwapchainBackBuffer` + RTV; NT handle `READ\|WRITE`. Compose-direct = `execute_to_imported`.



- [x] **Step 2:** Implement `RhiComposer` with stored `AdapterId`:

  1. `ensure_rhi_device` + GPU compose; prefer `composed_into_imported_shared`.

  2. Texture upload path (BGRA→RGBA) for quads.

  3. `copy_bgra_to_imported_shared` then `upload_bgra` fallbacks.

  4. Sticky software per adapter on hard failure.



- [x] **Step 3:** `make_frame_composer(kRhi, adapter)` returns `RhiComposer` when device init succeeds; else sticky software for that adapter.



- [x] **Step 4:** GN: `gpu_backend` deps `//src/render:rhi` (+ graph/scene). Always-on for RHI compose path (documented in README).



- [ ] **Step 5:** Add `gpu_rhi_composer_test` covering: hub bind; select rhi → fallback when device null; two logical adapters with independent sticky flags; BGRA channel order smoke on Null where applicable.



- [ ] **Step 6 (human):**



```bat

build.bat render_backend_test.exe

out\render_backend_test.exe

REM optional:

set SMT_GPU_COMPOSE=rhi

out\render_backend_test.exe

```



Expected: default suite green; with `SMT_GPU_COMPOSE=rhi`, RhiComposer path runs (present still upload_bgra until FlyCube import).



---



### Task 4: M2 — GPU compose of `kSolid` / `kBgra` / `replaces` (per device)



**Files:**

- Modify: `src/gpu/compositor/rhi_composer.*`

- Test: extend `gpu_rhi_composer_test` + run tile cases under `SMT_GPU_COMPOSE=rhi`



**Interfaces:**

- Consumes: root `RenderPass` quad list on the bound device

- Produces: GPU src-over (and copy for `replaces=true`) into shared surface without full CPU `blend_render_pass` when backend is `kRhi` and that adapter’s device is live



- [x] **Step 1:** Implement fullscreen / scissored solid fill and textured blit pipelines via `render::rhi` (record+execute).



- [x] **Step 2:** Honor `opacity`, `replaces`, and back-to-front order (pixel truth still `blend_render_pass` until GPU readback).



- [x] **Step 3:** On hard present failure: sticky software for **that adapter**.



- [ ] **Step 4 (human):** Tile opacity / multi-raster / replaces under RHI compose; compare to software.



---



### Task 5: M3 — Texture cache for tile quads (per DeviceSlot)



**Files:**

- Modify: `rhi_composer.*`, `gpu_device_hub.*`, possibly `raster/tile/*` for stable cache keys without leaking RHI types into raster headers



**Interfaces:**

- Consumes: `DrawQuad` + optional cache key in `detail` only

- Produces: reused `render::rhi::Texture*` across frames on the **same AdapterId** when pixels unchanged



- [x] **Step 1:** Define cache key in `DrawQuad::texture_cache_key` + tile layer FNV keys.



- [x] **Step 2:** Evict on hub `slot_generation` change (device-lost / rebind).



- [ ] **Step 3 (human):** XYZ mosaic hand-test (`SMT_MAP_BACKEND=a` + `SMT_XYZ_URL=...` + `SMT_GPU_COMPOSE=rhi`).



---



### Task 6: M4 — Frame Graph / GpuScene / effect::map underlay bridge (same AdapterId)



**Files:**

- Create: `src/gpu/compositor/rhi_underlay_bridge.h` / `.cc` (name flexible; stay in `gpu::detail`)

- Modify: GPU process init to optionally own per-adapter `render::rhi::Device` shared by composer + graph

- Do **not** modify chrome includes; do **not** compose in browser



**Interfaces:**

- Consumes: `render::graph::present` **or** a single underlay `Effect` list on the surface’s adapter; output texture

- Produces: blit underlay into shared surface before overlay quads, **or** inject as root `DrawQuad`



- [x] **Step 1:** Adapter only: `frame_graph.cc` remains unaware of `OutputSurface` (`rhi_underlay_bridge`).



- [x] **Step 2:** Scene3d / GpuScene underlay record path via `OpaqueEffect` + `record_gpu_scene_underlay` (BGRA readback still TODO).



- [ ] **Step 3:** effect/map: CPU `gis::vista::Layout` stays in gis; RHI Pass is `effect::map` (`src/vista/map`); bridge copies color target into compositor on the same `AdapterId` — deferred until offscreen readback.



- [ ] **Step 4 (human):** Multiprocess smoke: browser presents shared handles only; underlay visible; GPU crash does not kill chrome.



---



### Task 7: M5 — Fallback, TDR, adapter rebind, docs as-built



**Files:**

- Modify: `src/gpu/README.md`, this plan checkboxes, spec Status when M2+ accepted

- Modify: composer / hub device-lost handling



- [x] **Step 1:** On device-lost for adapter A: `notify_device_lost` destroys A’s RHI, sticky software for A, bumps surface generations.



- [x] **Step 2:** `rebind_surface_to_monitor` re-pins via `prefer_adapter_for_monitor`.



- [x] **Step 3:** Update `src/gpu/README.md` architecture + RHI subsection (landed vs blocked).



- [x] **Step 4:** Cross-link from layout / frame-graph / multiprocess docs Related lines if missing（`ui-shell-multiprocess.md` + living diagrams 2026-10-02）.



- [ ] **Step 5 (human):** Kill GPU child under load; confirm browser recovery; dual-adapter sticky fallback still matches `docs/superpowers/ui-shell-multiprocess.md`.



---



## Spec coverage checklist



| Spec requirement | Task |

| --- | --- |

| GPU-process-only compose | Global + Task 1, 6 |

| Multi-GPU hub / `AdapterId` | Task 1–2, 7 |

| Pluggable software \| rhi composer per device | Task 1, 3–4 |

| Keep `CompositorFrame` IR | Task 1–4 (no alternate IR) |

| IPC: content submits; chrome present-only | Spec IPC § + Task 6–7 |

| OutputSurface ↔ RHI mapping per adapter | Task 2–3 |

| No chrome RHI include | Global + Task 6 |

| Frame Graph / effect::map / GpuScene ownership | Task 6 |

| Phased M0–M5 acceptance | Tasks 1–7 |

| Risks TDR / shared texture / per-adapter fallback | Task 3 spike, Task 7 |

| Topology A↔B bridge (L0–L3 remap) | Task 8 |

| Dual-topology docs / diagrams | Task 8 + living § |



## Execution note



Plan complete when saved. Implementation continues on `master` under the multi-GPU GPU-process compose model. Prefer subagent-driven execution per task with human `build.bat` gates between tasks.



---



## Follow-up: default RHI + monitor LUID (A+C)



> Living design: [../specs/2026-09-13-render-rhi-scene-design.md](../specs/2026-09-13-render-rhi-scene-design.md) §GPU-process accelerate (A+C). Earlier Global Constraints that said software-default are **superseded** by this section.



**Locked:** default `ComposeBackend` = `kRhi` (unset `SMT_GPU_COMPOSE` → `kRhi`); `SMT_GPU_COMPOSE=software` escape; sticky per-adapter software fallback; Attach/Resize carry monitor LUID (+ optional `adapter_hint`); `HMONITOR` never on the wire; topology stays 1 gpu process × N adapters; shell never final-compose.



### P0 — Default RHI compose



- [x] Flip / verify `select_compose_backend` / env parse: unset / empty / unknown → `kRhi`; only `SMT_GPU_COMPOSE=software` → `kSoftware` (`src/gpu/compositor/composer/composer.cc`, `composer.h`).
- [x] Keep sticky per-adapter software fallback on RHI init / hard present failure (`src/gpu/device/gpu_device_hub.*`, `make_frame_composer`).
- [x] Update unit tests expecting software-default (`src/gpu/*_test.cc` / `gpu_rhi_composer_test`).
- [x] Align as-built wording: `src/gpu/README.md` Multi-GPU + living §GPU-process accelerate.

### P1 — Monitor LUID IPC + rebind



- [x] Extend `AttachSurfaceBody` / `ResizeSurfaceBody` with `monitor_luid_low` / `monitor_luid_high` and optional `adapter_hint` (`0xffffffff` = unset) — `src/content/common/host_protocol.h`.
- [x] Shell / browser fill LUID from local `HMONITOR` (do **not** put `HMONITOR` on IPC) — e.g. `src/content/browser/contents/map_contents.cc`, Views map host as needed.
- [x] `gpu_main` on Attach/Resize: resolve adapter via `GpuDeviceHub` and `bind_surface` / `rebind_surface_to_monitor` (LUID), not forever-primary — `src/gpu/gpu_main.cc`, `src/gpu/device/gpu_device_hub.*`.
- [ ] Tests: Attach/Resize with LUID pins expected `AdapterId`; primary-only path remains fallback when LUID unset.

### P2 — TDR recovery + docs



- [ ] Wire device-lost / TDR recovery to `notify_device_lost` → sticky software + surface generation bump for that adapter (`src/gpu/device/gpu_device_hub.*`, display / present path).
- [ ] Dual-adapter sticky-fallback hand notes + as-built refresh (`src/gpu/README.md`, `docs/superpowers/ui-shell-multiprocess.md` if behavior changes).
- [ ] Confirm shell still never final-compose; no single-frame multi-GPU split.

---

## Task 8: Bridge to in-process L0–L3（Topology A ↔ B）

> Living fold: §GPU-process × §src_render + vista parallel (2026-10-02).
> In-process peer plan: [`2026-10-02-src-render-vista-parallel-accelerate.md`](2026-10-02-src-render-vista-parallel-accelerate.md).
> Diagram: [`../diagrams/render-accelerate-topology.html`](../diagrams/render-accelerate-topology.html)（§6 Bridge）.

**Goal:** When multiprocess is on, map underlay / graph output **submits to the GPU process** (`AdapterId` pin); UI never joins; chrome never blends. Keep a single `CompositorFrame` IR. Do not invent a second present on the same HWND.

**Files (anchors):**

- `src/gpu/frame_sink.h` · `src/gpu/display/display.cc` (`draw_and_swap`)
- `src/gpu/compositor/underlay/underlay_bridge.*` (`record_underlay_effects` / `record_gpu_scene_underlay`)
- `src/gpu/device/gpu_device_hub.*` · `src/content` presenters / Views Display mailbox
- Living § tables in `2026-09-13-render-rhi-scene-design.md`

- [x] Documented mode switch: Topology A (Display `graph::present`) vs B (`--type=gpu` `FrameComposer`) — living § + **统一规范图** [`render-accelerate-topology.html`](../diagrams/render-accelerate-topology.html)
- [ ] Views multiproc: Map2d/Scene3d cold path submits DrawRequest / underlay to gpu process (not local final blend)
- [ ] Underlay: finish BGRA readback / blit into shared surface before overlay quads (`underlay_bridge` + `RhiComposer`)
- [ ] effect::map color-target → compositor on same `AdapterId` (Task 6 Step 3)
- [x] Assert: no Skia Ganesh map; no dual HWND FlyCube + shared-surface present; `view.backend.rhi` still = `ContentSource::kDirect`（locked in living § Non-goals / Env）
- [ ] Human smoke: browser present-only SharedHandle; kill `--type=gpu` → recover; dual-adapter sticky software

