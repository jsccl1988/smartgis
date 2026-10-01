<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Harness capability runtime — implementation plan

> **For agentic workers:** implement task-by-task; keep `master`; do not open a new dated design twin (living § in Views shell umbrella).

**Goal:** Sink shared harness verbs into `content::CapabilityHost`, run Interact DSL through `shell/runtime`, thin-wrap DebugAgent `script.run`, and migrate suite bodies to suite-colocated `*.il`.

**Architecture:** Host callback bag in content (no `Browser` dep) ← filled by shell/runtime ← DSL exec + chrome verbs ← harness registry / DebugAgent.

**Tech stack:** C++23, existing Interact.g4 RD mirror, suite JSON + `loop_runner.py`.

## Global Constraints

- Work on `master`; no Qt; no content→app deps.
- Gen roots `out/Debug` / `out/Release`; verify with `build.bat debug src/app/views:views`.
- Snake_case for new functions; English comments.

## Tasks

### Task 1: CapabilityHost + GN

- [x] Add `src/content/browser/capability/host.h` (+ optional `verbs.cc` helpers).
- [x] GN `source_set("capability")`; deps from `debug_agent` / views as needed.

### Task 2: shell/runtime

- [x] `fill_host(Browser&, CapabilityHost*, mark_leaf)`.
- [x] `run_script(Browser&, path)` / `try_run_suite_script(id)`.
- [x] Point interact path through runtime; DSL uses Host for pump/mark/wait/load/detach/tabs/input.

### Task 3: DebugAgent thin wrap

- [x] `DebugAgentHost::script_run`.
- [x] RPC `script.run` + `:script <path>`.
- [x] Bind in `BrowserView::bind_debug_agent_host`.

### Task 4: Suite migration (incremental)

- [x] Keep C++ fallback until script produces required marks/BMP.
- [x] `input.il` + digitize verbs (`tool` / `expect_tool` / `expect_geom`); suite green.
- [x] `browse.il` + lean `--browse-showcase` (`browse_stress` / `expect_wheel_cursor`); suite green.
- [x] `map2d.*.il` + Host `map2d_run`; C++ body fallback.
- [x] `atmosphere.*.il` + Host `atmosphere_run`; C++ body fallback.
- [x] `console.il` + Host `console_run`; C++ body fallback.
- [x] Update `ui-testing.md` when Wave 1 green.

### Task 5: Cleanup

- [x] Relocate DSL under `runtime/dsl/`; showcase keeps thin `interact_script` adapter.
- [x] harness/common stays Host fillers' helpers (pump/mark/maps/sample); no second wrapper layer.
