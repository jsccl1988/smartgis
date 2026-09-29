<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# GDI leftover profile (base::trace + MFC log) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Instrument GDI leftover paint with `base::trace::process_trace`, stream per-frame text logs in Legacy SmartGis, and filter GDI in Views RenderTrace.

**Architecture:** Hot path uses `BASE_TRACE_EVENT` / `process_trace_add` only when tracing is armed. Geom types accumulate µs per layer then flush one span each. MFC dock polls `for_each_process_trace_event` and formats lines via shared `format_trace_frame_log_lines`.

**Tech Stack:** `src/base/trace`, GDI leftover thread (`map_painter` / `frame_scheduler` / `compose`), MFC `CBCGPDockingControlBar`, Views `RenderTracePanel`.

**Spec:** [`../specs/2026-09-13-render-rhi-scene-design.md`](../specs/2026-09-13-render-rhi-scene-design.md) §GDI leftover profile

## Global Constraints

- Categories: `gdi.frame` / `gdi.layer` / `gdi.geom` only for this slice.
- No per-feature spans; no second profiler stack; no Views HWND inside MFC.
- New comments in English; functions `snake_case` on new symbols.
- Stay on `master`.

## Files

| Path | Role |
| --- | --- |
| `src/base/trace/log/frame_log.h` | Header-only frame log formatter |
| `src/base/trace/trace_test.cc` | Formatter unit coverage |
| `src/base/trace/BUILD.gn` | `log/frame_log.h` |
| `…/gdi/thread/{frame_scheduler,map_painter,render_thread}.*` | Frame/layer/geom spans |
| `…/gdi/buffer/compose.cc` | `compose` span |
| `src/legacy/app/shell/dock/render_trace.*` | MFC thin panel |
| `src/legacy/app/shell/frame/main.*` + `resource.h` | Dock wire |
| `src/ui/gis/debug/render_trace_panel.*` | GDI filter checkbox |

---

### Task 1: `format_trace_frame_log_lines` + test

- [x] Add `base/trace/log/frame_log.h` — filter by cat prefix, group under each `RenderMap` complete span, emit one text line per frame.
- [x] Extend `trace_test` with synthetic events → expected line substrings.
- [x] `.\build.bat debug trace_test` green.

### Task 2: GDI instrumentation

- [x] `frame_scheduler`: `submit` / `cancel` / `paint_loop` under `gdi.frame`.
- [x] `paint_once` / `render_map`: `RenderMap`.
- [x] `render_layer*`: `gdi.layer` + name; geom µs accum → flush `gdi.geom` spans.
- [x] `submit_compositor_frame`: `compose` when sink active.
- [x] Link already has `//src/base:base`.

### Task 3: Legacy MFC RenderTrace dock

- [x] `RenderTraceDockBar`: Record/Stop/Clear/Refresh + `CListBox`; timer appends new frame lines while armed.
- [x] Host under AMBox dock tab `"RenderTrace"`.
- [x] Wire `base::trace::set_tracing_enabled` / `base::trace::process_trace().clear()`.

### Task 4: Views GDI filter

- [x] `show_gdi_` checkbox; `cat_is_gdi`; include in `visible_events`.
- [x] Title/status copy mentions GDI.

### Task 5: Verify

```bat
.\build.bat debug trace_test
.\build.bat debug SmartGis
.\build.bat debug SmartGisViews
```

- [x] Build: `trace_test` / `legacy_app` / `SmartGisViews` green.
- [ ] Manual: Record → pan map → log lines appear; Stop freezes stream.
