<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/legacy/plugin` subdirectory layout Implementation Plan

> **For agentic workers:** Landed on **master**. Spec: [`../specs/2026-09-27-legacy-plugin-subdirectory-layout-design.md`](../specs/2026-09-27-legacy-plugin-subdirectory-layout-design.md).

**Status:** landed (docs / freeze; physical moves already on tree)  
**Date:** 2026-09-27  
**Spec:** [`../specs/2026-09-27-legacy-plugin-subdirectory-layout-design.md`](../specs/2026-09-27-legacy-plugin-subdirectory-layout-design.md)  
**Parent layout:** [`../../specs/2026-09-14-plugin-subdir-layout-design.md`](../specs/2026-09-14-plugin-subdir-layout-design.md)

**Goal:** Freeze and document the as-built `src/legacy/plugin` layout; no further subdirectory moves unless a future strangler retires a domain DLL.

**Architecture:** AuxModule DLL at package root; `adapter/` bridges; one directory per domain MFC DLL; `orthogrid/kernel/` holds the 2010 Orthogrid class. Endgame Views stay under `src/plugin/product/*`.

**Tech Stack:** GN `smt_shared_library` / `smt_mfc_shared_library`, LoadLibrary `dll_stem`, leftover MFC.

## Global Constraints

- Preserve `Smt_*` / LoadLibrary / DEF export ABI; no drive-by snake_case on leftover exports.  
- No include shims at old `plugin/legacy/` paths.  
- No Qt; Views + Skia remain the desktop endgame.  
- User did not ask for commit → do not commit.  
- Parallel agents own `legacy/render` and `legacy/tool` — leave those trees alone.

---

## Locked decisions（禁止反转）

| # | Decision |
| --- | --- |
| 1 | **Option A — freeze as-built**; reject nesting AuxModule into `aux/` and nesting `dlg_*` deeper in this plan. |
| 2 | Aggregate labels and `dll_stem` names stay. |
| 3 | Docs + README are the deliverable for this plan; code moves only if inventory finds a mismatch with the freeze tree. |

---

## Task 1: Inventory vs freeze tree

- [x] List top-level dirs under `src/legacy/plugin` (`adapter`, `dem`, `proj`, `print`, `model3d`, `orthogrid` + root AuxModule sources).  
- [x] Confirm `adapter/` has `legacy_am` / `legacy_cmd` source_sets only.  
- [x] Confirm `orthogrid/kernel/` owns 2010 Orthogrid sources; `orthogrid_kernel` GN target present.  
- [x] Confirm callers include `legacy/plugin/….h` (xview, xcatalog, smtapp, domain plugs).  
- [x] Verdict: **no file moves required** — tree matches freeze.

---

## Task 2: Spec + plan + index docs

- [x] Write [`../specs/2026-09-27-legacy-plugin-subdirectory-layout-design.md`](../specs/2026-09-27-legacy-plugin-subdirectory-layout-design.md).  
- [x] Write this plan.  
- [x] Revise L1 parent status note (legacy half landed / pointer to 2026-09-27 freeze).  
- [x] Update [`../../../README.md`](../../../README.md) Active table row for plugin subdir.  
- [x] Add [`../../../../src/legacy/plugin/README.md`](../../../../src/legacy/plugin/README.md).  
- [x] Add `plugin/` row to [`../../../../src/legacy/README.md`](../../../../src/legacy/README.md) if missing.

---

## Task 3: Optional compile smoke (only if user asks)

- [x] Smoke command documented for human (agent does not run `build.bat` / `ninja` per `no-agent-build`). Graph query: default `out/` has `:plugin`, adapter, `:orthogrid_kernel` / `:plugin_orthogrid`; domain `plugin_*` need `smt_build_app` (`build.bat legacy_app`). Prior artifact `out/plugin_d.dll` (2026-09-27).
- [x] Smoke verified green on 2026-09-27 (`out/smoke_*.log` for the six targets above).

```bat
.\build.bat src/legacy/plugin:plugin
.\build.bat src/legacy/plugin/adapter:am
.\build.bat src/legacy/plugin/adapter:cmd
.\build.bat src/legacy/plugin/orthogrid:orthogrid_kernel
.\build.bat src/legacy/plugin/orthogrid:plugin_orthogrid
.\build.bat legacy_app
```

---

## Task 4: Archive when freeze docs land

- [x] Spec Status `landed`; move spec+plan to `docs/superpowers/archive/{specs,plans}/`; as-built facts remain in `docs/superpowers/src-layout.md` + module README.  
- [x] Parent L1 stays living for product/runtime; archive only this legacy freeze topic.

---

## Non-goals (do not open)

- Moving `module*` / `plugin_msg*` into `aux/`.  
- Domain `dlg_*` reshuffles.  
- Changes under `src/plugin/runtime` or `src/plugin/product` (owned by L1 / full-upgrade).  
- `src/legacy/tool/**` or `src/legacy/render/**`.
