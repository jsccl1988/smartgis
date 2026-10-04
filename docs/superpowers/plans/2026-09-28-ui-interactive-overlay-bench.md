<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# UI Interactive Harness + Overlay Bench Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship L1 `views_interactive_tests`, perf `views_bench`, C++ harness under `src/ui/views/testing/harness/`, DebugAgent `ui.*` RPC, Python smoke, optional OpenCppCoverage script, and as-built updates — per [`../specs/2026-09-27-views-desktop-shell-design.md`](../specs/2026-09-27-views-desktop-shell-design.md) §UI interactive harness + overlay bench.

**Architecture:** In-process `EventGenerator` + `ViewsTestBase` + `OverlayScene` drive synthetic input and shell-only overlay assertions (Wave1 compositor / `PainterRegistry` / `PaintCommit`; Wave2 `MapViewport` / `AuxOverlay` semantic only). Live `SmartGIS.exe` runs reuse the same semantics through loopback DebugAgent `ui.*` methods orchestrated by `tools/debug/scripts/ui_smoke.py`.

**Tech Stack:** C++23, GN/Ninja (`out/Debug`, `out/Release`), Views + Skia, existing `DebugAgent` NDJSON TCP, system Python 3, optional OpenCppCoverage on Windows.

## Global Constraints

- Work on **`master`** only; parallel agents must use **non-overlapping paths**.
- **No Qt**; desktop UI stays **Views + Skia**.
- New-tree functions **`snake_case`**; public namespace **`ui::views`** (internals in `ui::views::detail` or anonymous namespace in `.cc`).
- Copyright year **2026**; English comments in new/changed source.
- Build via **`build.bat`** / `ninja -C out/Debug`; gen roots **`out/Debug`** and **`out/Release`** only.
- **OpenCppCoverage** is optional CI; **must not** block default **`build.bat te`**.
- Do not add map pixels to L2 overlay or default pixel goldens.

## File map

| Path | Role |
| --- | --- |
| `src/ui/views/testing/harness/event_generator.{h,cc}` | Synthetic mouse/keyboard + message pump helpers |
| `src/ui/views/testing/harness/views_test_base.{h,cc}` | Fixture: root View, `click`, `wait`, `find_view_by_id` |
| `src/ui/views/testing/harness/overlay_scene.{h,cc}` | Shell-only overlay layers for Wave1 paint assertions |
| `src/ui/views/testing/interactive/views_interactive_tests.cc` | L1 scenario entry + matrix cases |
| `src/ui/views/testing/interactive/interactive_matrix.md` | Behavioral matrix (control × action × assertion) |
| `src/ui/views/testing/bench/views_bench.cc` | Perf microbench main |
| `src/ui/views/BUILD.gn` | `views_interactive_tests`, `views_bench`, harness sources |
| `testing/test.gni` or root `BUILD.gn` | Wire L1 into `//:test_all` when first scenario is green |
| `src/content/browser/debug/debug_agent.{h,cc}` | `DebugAgentHost` `ui_*` hooks + `ui.*` dispatch / `:ui` lines |
| `tools/debug/scripts/ui_smoke.py` | Agent discovery + smoke sequence |
| `testing/scripts/open_cpp_coverage_views.ps1` | OpenCppCoverage wrapper for views PEs |
| `docs/superpowers/ui-testing.md` | L1/P2 as-built |
| `testing/README.md` | GN run instructions |
| `src/ui/views/README.md` | Harness + targets summary |

**Parallelism:** Task 1 (harness) blocks Task 2 (matrix exe); Task 3 (bench) ∥ Task 2 after Task 1; Task 4 (Agent RPC) ∥ Task 2; Task 5 (Python) after Task 4; Task 6 (coverage script) ∥ Task 5; Task 7 (docs) after Task 2 lands at least one green test.

---

### Task 1: Harness core (`EventGenerator`, `ViewsTestBase`, `OverlayScene`)

**Files:**
- Create: `src/ui/views/testing/harness/event_generator.h`, `event_generator.cc`
- Create: `src/ui/views/testing/harness/views_test_base.h`, `views_test_base.cc`
- Create: `src/ui/views/testing/harness/overlay_scene.h`, `overlay_scene.cc`
- Modify: `src/ui/views/BUILD.gn` (add `source_set("views_test_harness")` or list sources on interactive target)
- Test: compile harness via `ninja -C out/Debug views_interactive_tests` (after Task 2 adds test binary)

**Interfaces:**
- Produces:
  - `class EventGenerator` with `move_to` / `press` / `release` / `click` / `drag` / `key_press` / `type_char` / `type_utf8`
  - `make_test_widget` / `find_child_at` helpers (`ViewsTestBase` style)
  - `class OverlayScene` with `set_layer_count` / `measure_commit_ns`

- [x] **Step 1:** Add `event_generator.h` / `.cc` with copyright 2026 and `ui::views` namespace; synthetic events match existing kernel event types.
- [x] **Step 2:** Add `views_test_base.h` / `.cc` — headless root consistent with `views_unittests` patterns.
- [x] **Step 3:** Add `overlay_scene.h` / `.cc` — Wave1 shell-only overlay hooks (no `MapViewport` bitmap reads).
- [x] **Step 4:** Register `source_set("views_test_harness")` in `src/ui/views/BUILD.gn`.
- [x] **Step 5:** `build.bat debug views_interactive_tests` — link + run OK.

---

### Task 2: Interactive matrix + `views_interactive_tests`

**Files:**
- Create: `src/ui/views/testing/interactive/views_interactive_tests.cc`
- Create: `src/ui/views/testing/interactive/interactive_matrix.md`
- Modify: `src/ui/views/BUILD.gn` (`test("views_interactive_tests")`)
- Modify: `BUILD.gn` or `testing/test.gni` (add to `//:test_all` when first case passes)
- Modify: `testing/README.md` (run line for `out/Debug/views_interactive_tests.exe`)
- Test: `build.bat debug te` includes new exe once wired

**Interfaces:**
- Consumes: Task 1 `ViewsTestBase`, `EventGenerator`, `OverlayScene`
- Produces: `views_interactive_tests.exe` exit 0 on green matrix subset

- [ ] **Step 1:** Author `interactive_matrix.md` with Wave1 rows (MenuBar, TabStrip, StatusBar, theme invalidate) and Wave2 placeholders (semantic only).
- [x] **Step 2:** Implement first green scenarios in `views_interactive_tests.cc` (Button/Checkbox/Textfield/Tab + multi-step button→label).
- [x] **Step 3:** Add GN `test("views_interactive_tests")` with `output_name = "views_interactive_tests"`.
- [x] **Step 4:** Hook into `//:test_shell` (and `test_all` if that group includes `test_shell` deps); document in `ui-testing.md` / views README.
- [x] **Step 5:** Run `out/Debug/views_interactive_tests.exe` — exit 0 (`views_interactive_tests: OK`).

---

### Task 3: `views_bench` perf target

**Files:**
- Create: `src/ui/views/testing/bench/views_bench.cc`
- Modify: `src/ui/views/BUILD.gn` (`executable("views_bench")` or `test` with no assert)
- Test: `out/Debug/views_bench.exe` prints timing lines to stdout

**Interfaces:**
- Consumes: compositor / `PainterRegistry` / `PaintCommit` paths from `:views`
- Produces: `views_bench.exe` stable CSV-style lines for CI log capture

- [x] **Step 1:** Add `views_bench.cc` microbench loop (EventGenerator click + OverlayScene commit + ShellCompositor).
- [x] **Step 2:** GN `benchmark("views_bench")`, wired to `//:benchmark_all`.
- [x] **Step 3:** Document run in `docs/superpowers/ui-testing.md` (not part of default `te`).
- [x] **Step 4:** Run `out/Debug/views_bench.exe` — exit 0 with ns/op lines.

---

### Task 4: DebugAgent `ui.*` RPC

**Files:**
- Modify: `src/content/browser/debug/debug_agent.h`, `debug_agent.cc`
- Modify: `src/app/views/shell/ui/panels/shell_panels.cc` (bind hooks)
- Test: `src/content/browser/debug/debug_agent_test.cc`

**Interfaces:**
- Produces:
  - `ui.find` / `ui.click` / `ui.type` / `ui.dump_tree` / `ui.overlay_stats`
  - Console `:ui find|click|type|tree|overlay`
  - Unbound hooks → `ui host not bound`

- [x] **Step 1:** Extend `DebugAgentHost` with optional `ui_*` std::function hooks (no `ui_commands` TU).
- [x] **Step 2:** Wire `dispatch_method` + `exec_line` / `:help`.
- [x] **Step 3:** Unit tests for unbound + bound hooks; shell Wave1 click/type/dump_tree.
- [x] **Step 4:** `build.bat debug debug_agent_test` — PASS.

**Gaps (next slice):** markup id find; `ui.overlay_stats` still `"unavailable"` until shell exposes OverlayScene metrics.

---

### Task 5: Python smoke script

**Files:**
- Create: `tools/debug/scripts/ui_smoke.py`
- Test: manual — `SmartGIS.exe --debug-console` + `python tools/debug/scripts/ui_smoke.py`

- [x] **Step 1:** Read `%TEMP%/smartgis-debug.json`, connect, `ping` + `ui.dump_tree` + `ui.overlay_stats`.
- [ ] **Step 2:** Optional `--scenario shell_tab` Wave1 sequence.
- [ ] **Step 3:** Document in `tools/debug/README.md` or `testing/README.md`.
- [ ] **Step 4:** Live smoke against Views with agent enabled — exit 0 when host bound.

---

### Task 6: OpenCppCoverage script (optional CI)

**Files:**
- Create: `testing/scripts/open_cpp_coverage_views.ps1`
- Modify: `testing/README.md` (optional CI section)

**Interfaces:**
- Consumes: built `out/Release/views_interactive_tests.exe`, `out/Release/views_unittests.exe`
- Produces: Cobertura/XML under `out/coverage/views/`

- [ ] **Step 1:** Script runs OpenCppCoverage with explicit module filter `ui_views.dll`, `SmartGIS.exe` as needed.
- [ ] **Step 2:** Document that default `build.bat te` does **not** invoke this script.
- [ ] **Step 3:** Dry-run script path check (skip if OpenCppCoverage not installed — exit 0 with message).

---

### Task 7: Docs as-built

**Files:**
- Modify: `docs/superpowers/ui-testing.md` (L1 landed/in-progress, bench row, 最后更新 2026-09-28)
- Modify: `src/ui/views/README.md` (harness + targets)
- Modify: `docs/superpowers/specs/2026-09-27-views-desktop-shell-design.md` (check Wave checkboxes when landing)

- [x] **Step 1:** Update `ui-testing.md` L1 row to `views_interactive_tests` with path and `build.bat te` policy.
- [x] **Step 2:** Add **bench** row for `views_bench.exe` (manual / optional CI perf).
- [x] **Step 3:** Link harness directory in `src/ui/views/README.md`.
- [x] **Step 4:** Mark spec § Wave1 checklist items for landed harness / interactive / bench / `ui.*`.

---

## Spec coverage (self-check)

| Spec locked item | Task |
| --- | --- |
| Dual layer C++ + Agent `ui.*` | 1, 4, 5 |
| Wave1 compositor / PaintCommit overlay | 1, 2, 3 |
| Wave2 MapViewport / AuxOverlay semantic | 2, 4 |
| Behavioral matrix required | 2 |
| OpenCppCoverage optional | 6 |
| `views_interactive_tests` / `views_bench` | 2, 3 |
| Harness under `testing/harness/` | 1 |
| Align `ui-testing.md` P2/L1 | 7 |
| No Qt; snake_case; `ui::views`; master | Global Constraints |
