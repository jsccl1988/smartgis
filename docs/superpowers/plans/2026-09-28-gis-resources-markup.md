<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# GIS resources nest + panel markup (Wave1) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Nest `src/ui/resources/` by area (aligned with `ui/gis`), preserve relative paths under `out/ui/`, and migrate Wave1 GIS panels to `load_markup` + id bind.

**Architecture:** Approach 1 from design — `resources/{toolkit,dialogs,catalog,inspect,shell,style,analysis,debug}/`; C++ tree under `ui/gis/` unchanged; panels adopt FillLayout + markup root like product dialogs.

**Tech Stack:** C++23, Yoga markup, GN copy groups, `ui_views.dll` (GIS chrome same PE).

**Living §:** [`../specs/2026-09-27-views-desktop-shell-design.md`](../specs/2026-09-27-views-desktop-shell-design.md) §Declarative markup + §ui/gis resources nest.

## Global Constraints

- Work on `master` only.
- No Qt; namespace stays `ui::views`.
- Panels remain string/callback-only (no GIS headers).

---

### Task 1: Nest resources + path updates

- [x] Move existing assets into `toolkit/` / `dialogs/`
- [x] GN per-area copy → shared `out/ui/<area>/{{source_file_part}}` (`$root_out_dir/../ui`)
- [x] Update all `load_markup` call sites + markup_unittests

### Task 2: Wave1 panel markup

- [x] `shell/status_bar.*` + StatusBar ctor
- [x] `inspect/measure_panel.*` + MeasurePanel ctor
- [x] `inspect/selection_panel.*` + SelectionPanel ctor
- [x] `style/legend_panel.*` + LegendPanel ctor

### Task 3: Docs + verify

- [x] Living umbrella § + README refresh
- [x] `build.bat debug markup_unittests` resolve nested paths green; `ui_views` links (GIS chrome)
- [ ] Full MarkupRoot load/destroy unittests (blocked on View `paint_delegate_` ABI across test exe / `ui_views.dll`)

### Task 4: Wave2/3 panel markup

- [x] Symbology / LayerProperties / FeatureInfo / AttributeTable
- [x] CatalogView (chrome + tabs host) / SpatialAnalysis / Processing / History / Atmosphere
- [x] DebugConsole / RenderTrace / DiagnosticTools chrome + `tabs_host` (Memory page paint stays C++)
- [x] Ambox scroll shell + ChartView title chrome + Memory page chrome → markup; dynamic buttons / series plot / sparkline stay C++
- [x] LayerTree stays custom paint (no outer chrome; Catalog hosts it)

---

## Spec coverage

| Spec item | Task |
| --- | --- |
| Nested resources + relative out/ui | 1 |
| Wave1 panels | 2 |
| Docs / tests | 3 |
| Wave2/3 panels | 4 |
