<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# DLL / STL trust model — checklist

**Status:** landed (2026-10-23); build-consistency slice 2026-10-24  
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

## P6 — Build consistency (2026-10-24)

- [x] `base/core/abi_guard.h` + type-local `detect_mismatch` (`draw_host` / `view`)
- [x] `//build:gen_build_id` → `SMARTGIS_BUILD_ID` + `out/<config>/smartgis_build_id.txt`
- [x] `product_shared_library` injects `module_build_id.cc` (`smartgis_module_build_id`)
- [x] `base::verify_product_dll_build_ids()` at Views `wWinMain`
- [x] Homogeneity gate scans build_id marker in product stem DLLs
- [x] Living § + diagram + agent rule updated (no SEH ABI “兜底”)

## P5 — Deferred (documented only)

- [x] Custom libc++ — deferred
- [x] Default Release static (`is_component_build=false`) — deferred experiment
- [x] Full PA-E — remains open plan `2026-09-28-partition-alloc-everywhere.md`
