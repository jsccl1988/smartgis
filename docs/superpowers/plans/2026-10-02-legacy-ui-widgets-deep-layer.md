<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `legacy/ui/widgets` deep layer — Implementation Plan

> **Living lock:** [`../specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md`](../specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md) §11d.  
> **Diagram:** [`../diagrams/legacy-ui-widgets-deep-layer.html`](../diagrams/legacy-ui-widgets-deep-layer.html)

**Goal:** Responsibility dirs + composition under `legacy/ui/widgets`; scheme C; freeze sole AFX `DllMain` + `CBCGP*` typedef bridge.

**Done when:** tree matches §11d; `build.bat debug ui_legacy` green.

## Tasks

- [x] `feature_pack/` — rename `bcg_cmfc.h` → `feature_pack.h` (typedefs / macros / DPI only)
- [x] `prop/` — `prop_list.h` + `prop_value.h` (`prop_as_long` / `prop_as_float`); drop `prop_list_dock` stem
- [x] `dll/` — `dll_main.cpp` + `afx_ext_support.h` (sole AFX attach)
- [x] Include / GN sweep; README + src-layout + umbrella §11d + diagram
- [x] `build.bat debug //src/legacy/ui/widgets:widgets_sources` green (`ui_legacy` full link may be blocked by parallel map/inspect/chart waves)
