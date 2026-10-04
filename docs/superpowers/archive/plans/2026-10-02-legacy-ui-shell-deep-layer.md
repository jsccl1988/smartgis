<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `legacy/ui/shell` deep layer — Implementation Plan

**Status:** landed (archived 2026-10-03 — checkboxes complete)

> **Living lock:** [`../../specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md`](../../specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md) §11f.  
> **Diagram:** [`../../diagrams/legacy-ui-shell-deep-layer.html`](../../diagrams/legacy-ui-shell-deep-layer.html)

**Goal:** Role stems + ambox/chart composition under `legacy/ui/shell`; map hosts leave `shell_sources`; scheme C; freeze exported types.

**Done when:** tree matches §11f; `build.bat debug ui_legacy` green.

## Tasks

- [x] Root: `shell.*` → `xview.*`; `view_shell.*` → `input_dispatch.*`
- [x] `ambox/title` — shared caption helpers; `ambox.*` → `tree.*`; `ambox_dock_bar.*` → `outlook_bar.*`
- [x] `chart`: `diagramdata` → `diagram_data`; `chart_view_dlg` → `view_dlg`; `diagram.rc` → `chart.rc`
- [x] `map_sources` owns map views; drop map TUs from `shell_sources`
- [x] Include / GN / README / umbrella §11f + diagram
- [x] `build.bat debug ui_legacy` green
