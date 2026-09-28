<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/base/trace`

mogu-aligned Chrome Trace buffer for flow dump / render profiling.

| API | Header |
| --- | --- |
| `base::Trace` / `ScopedTracer` | `trace.h` |
| `process_trace` / `BASE_TRACE_EVENT` / `SMT_TRACE` | `process_trace.h` |
| `SpanRecorder` | `span_recorder.h` |
| `export_chrome_trace` | `chrome_trace.h` |

Dump: `{"traceEvents":[...]}` (Perfetto / chrome://tracing).

```bat
.\build.bat trace_test
```

See living §Trace in `docs/superpowers/specs/2026-09-14-base-root-hybrid-design.md`.
