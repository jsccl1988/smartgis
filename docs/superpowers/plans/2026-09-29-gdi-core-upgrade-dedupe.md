<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# GDI core upgrade + dedupe (B then A2) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Upgrade `impl/gdi/core` internals: first structural cleanup (B), then canvas-as-recorder so map Draw* record into `Rhi2dCommandEncoder` and a single `replay` paints (A2).

**Architecture:** Spec §GDI core upgrade + dedupe in [`../specs/2026-09-13-render-rhi-scene-design.md`](../specs/2026-09-13-render-rhi-scene-design.md). Approach: `GdiPaintCanvas` keeps Draw* API; TLS encoder bind (`paint/carto_draw/encoder_tls.h`); leaves push typed ops + blob; host `EndRender` and worker FrameJob `take`+`replay`. No `render::rhi` symbols.

**Tech Stack:** C++23, Win32 GDI/GDI+, existing `legacy_render` / GN.

## Global Constraints

- Work on **`master`** only; parallel agents use **non-overlapping paths**.
- Keep `SmtRhi2dRenderDevice` / `SmtGdiRenderThread` / CreateDevice string + `RenderDevice2d` virtuals.
- Do **not** grow `GdiPaintCanvas` with members that shift `SmtRhi2dRenderDevice::render_thread_` offsets.
- Single GDI play lane; comments English; new helpers `snake_case`.
- **Do not** `git commit` unless the user asks.
- Windows forbids path segment `aux/` — keep `gdiaux/`.

## File map

| Path | Role |
| --- | --- |
| `core/encode/*` | Blob-backed buffer + polyline/polygon/text/pen ops + replay |
| `paint/carto_draw/encoder_tls.h` | TLS encoder bind (layout-safe) |
| `core/paint/paint_canvas.*` | `set_encoder`; Draw* record-or-immediate |
| `core/host/render_device.*` | EndRender: take + replay; no empty encode shell |
| `core/worker/render_thread.*` / `layer_painter.*` | FrameJob encode session + replay |
| `BUILD.gn` / `README.md` | `.cc` unify + layout |

---

### Task 1: Phase B — unify `.cc` + trim

- [x] Rename `core/**/*.cpp` → `.cc`; update `BUILD.gn`.
- [x] Encoder TLS extracted (`paint/carto_draw/encoder_tls.h`) so canvas size does not shift `render_thread_`.
- [x] Drop noop `encode_idle`; README note.
- [x] GDI tests green.

### Task 2: Phase A — encoder blob + richer ops

- [x] `Rhi2dCommandBuffer` blob + `append_blob`.
- [x] Ops: `kSetPen` / `kSetBrush` / `kPolyline` / `kPolyPolygon` / `kEllipse` / `kText` (+ replay).
- [x] `gdi_encode_test` polyline smoke.

### Task 3: Phase A — canvas-as-recorder

- [x] `set_encoder` / `is_recording()` via TLS.
- [x] Device leaves + style prep record when encoding.
- [x] Road casing: dual pen polylines while encoding.

### Task 4: Phase A — wire host + worker replay

- [x] Host begin/end: record-only + replay onto surface.
- [x] `render_map` encode session + replay.
- [x] Worker uses painter encode path.
- [x] `gdi_map_paint_test` + `gdi_encode_test` + `gdi_compose_test` green.

### Task 5: Docs

- [x] Living § accepted; README; Active table plan link.

## Done when

- [x] Phase B + A2 landed; GDI tests green; ABI names unchanged.
