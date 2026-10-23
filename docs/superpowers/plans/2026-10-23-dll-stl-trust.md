<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# DLL / STL trust model — checklist

**Status:** landed (2026-10-23)  
**Living spec:** [`../specs/2026-09-14-base-root-hybrid-design.md`](../specs/2026-09-14-base-root-hybrid-design.md) §DLL / STL trust model  
**Diagram:** [`../diagrams/dll-stl-trust.html`](../diagrams/dll-stl-trust.html)  
**Plugin C-boundary:** [`../specs/2026-09-13-plugin-host-design.md`](../specs/2026-09-13-plugin-host-design.md) §DLL / STL C-boundary

## P0 — Document

- [x] Living § on base umbrella + diagram link
- [x] HTML diagram `diagrams/dll-stl-trust.html`
- [x] Superpowers README Active / Diagrams rows
- [x] Agent rule `.cursor/rules/repo/dll-stl-trust.mdc`

## P1 — Homogeneity gate

- [x] `build/tools/check_dll_homogeneity.py`
- [x] Hook into `check_out_artifacts` (product / harness / te)
- [x] Document in `build/README.md`

## P2 — Boundary classification

- [x] A/B/C/D table in living §
- [x] Plugin-host § for C-boundary (no STL on plugin wire)

## P3 — Export hygiene

- [x] `base/core/component_export.h` (`COMPONENT_EXPORT`)
- [x] Product `*_export.h` gated on `COMPONENT_BUILD`
- [x] A vs B/C export style locked in § + rule

## P4 — Component build hook

- [x] GN `is_component_build` (default `true`)
- [x] `COMPONENT_BUILD` define when true
- [x] `product_shared_library` → static when false

## P5 — Deferred (documented only)

- [x] Custom libc++ — deferred
- [x] Default Release static (`is_component_build=false`) — deferred experiment
- [x] Full PA-E — remains open plan `2026-09-28-partition-alloc-everywhere.md`
