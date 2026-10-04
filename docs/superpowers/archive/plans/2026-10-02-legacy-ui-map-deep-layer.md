<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `legacy/ui/map` deep layer — Implementation Plan

**Status:** landed (archived 2026-10-03 — checkboxes complete)

> **Living lock:** [`../../specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md`](../../specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md) §11e.  
> **Diagram:** [`../../diagrams/legacy-ui-map-deep-layer.html`](../../diagrams/legacy-ui-map-deep-layer.html)

**Goal:** Role dirs + composition helpers under `legacy/ui/map`; scheme C; freeze `Smt*XView` ABI.

**Done when:** tree matches §11e; `build.bat debug ui_legacy` green.

## Tasks

- [x] `chrome/` — `identity_hud` + `aux_overlay` (dedupe 2D/3D HUD / rubber-band)
- [x] `framing/` — `oper_map_frame` (ZoomToRect / deferred timer)
- [x] `menu/` — AM-module attach helper shared by 2D/3D
- [x] `tools/` — Workspace bind + `view.pan` / ZOOMMOVE align
- [x] `viewport/` — move `view_2d` / `view_3d` / `view_2d_edit`; strip edit passthrough
- [x] `map_sources` GN + include sweep; README + umbrella §11e
- [x] `build.bat debug ui_legacy` green
