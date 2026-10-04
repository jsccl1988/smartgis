<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Plan: base::trace + Views RenderTrace panel (P0–P3)

**Status:** landed (archived 2026-10-03 — checkboxes complete)

**Status:** landed  
**Specs:** `2026-09-14-base-root-hybrid-design.md` §Trace; `2026-09-27-map2d-frame-design.md` §Perf; `2026-09-27-views-desktop-shell-design.md` §RenderTrace; RHI living short cross-ref  
**Locked:** dump `{"traceEvents":[...]}`; panel = inspector tab `RenderTrace`; P0–P3 parallel; no further confirm

## Files

| Path | Role |
| --- | --- |
| `src/base/trace/{event,recorder,export,log,diag,detail}/…` | Foundation tracer (`base::trace::`; see module README) |
| `src/base/trace/trace_test.cc` + BUILD | Unit tests |
| `src/base/BUILD.gn` / README | foundation deps |
| `content/.../map2d/{frame_cache,gpu,software}` + scene3d present | P1 spans |
| `ui/gis/debug/render_trace_panel.*` | P2 timeline UI |
| `app/views/shell/ui/{browser_view,main}` | Inspector tab + `SMT_TRACE` |

## Tasks

- [x] P0 Trace + ScopedTracer + process_trace enable/env
- [x] P3 span_recorder + export_chrome_trace (same tree)
- [x] P1 map2d/scene3d instrumentation
- [x] P2 RenderTracePanel + shell wire
- [x] `build.bat` foundation test + SmartGisViews link

## Verify

```bat
.\build.bat trace_test
.\out\trace_test.exe
.\build.bat SmartGisViews
```
