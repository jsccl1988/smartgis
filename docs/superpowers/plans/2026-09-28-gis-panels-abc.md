<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# GIS panels A+B+C Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Land Measure / Selection (C), Symbology / Legend / LayerProperties (A), and SpatialAnalysis + History (B) as map-agnostic `ui::views` panels with shell inspector wiring.

**Architecture:** Toolkit widgets under `src/ui/gis/{style,analysis,inspect,shell,debug}/` expose string/POD/callback APIs. `BrowserView` places them on the inspector `TabStrip` and wires callbacks in `shell_panels.cc` without pulling GIS headers into panel TUs.

**Tech Stack:** C++23, `ui::views` primitives (Label/Button/TableView/RadioButton/Combobox/Textfield/TabStrip), GN `//src/ui/views:views`, `views_unittests`, `SmartGisViews` shell.

## Global Constraints

- Work on `master` only; no feature branch.
- Functions `snake_case`; namespaces two-level `ui::views`.
- Panels must not `#include` `gis/` or hold `SmtFeature*`.
- Output root `out/` via `build.bat`; no Qt.
- Copyright year 2026 on new/touched Mogu headers.

---

### Task 1: MeasurePanel + SelectionPanel (M1 toolkit)

**Files:**
- Create: `src/ui/gis/inspect/measure_panel.h`, `measure_panel.cc`
- Create: `src/ui/gis/inspect/selection_panel.h`, `selection_panel.cc`
- Modify: `src/ui/views/BUILD.gn`, `src/ui/views/views.h`
- Test: `src/ui/views/testing/unit/views_unittests.cc`

**Interfaces:**
- Produces: `MeasurePanel::{Mode, ResultRow, set_mode, set_results, set_mode_change}`
- Produces: `SelectionPanel::{LayerSummary, set_count, set_layers, set_command}`

- [x] **Step 1:** Implement MeasurePanel (radios length/area/azimuth + results table + unit label).
- [x] **Step 2:** Implement SelectionPanel (count label + summary table + Clear/Invert/Zoom/Export buttons).
- [x] **Step 3:** Register sources in BUILD.gn + views.h; add unittest coverage.
- [x] **Step 4:** `build.bat views_unittests` green for new asserts.

### Task 2: Symbology + Legend + LayerProperties (M2 toolkit)

**Files:**
- Create: `symbology_panel.*`, `legend_panel.*`, `layer_properties_panel.*`
- Modify: BUILD.gn, views.h, views_unittests.cc

**Interfaces:**
- Produces: `SymbologyPanel::{set_layer, set_fields, set_paint, on_apply}`
- Produces: `LegendPanel::{Entry, set_entries, set_toggle}`
- Produces: `LayerPropertiesPanel` TabStrip hosting Symbology + Source Label page

- [x] **Step 1:** SymbologyPanel — geom label, field Combobox, paint TableView, Apply button.
- [x] **Step 2:** LegendPanel — entries table + optional check column callback.
- [x] **Step 3:** LayerPropertiesPanel — inner TabStrip Symbology / Source.
- [x] **Step 4:** Unittests + BUILD/views.h.

### Task 3: SpatialAnalysis + History (M3 toolkit)

**Files:**
- Create: `spatial_analysis_panel.*`, `geoprocessing_history_panel.*`
- Modify: BUILD.gn, views.h, views_unittests.cc

**Interfaces:**
- Produces: `SpatialAnalysisPanel::{Operator, Param, set_operators, set_params, set_progress, set_run_handler, set_cancel_handler, history()}`
- Produces: `GeoprocessingHistoryPanel::{Entry, set_entries, set_rerun, set_clear}`

- [x] **Step 1:** GeoprocessingHistoryPanel table + Rerun/Clear.
- [x] **Step 2:** SpatialAnalysisPanel — ops table (category/id/title), params table + value Textfield, progress label, Run/Cancel, embed history.
- [x] **Step 3:** Unittests + BUILD/views.h.

### Task 4: Shell inspector wiring

**Files:**
- Modify: `src/app/views/shell/ui/browser_view.cc`, `browser_view.h`
- Modify: `src/app/views/shell/ui/panels/shell_panels.cc`
- Modify: `inspector_sync.cc` if selection/legend sync hooks exist

- [x] **Step 1:** Construct M1–M3 panels; add inspector tabs; keep ProcessingPanel for delegate API (may hide or keep tab).
- [x] **Step 2:** `wire_measure_panel` / `wire_selection_panel` / `wire_symbology_panel` / `wire_legend_panel` / `wire_spatial_analysis_panel` — status-bar feedback + reuse `run_processing_operator` for analysis run.
- [x] **Step 3:** Fix `on_processing` tab index to SpatialAnalysis / Processing.
- [x] **Step 4:** `build.bat` SmartGisViews (or views target) compiles.

### Task 5: Docs close-out

- [x] Confirm living §§ match as-built tab names.
- [x] Touch `docs/superpowers/README.md` Active row if needed.

---

## Spec coverage

| Spec item | Task |
| --- | --- |
| MeasurePanel | 1 |
| SelectionPanel | 1 |
| Symbology / Legend / LayerProperties | 2 |
| SpatialAnalysis / History | 3 |
| Shell TabStrip + wire | 4 |
