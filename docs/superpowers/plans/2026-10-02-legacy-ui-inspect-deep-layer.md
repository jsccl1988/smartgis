<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `legacy/ui/inspect` deep layer — Implementation Plan

> **Living lock:** [`../specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md`](../specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md) §11h.  
> **Diagram:** [`../diagrams/legacy-ui-inspect-deep-layer.html`](../diagrams/legacy-ui-inspect-deep-layer.html)

**Goal:** Responsibility dirs + composition under `legacy/ui/inspect`; scheme C; freeze `SysConfigDockBar` / `EditConfigDockBar` ABI; peel from `dialogs_sources`.

**Done when:** tree matches §11h; `build.bat debug ui_legacy` green.

## Tasks

- [x] `host/` — `prop_host` (create / size / paint / erase / option helpers)
- [x] `sys/` — `sys_config_dock` + `flash_styles` (was flat `config_dock_bar`)
- [x] `edit/` — `edit_config_dock` (was flat `edit_config_dock_bar`)
- [x] Own `inspect_sources` GN; peel docks from `dialogs_sources`; include sweep
- [x] Umbrella §11h + diagram + README Active row
- [ ] `build.bat debug ui_legacy` green
