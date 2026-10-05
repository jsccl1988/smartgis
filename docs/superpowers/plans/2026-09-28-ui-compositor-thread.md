<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# UI compositor thread — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Land Chromium-style UI / compositor / raster / GPU thread roles for Views shell + map present, phases P0–P5, without Blink/cc/Mojo viz.

**Architecture:** UI thread owns HWND and Commits immutable `DisplayList` snapshots. Compositor thread holds pending/active and builds `CompositorFrame` (map quads + shell `kBgra` + HUD). One raster worker replays dirty rects into resources. GPU thread exclusively owns `rhi::Device` and `gpu::draw_and_swap`; `GpuPresentFn` becomes Submit. BeginFrame (DWM-aligned) lands only after P4. Shutdown via `Widget::will_close`.

**Tech Stack:** C++23, GN, `ui::views` / `ui::gfx` / `gpu::detail`, existing Vista RHI. No Qt. No `--type=gpu` this phase.

**Spec:** [`../specs/2026-09-27-views-desktop-shell-design.md`](../specs/2026-09-27-views-desktop-shell-design.md) §Shell perf upgrade waves（compositor roles；P0–P5 predecessor）— dated twin archived at [`../archive/specs/2026-09-28-ui-compositor-thread-design.md`](../archive/specs/2026-09-28-ui-compositor-thread-design.md)  
**Diagram:** [`../diagrams/ui-views-shell-architecture.html`](../diagrams/ui-views-shell-architecture.html)

## Global Constraints

- Namespace stays two public layers (`ui::views`, `ui::gfx`, `gpu` + `gpu::detail`). New functions are `snake_case`.
- No HWND on compositor / raster / GPU threads. No Skia Ganesh for shell. No `SmtGdiRenderThread` reuse.
- Do not vendor Chromium. Do not introduce property trees or Mojo viz.
- Agents do not run `build.bat`, `gn`, `ninja`, `cl`, or test binaries. The human compiles.
- Work on `master`. Do not create a branch. Do not commit unless the user asks.
- Copyright year 2026. New comments in English. Colocate headers with sources.
- Parallel-land P0–P5 is approved: tracks may draft in parallel, but **behavior switches follow phase dependencies** (P5 after P4). Shared files (`Widget`, `MapViewport`, `frame_sink`, `BUILD.gn`, this plan) belong to the coordinator.

## Phase checklist

### P0 — PaintCounters gate

- [x] Expose or extend `ui::gfx::PaintCounters` (or adjacent counters) for **commit** / **raster** / **present** timings (`note_commit_qpc` / `note_raster_qpc` / `note_present_qpc`)
- [x] Wire counters at today's sync call sites so baselines exist before threading (`PaintCommit`, `ShellCompositor`, Widget paint)
- [x] Unit or harness assertion reads the three buckets (`views_unittests` or gfx test)
- [x] Document how later phases must keep the gate green (`ui/gfx` PaintCounters comments)

### P1 — Commit boundary (same-thread)

- [x] Define immutable Commit snapshot type wrapping `ui::gfx::DisplayList` (+ size / dpi / shell metadata as needed) — `PaintCommit`
- [x] UI invalidation path: record → Commit snapshot; do not paint from the live mutable list
- [x] `WM_PAINT` / shell paint: **replay only** the last committed snapshot (`ShellCompositor::present` BitBlt)
- [x] Tests: mutating the live tree after Commit does not change the in-flight paint snapshot
- [x] Keep all work on UI thread; no new threads yet (P1 landed before P2 worker)

### P2 — Compositor thread pending / active

- [x] Introduce compositor thread (or dedicated serial task runner) — `ShellCompositor` worker
- [x] Pending queue: UI posts Commit; compositor accepts latest, drops superseded pending
- [x] Active tree: Activate copies/moves pending → active for raster / frame build
- [x] UI must not write active; compositor must not touch HWND
- [x] Smoke: shell still paints; counters still report commit vs present

### P3 — Raster worker (start with 1)

- [x] One raster worker thread; queue dirty-rect jobs from active snapshot
- [x] Replay `DisplayList` into resources consumed as shell `kBgra` / textures
- [x] Integrate with `ShellRaster` / `attach_shell_raster` path without HWND (`Widget::shell_raster` + host `commit_shell_overlay`)
- [x] Drain on shutdown (see Shutdown); reject new jobs after stop
- [x] Test or diagnostic: dirty-rect job completes before Submit sees the resource (`wait_published`)
- [ ] Multi-worker raster pool — **deferred** (start with 1; optimize later)

### P4 — GPU thread owns draw_and_swap

- [x] GPU / Display thread: exclusive `rhi::Device` init / destroy (`MapViewport` Display mailbox)
- [x] Move `gpu::draw_and_swap` calls onto that thread only (via `PresentMailbox` / Display path)
- [x] Replace sync `GpuPresentFn` / `MapViewport::gpu_present_` invoke with **Submit** (`set_gpu_submit` + Display BeginFrame; `GpuPresentFn` runs on Display thread)
- [x] `DrawRequest.shell` / `shell_generation` + skip `attach_shell_raster` memcpy when gen unchanged
- [x] Retarget `MapViewport` frame_request_ / presented pairing to Submit/ack (no UI-thread device present)
- [x] Product glue: `BrowserView` copies `Widget::shell_raster` → `MapViewport::commit_shell_overlay` (app layer; views must not hard-dep gpu)
- [ ] Build full `CompositorFrame` on compositor with HUD-as-quad — **deferred** (HUD still GDI `overlay_paint_` / paint_hud; `commit_shell_overlay` not consumed by Vista `GpuPresentFn` yet)
- [ ] Merge views `ShellCompositor` ↔ `gpu::PresentMailbox` — **deferred** (layering: keep glue in `src/app/views`)
- [ ] Human verify map + shell still present (`build.bat` / `build.bat te` as appropriate)

### P5 — DWM-aligned BeginFrame (after P4)

- [x] BeginFrame **timer stub** on Display / `PresentMailbox` (~16 ms) driving produce when `frame_request_` advances
- [x] BeginFrame source aligned with DWM / display refresh (Windows) — `ui::gfx::VblankClock` (`IDXGIOutput::WaitForVBlank`); Sleep fallback via `set_interval_ms` / 16 ms
- [ ] Drive Activate / next Commit from BeginFrame; stop free-running present loops — **partial** (Display + PresentMailbox paced by vblank; shell Commit still UI `WM_PAINT`)
- [x] Ensure BeginFrame stops first in shutdown (`prepare_close` → `detach` / `stop_display_thread` before `shutdown_compositor`)
- [x] Counters: present → BeginFrame latency visible under PaintCounters (`begin_frame_qpc` / `begin_frame_to_present_qpc` on MapViewport Display path)
- [ ] Human verify: steady frame pacing; no regression vs P4 Submit path

## Shutdown (cross-cutting; land with P2+ and finish in P4/P5)

- [x] Hook `Widget::set_will_close` / `will_close` to ordered stop:
  1. clear `on_shell_published` (no more shell → map Commits)
  2. stop BeginFrame + GPU drain (`BrowserView::prepare_close` → `MapViewport::detach` / `release_rhi_device` / `stop_display_thread`)
  3. stop Commit + drain raster (`Widget::shutdown_compositor`)
  4. `DestroyWindow` on UI thread
- [x] No synchronous GPU destroy on the UI message that still holds device locks incorrectly (Display thread owns Device destroy)
- [ ] Drop unsubmitted PresentMailbox frames explicitly on shutdown — covered by mailbox `drain_for_shutdown` where wired; keep under smoke

## Coordinator / docs

- [x] Product glue in `src/app/views` (`commit_widget_shell_to_maps`) — not inside `//src/ui/views` → gpu
- [ ] Update `src/ui/views` / `src/gpu` README only when behavior lands (HUD-in-frame / PresentMailbox merge)
- [ ] When fully landed: archive this plan + mark spec Status landed; fold as-built into `docs/superpowers/ui-views-skia.md` (or module README)

## Tracks (parallel drafting)

| Track | Phases | Notes |
| --- | --- | --- |
| **A** | P0, P1 | gfx + Widget paint path; no new threads |
| **B** | P2, P3 | compositor + raster; owns pending/active APIs |
| **C** | P4, P5 | GPU thread + BeginFrame; owns Submit / device |
| **D** | Shutdown + MapViewport retarget + shared BUILD + app glue | Coordinator |

Shared files: `widget.*`, `map_viewport.*`, `frame_sink.*`, `display.*`, root `BUILD.gn` fragments — **D / coordinator only** once tracks merge.

## Deferred (explicit)

Absorbed by follow-on plan [`2026-09-30-ui-shell-perf-upgrade.md`](../archive/plans/2026-09-30-ui-shell-perf-upgrade.md) (living §Shell perf upgrade waves). Do not re-open parallel Deferred here.

| Item | Status |
| --- | --- |
| DWM BeginFrame clock | Landed: `ui::gfx::VblankClock` + MapViewport / PresentMailbox |
| HUD-as-quad in submitted frame | → **U3** in ui-shell-perf-upgrade |
| views ↔ gpu PresentMailbox merge | → **U3** (glue stays `src/app/views`; views must not hard-dep gpu) |
| Multi-worker raster | → **U4** (optional) |
| Shell Commit driven by BeginFrame | → **U5** (optional; still UI `WM_PAINT` today) |

Human verify:

```bat
build.bat
build.bat te
```
