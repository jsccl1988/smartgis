<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# GDI internal RHI reshape Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reshape `rhi2d` leftover GDI into industry-shaped Device / Surface / CommandEncoder internals, enrich 2D capability in phases, keep GDI HWND present.

**Architecture:** Spec §GDI internal RHI reshape in [`../specs/2026-09-13-render-rhi-scene-design.md`](../specs/2026-09-13-render-rhi-scene-design.md). Approach A: internal `Gdi*` RHI vocabulary; not `render::rhi`. Phases 1→2→3→4. ABI may break with caller sync.

**Tech Stack:** C++23, Win32 GDI/GDI+, existing `legacy_render` DLL / GN.

## Global Constraints

- Work on **`master`** only; parallel agents use **non-overlapping paths**.
- Scope: **`src/legacy/render/rhi2d/`** (+ callers when ABI breaks).
- Prefix `Gdi*`; never put symbols in `render::rhi`.
- Single GDI play lane; no multi-thread GDI Draw*.
- Comments in English on touched code.
- **Do not** `git commit` unless the user asks.
- Windows forbids path segment `aux/` — keep `gdiaux/`.

## File map (target)

| Path | Role |
| --- | --- |
| `impl/gdi/core/surface/*` | `GdiSurface` / `GdiSurfacePool` / `GdiOwnedSurface` / compose |
| `impl/gdi/core/encode/*` | `GdiCommandEncoder` / `GdiCommandBuffer` |
| `impl/gdi/core/paint/*` | Paint player (Draw* execution) |
| `impl/gdi/core/host/*` | Device facade + UI helpers |
| `impl/gdi/core/worker/*` | FrameJob lane |
| `public/device/*` | Call-facing facade (may rename/split) |

---

### Task 1: Phase 1 — Resource / Surface (paths: `core/surface/` + compose tests)

- [x] Rename `SmtSurfacePool` → `GdiSurfacePool` (update `gdi_surface_pool()` return type + all refs).
- [x] Extend `GdiSurface` with `generation` + dirty rect helpers (`mark_dirty` / `clear_dirty` / `dirty` union).
- [x] `GdiOwnedSurface::clear` / paint paths bump generation and dirty when bits change.
- [x] Unit coverage in `gdi_compose_test` (or new `surface_pool_test`) for pool reuse + dirty.
- [x] `build.bat debug gdi_compose_test` green; run the test exe.

### Task 2: Phase 2 — CommandEncoder scaffold (paths: `core/encode/` **only** + BUILD add)

- [x] Add `core/encode/command_buffer.h` — typed ops enum + POD args (clear, fill_rect, blit, set_style stub).
- [x] Add `core/encode/command_encoder.h/.cc` — `begin_pass(GdiSurface*)`, record ops, `end_pass()`, `take_buffer()`, `replay()` via simple GDI player (FillRect/BitBlt only for v1).
- [x] Add `test/encode_test.cc` + GN `gdi_encode_test` (static like compose_test).
- [x] Do **not** edit `surface_pool.*` or `host/render_device.*` in this task.
- [x] `build.bat debug gdi_encode_test` green.

### Task 3: Wire BeginRender/Draw* → encoder (host + paint) — after Task 1+2

- [x] Facade records into encoder on BeginRender/EndRender (MAP/DYNAMIC/QUICK); DIRECT unchanged; `last_pass()` for tests. Draw* record deferred to Task 4; preview still windowport-only (comment notes future replay).
- [x] Sync callers if signatures change. (none — ABI unchanged)
- [x] `gdi_map_paint_test` green (fix: keep encoder members **after** `render_thread_` / mutexes so `device_->render_thread_` offsets stay valid).

### Task 4: Phase 3 — richer Draw ops on encoder

- [x] Path/stroke/fill, text, tile blit, clip/blend ops as recorded commands. (v1: stroke_rect / clip_rect / reset_clip / blit; text/path/blend deferred)
- [x] Tests for replay pixel stability on a few ops.

### Task 5: Phase 4 — schedule / trace

- [x] Align FrameJob with encoder take/replay; extend `gdi.*` trace spans around encode/play.
  - Host `BeginRender`/`EndRender`: `gdi.encode` spans `begin_pass` / `end_pass` / `take_buffer`; counter `last_pass_ops`.
  - Worker `paint_once`: keep `gdi.frame`/`RenderMap`; nested `encode_idle` (no `last_pass_` replay — would double-draw). Replay remains host-side until Task 4 preview.
  - `PreviewZoom*`: windowport-only; comments name `last_pass_` for future replay.
- [x] README + living § acceptance checkboxes (Task 5 evidence). Living § → **accepted** after Task 4 richer Draw ops landed.

## Done when

- [x] Phases 1–4 landed; GDI tests green (`compose` / `encode` / `map_paint`).
- [x] Living § **accepted** (Task 4 stroke/clip/blit + Task 5 `gdi.encode` traces). Text/path/blend ops remain deferred non-blockers.
- [x] Task 5 verify: `gdi_compose_test` + `gdi_map_paint_test` green after rebuild (host `gdi.encode` / worker `encode_idle`).
