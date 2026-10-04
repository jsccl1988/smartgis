<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# GIS Python spatial analysis + dual-runtime Implementation Plan

**Status:** landed (archived 2026-10-03 — checkboxes complete)

> **For agentic workers:** Implement task-by-task. Stay on **master**. Do **not** `git commit` unless the user asks.  
> **REQUIRED approach:** Dual-runtime **A** — embed + worker **in parallel** (plugin-host §Python dual-runtime).

**Goal:** Console runs real CPython with `smartgis.gis.*` + `smartgis.debug`; plugins contribute analysis docks/dialogs/processing; SpatialAnalysisPanel and Console share OpsRunner; worker remains for DAP/heavy; ceiling for flood/path via processing stubs → future OpsRunner.

**Architecture:** In-process `plugin::PythonRuntime` (default Console + `kind=python`); OOP worker for DAP/LSP/isolation. Shared `smartgis` API surface. `gis.analysis` → PluginHost processing + MapScene bridge. `debug.*` → `base::trace::process_trace`.

**Tech Stack:** CPython 3.12 embed, C API bindings, OGR/GEOS via `ops_runner`, Views Diagnostic Console, `tools/debug` worker stubs.

## Global Constraints

- Work on `master` only; no feature branch.
- No Qt / PyQt; no second GEOS/PROJ.
- New functions `snake_case`; namespaces ≤ two public layers.
- Copyright 2026; English comments.
- Gen roots `out/Debug` | `out/Release` via `build.bat`.
- End-state: full `src/gis` mirror under `smartgis.gis`; phase-1 only analysis + active map.

## Path ownership

**MAY edit:**
- `src/plugin/runtime/python/**`
- `src/content/browser/debug/debug_agent.*`
- `src/app/views/shell/browser/plugin/plugin_shell.*`
- `src/app/views/shell/ui/panels/shell_panels.cc`
- `src/app/views/BUILD.gn`
- Living specs + this plan; `docs/superpowers/README.md` Active row if needed

**MUST NOT edit:** `third_party/` CPython sources; Qt; parallel second OpsRunner.

## Acceptance (phase 1 + dual-runtime vertical)

1. `PythonRuntime::eval` runs true Python; `import smartgis.gis.analysis` works when `SMT_HAS_PYTHON`.
2. Console (no `:py` prefix) prefers in-process eval via `DebugAgentHost.py_eval`.
3. `analysis.buffer(distance=…)` (and siblings) write-back features; returns `{ok, text, feature_count}`.
4. SpatialAnalysisPanel status/history shows result text (feature count / ok).
5. `plugin_python_test` still green (skip OK without Python).
6. Python `Host.contribute_dialog` / `contribute_dock` / `contribute_processing` work; sample analysis plugin loads.
7. `smartgis.debug.trace_event` records into `base::trace::process_trace` when tracing enabled.
8. Worker `.pyi` documents shared surface; OOP spawn still works as fallback.
9. `smartgis.gis.scene` / `gis.style` / `ui.config` read/write via shell bridge + ThemeService.

## Tasks

- [x] Task 1: `PythonRuntime::eval` + `GisConsoleBridge` + `smartgis.gis.analysis` bindings
- [x] Task 2: `DebugAgentHost.py_eval`; OOP spawn remains fallback
- [x] Task 3: `PluginShell` owns `PythonRuntime`; shell wires bridge + `py_eval`
- [x] Task 4: Panel result text enrichment; BUILD data_deps for `python312.dll`
- [x] Task 5: Extend `plugin_python_test` for `eval` + analysis smoke; living §§
- [x] Task 6: Python `contribute_dock` / `contribute_dialog` / `contribute_processing` on `smartgis.Host`
  - Files: `src/plugin/runtime/python/bindings.cc`, `python_test.cc`, `tools/debug/smartgis/**/*.pyi`, sample hello optional
  - API: `contribute_dialog(plugin_id, id, title, fn)`, `contribute_dock(plugin_id, id, title, area, fn)`, `contribute_processing(plugin_id, id, title, fn)` where dialog/dock `fn()` opens UI; processing `fn(args_json)->bool` must not touch Views
  - Also bind `Host.open_dialog(id)` and `Host.run_processing(id, args_json="")` for orchestration
  - Test: temp plugin contributes dialog+processing; `open_dialog` increments; `run_processing` returns True from Python factory
- [x] Task 6b (P0+): `smartgis.ui.pick_open_file` / `pick_save_file` / `show_message_box`
  - Wrap `ui::views::pick_*` / `show_message_box`; return `{accepted, path}` dict; suppress-friendly in tests via existing C++ test hooks when needed
  - Update `tools/debug/smartgis/ui/__init__.pyi`
- [x] Task 7: `smartgis.debug` (`set_tracing` / `trace_event` / `tracing_enabled`)
- [x] Task 8: Sample `kind=python` analysis plugin (dialog + processing stub for flood/path ceiling)
- [x] Task 9: `tools/debug` stubs for `gis` / `debug` shared surface (worker track) — scene/style/config `.pyi`
- [x] Task 10: `smartgis.gis.scene` + `gis.style` + extend `GisConsoleBridge`; shell wire
- [x] Task 11: `smartgis.ui.config` ThemeService bind

## Spec coverage

| Living umbrella | § |
| --- | --- |
| views-desktop-shell | §GIS Python Console + analysis results |
| plugin-host | §smartgis.gis + §Python dual-runtime + **§product–Python division** (L1/L2/L3; P0=Task 6) |
| algorithm-layer-oss | §Python-facing analysis stays on OpsRunner |

## Follow-on (product–Python division P0–P4)

Tracked on living [`../../specs/2026-09-13-plugin-host-design.md`](../../specs/2026-09-13-plugin-host-design.md) §product–Python division. Do not open a parallel plan unless that § phases need a dedicated checklist.

- [x] P0+: `smartgis.ui` file_picker / message_box (with Task 6)
- [x] P1: dem/orthogrid kernels only via processing; Python sample orchestration — `samples/product_orchestrate` (L3; builtin UI remains reference)
- [x] P2: `smartgis.tool.activate` + baogrid digitize bridge — `GisConsoleBridge::activate_tool` wired to `MapContents::ActivateTool`
- [x] P3: scene-device primitives + model3d thin bind — `Model3dSceneWriter` + `model3d.*` processing ids
- [x] P4: builtin retreats to L1 + reference UI — **skeleton landed** 2026-09-28 (policy on living § + `src/plugin/runtime/samples/industry_pack/`; full builtin UI deletion / per-plugin migration out of scope)
