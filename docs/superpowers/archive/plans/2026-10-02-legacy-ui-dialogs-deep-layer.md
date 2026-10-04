<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `legacy/ui/dialogs` deep layer — Implementation Plan

**Status:** landed (archived 2026-10-03 — checkboxes complete)

> **Living lock:** [`../../specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md`](../../specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md) §11i.  
> **Diagram:** [`../../diagrams/legacy-ui-dialogs-deep-layer.html`](../../diagrams/legacy-ui-dialogs-deep-layer.html)

**Goal:** Role dirs + composition helpers under `legacy/ui/dialogs`; scheme C stems; freeze `Smt*Dlg` / `CDlg*` ABI. Inspect docks owned by §11h `inspect_sources`.

**Done when:** tree matches §11i; `build.bat debug ui_legacy` green.

## Tasks

- [x] `toolkit/` — `input_text_dialog` + `select_one_dialog` (≈ `ui/views/dialogs`)
- [x] `gis/` — `feature_info_dialog` + `att_struct_dialog` (≈ `ui/gis/inspect` AttributeSchema + feature info)
- [x] `detail/` — `ogr_field_type` + `feature_info_grid` composition helpers
- [x] Slim `dialogs_api` (keep exports; retire edit-param body noise)
- [x] GN + include sweep; README + umbrella §11i + src-layout
- [x] `build.bat debug ui_legacy` green
