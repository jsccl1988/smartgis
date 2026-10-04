<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Plan: `src/base/trace` Chromium-style subdirectory cutover

**Status:** landed (archived 2026-10-03 — checkboxes complete)

**Status:** landed  
**Spec:** `2026-09-14-base-root-hybrid-design.md` §Trace  
**Locked:** break includes (no shims); symbols → `base::trace`; layout `event/` `recorder/` `export/` `log/` `diag/` `detail/`

## Tasks

- [x] Move headers/sources into subdirs; rename `trace_log.h` → `log/frame_log.h`
- [x] Nest former `base::` Trace / process / diag / frame_log into `base::trace`
- [x] Update `BUILD.gn` + module README + living §Trace
- [x] Update all call-site includes + `base::` → `base::trace::` (except macros)
- [x] `build.bat debug trace_test` green; key call-site TUs recompiled

## Verify

```bat
.\build.bat debug trace_test
.\out\Debug\trace_test.exe
```
