<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/base/trace`

mogu-aligned Chrome Trace buffer for flow dump / render profiling.

| API | Header |
| --- | --- |
| `base::trace::Trace` / `ScopedTracer` | `event/trace.h` |
| `process_trace` / `BASE_TRACE_EVENT` | `event/process_trace.h` |
| `SpanRecorder` | `recorder/span_recorder.h` |
| `export_chrome_trace` | `export/chrome_trace.h` |
| `format_trace_frame_log_lines` | `log/frame_log.h` |
| `start_always_on_diagnostics` | `diag/diagnostic_bootstrap.h` |
| `dump_startup_profile` / `maybe_dump_startup_profile` | `diag/startup_profile.h` |

Dump: `{"traceEvents":[...]}` (Perfetto / chrome://tracing).

Startup (SmartGisViews): `BASE_TRACE_EVENT(name, "startup")` spans; after first
show, Debug builds (or `SMT_STARTUP_PROFILE=1`) print a phase table to stderr
and write `out/Debug/log/startup_profile.txt` (+ sibling `.json`) by default.
Override path with `SMT_STARTUP_PROFILE_DUMP=...`. Progressive dumps also fire
before FlyCube attach and after `Browser::init`.

```bat
.\build.bat trace_test
set SMT_STARTUP_PROFILE=1
out\Debug\SmartGIS.exe --ui-showcase=shell
type out\Debug\log\startup_profile.txt
```

See living §Trace in `docs/superpowers/specs/2026-09-14-base-root-hybrid-design.md`
and Views shell §Startup profile.
