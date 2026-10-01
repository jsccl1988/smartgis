<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# GDI leftover worker (dual-track) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Keep leftover GDI 2D present usable during dual-track by turning `SmtGdiRenderThread` into a thin **FrameJob** shell (B→A→C). No multi-core speedup.

**Architecture:** Spec §GDI leftover worker in [`../specs/2026-09-13-render-rhi-scene-design.md`](../specs/2026-09-13-render-rhi-scene-design.md). Do **not** wire into `gpu/` compositor until leftover present is deleted.

**Tech Stack:** C++23, existing `legacy/render/rhi2d/impl/gdi`, Win32 GDI, `std::thread` / mutex / cv.

## Global Constraints

- Work on **`master`** only; parallel agents use **non-overlapping paths**.
- **Do not** expand Draw* API on the worker class.
- **Do not** `join()` the worker from the HWND thread.
- Comments in English on touched code.
- **Do not** `git commit` unless the user asks.

## File map

| Path | Role |
| --- | --- |
| `thread/render_thread.h` `.cpp` | Thin `SmtGdiRenderThread` façade (Phase C API) |
| `thread/frame_scheduler.h` `.cpp` | `Rhi2dFrameScheduler` — executor / cancel / gen / coalesce |
| `thread/map_painter.h` `.cpp` | `GdiMapPainter` — map/layer/feature/geometry |
| `thread/style_canvas.h` `.cpp` | `GdiStyleCanvas` — style + Draw* |
| `device/render_device.h` `.cpp` | Host stage/submit/present; calls Phase C API only |

---

### Task 2.5: host FrameJobs on `base/execution`

- [x] **Step 1:** Replace resident `std::thread` + cv wait with dedicated `NThreadPoolExecutor(1)`.
- [x] **Step 2:** `resume()` posts at most one `paint_loop` task; pending coalesces in-task.
- [x] **Step 3:** HWND `stop()` still must not `join` a live GDI job — leak executor if needed.

### Task 1 (Phase B): cancel + bounded wait + generation

**Files:** `gdi_renderthread.h` / `.cpp`, `gdi_renderdevice.cpp`

- [x] **Step 1:** Add `request_cancel()`, `wait_idle(timeout_ms)`, `job_generation()`, bump generation on `resume` / context submit.
- [x] **Step 2:** Replace device `while (IsRendering()) Sleep(0)` with cancel + bounded wait.
- [x] **Step 3:** `RenderMap` layer loop honors cancel + stop; skip front publish if generation mismatch.
- [ ] **Step 4:** Verify close / `--self-test` does not hang (manual or `build.bat debug` target that exercises GDI present).

---

### Task 2 (Phase B): front/back publish contract

**Files:** `gdi_renderthread.cpp`, `gdi_renderdevice.cpp`

- [x] **Step 1:** Document and enforce: private back only during paint; publish to shared front only at end if gen matches.
- [x] **Step 2:** Device skips `ClearBuf` on map front while worker busy / publishing.
- [ ] **Step 3:** Smoke pan/zoom — no concurrent GDI hang on shared HBITMAP.

---

### Task 3 (Phase A): single submit + present-on-gen

**Files:** `gdi_renderdevice.cpp` (`Timer`, `ScheduleDelayedRedraw`, `ReRenderMap*`)

- [x] **Step 1:** Stage pending job from all zoom/redraw entry points; only Timer (or urgent idle submit) resumes.
- [x] **Step 2:** Present / Invalidate only when front generation advances.
- [x] **Step 3:** Route realtime through cancel → urgent job when possible (UI paint fallback).

---

### Task 4 (Phase C): shrink surface + thread composition

- [x] **Step 1:** Add `Rhi2dFrameScheduler` / `GdiMapPainter` / `GdiStyleCanvas` under `thread/` (`render::detail`); move bodies out of `render_thread*` / delete `render_thread_draw.cpp`.
- [x] **Step 2:** Shrink `SmtGdiRenderThread` public API to Phase C snake_case; **sync** `render_device.cpp` call sites (no dual names).
- [x] **Step 3:** Update `impl/gdi/BUILD.gn`; `build.bat debug` `legacy_render` / `gdi_map_paint_test` green.
- [ ] **Step 4:** When Views map drops GDI leftover bind, delete `gdi/thread` and archive this plan + umbrella §.

---

## Done when

- Phase B: no unbounded UI spin-wait; cancel/gen gate publish; stop path still non-join.
- Phase A: interactive debounce without present flood.
- Phase C: thin façade + three collaborators; device on Phase C API; GDI tests build.
