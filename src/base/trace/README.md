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

Dump: `{"traceEvents":[...]}` (Perfetto / chrome://tracing).

```bat
.\build.bat trace_test
```

See living §Trace in `docs/superpowers/specs/2026-09-14-base-root-hybrid-design.md`.
