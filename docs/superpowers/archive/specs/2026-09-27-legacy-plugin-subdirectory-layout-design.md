<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/legacy/plugin` subdirectory layout (as-built freeze)

**Date:** 2026-09-27  
**Status:** landed  
**Implementation note (2026-09-27):** Scheme A freeze as-built on master — no further subdirectory moves. Docs + module README are the deliverable; physical tree already matched L1. Optional compile smoke is user-owned (`build.bat` / `ninja -C out` targets below).  
**Scope:** Physical + GN layout of leftover AuxModule / domain MFC shells under `src/legacy/plugin` only. Does **not** reopen endgame `src/plugin/{runtime,product}` layout.  
**Parent (living):** [`../../specs/2026-09-14-plugin-subdir-layout-design.md`](../../specs/2026-09-14-plugin-subdir-layout-design.md) — L1 + I1; this doc freezes the **legacy half** after relocate.  
**Related:** full upgrade [`../../specs/2026-09-14-plugin-full-upgrade-design.md`](../../specs/2026-09-14-plugin-full-upgrade-design.md); as-built [`../../../build/src-layout.md`](../../../build/src-layout.md), [`../../../../src/legacy/plugin/README.md`](../../../../src/legacy/plugin/README.md).  
**Peer precedent (pattern only):** tool / atmosphere / gpu subdirectory specs — nest by responsibility, stable aggregate GN, no shim headers; leftover ABI stays.  
**Plan (landed):** [`../plans/2026-09-27-legacy-plugin-subdirectory-layout.md`](../plans/2026-09-27-legacy-plugin-subdirectory-layout.md)

Smoke (human; agent does not run compilers):

```bat
.\build.bat src/legacy/plugin:plugin
.\build.bat src/legacy/plugin/adapter:am
.\build.bat src/legacy/plugin/adapter:cmd
.\build.bat src/legacy/plugin/orthogrid:orthogrid_kernel
.\build.bat src/legacy/plugin/orthogrid:plugin_orthogrid
REM Domain MFC DLLs are app-gated — regenerate with smt_build_app:
.\build.bat legacy_app
```

## Goal

1. Record the **as-built** tree after `src/plugin/legacy` → `src/legacy/plugin` and adapter / orthogrid-kernel moves.
2. Lock **no further subdirectory churn** inside `legacy/plugin` unless a future strangler step retires a whole domain DLL.
3. Keep dual-run clear: Views builtins live under `src/plugin/product/*`; MFC `*.am` / AuxModule stay here.

## Non-goals

- **Do not** rename `Smt_*` / `dll_stem` / LoadLibrary export ABI, or snake_case leftover exports.
- **Do not** nest AuxModule core into `aux/` / `host/` (would retarget every `#include "legacy/plugin/module*.h"` / `plugin_msg.h` in `legacy/app`, `legacy/ui/*`).
- **Do not** flatten or re-nest `dlg_*` under `dem/dlg/` etc. (leftover MFC noise; Phase B of full-upgrade retires them from Views, not by reshuffling).
- **Do not** edit `src/legacy/render/**` or `src/legacy/tool/**` layout in this topic.
- **Do not** introduce Qt or move product domains back under `legacy/`.
- **Do not** add include shims at old `plugin/legacy/…` paths.

## Locked decisions

| # | Decision |
| --- | --- |
| 1 | Target tree = **current on-disk tree** (freeze). Matches L1 legacy half. |
| 2 | AuxModule DLL sources stay at **`legacy/plugin/` root** (`module*`, `plugin_msg*`); GN `//src/legacy/plugin:plugin`, `dll_stem=plugin`. |
| 3 | Bridges stay in **`adapter/`** (`am` / `cmd` source_sets; not the AuxModule DLL). Root `//src/plugin:cmd` re-exports `:cmd` (`:legacy_cmd` alias). |
| 4 | Domain MFC shells stay **`legacy/plugin/<domain>/`** with one `smt_mfc_shared_library` each (`plugin_dem`, `plugin_proj`, `plugin_print`, `plugin_model3d`, `plugin_orthogrid`). |
| 5 | 2010 `Orthogrid` stays under **`orthogrid/kernel/`** (`//src/legacy/plugin/orthogrid:orthogrid_kernel`). Product Laplace/Views remain under `plugin/product/orthogrid`. |
| 6 | Public C++ for bridges stays **`namespace plugin`** (two layers). Leftover classes keep existing `Smt*` names. |
| 7 | Includes: `legacy/plugin/….h` only — **no** transitional `plugin/legacy/` shims. |
| 8 | Git: work on **`master`**; no feature branch for this layout. |

## Approaches considered

| Option | Idea | Verdict |
| --- | --- | --- |
| **A — Freeze as-built** | Document + README; no file moves | **Chosen.** Tree already matches L1; further moves are include churn without ABI gain. |
| B — Nest AuxModule into `aux/` | `legacy/plugin/aux/module*` | Rejected now: callers in `legacy/app`, `legacy/ui/xview`, `legacy/ui/xcatalog`, domain plugs. |
| C — Nest `dlg_*` / `res/` deeper | `dem/dlg/…`, `dem/shell/…` | Rejected: leftover-only; full-upgrade Phase B deletes Views use, not rearrange MFC. |

## As-built tree

```
src/legacy/plugin/
  BUILD.gn                 # smt_shared_library("plugin") — AuxModule DLL
  module.h / module.cpp
  module_manager.h / module_manager.cpp
  plugin_msg.h / plugin_msg.cpp
  README.md                # as-built pointer

  adapter/
    BUILD.gn               # :am :cmd (source_set; :legacy_* aliases)
    am.h / .cc
    cmd.h / .cc

  dem/                     # plugin_dem DLL + MFC dlg_*
  proj/                    # plugin_proj
  print/                   # plugin_print
  model3d/                 # plugin_model3d
  orthogrid/               # plugin_orthogrid shell (plug/creater/…)
    kernel/                # orthogrid_kernel — 2010 Orthogrid class
      detail/polyline.*
```

Depth cap: `src/legacy/plugin/<bucket>/…` (adapter or domain). `orthogrid/kernel/` is the one allowed extra nest for the 2010 class (already landed in L1).

## GN / dual-run

| Label | Role |
| --- | --- |
| `//src/legacy/plugin:plugin` | AuxModule runtime DLL |
| `//src/legacy/plugin/adapter:am` | `*.am` scan → Registry manifests (host deps this; no upward cycle) |
| `//src/legacy/plugin/adapter:cmd` | `AM_MSG_*` → catalog ids |
| `//src/legacy/plugin/<domain>:plugin_*` | Domain MFC DLLs |
| `//src/legacy/plugin/orthogrid:orthogrid_kernel` | Linked by `plugin_orthogrid` (and any product need for CreateOrthGrid) |
| `//src/plugin:plugin` / `:cmd` | Re-export groups only (`:legacy_cmd` alias) |

Endgame builtins: `//src/plugin/product/<domain>:*_views` — **not** moved by this spec.

## Callers (inventory, 2026-09-27)

- `legacy/app/smtapp.cpp` — `module_manager.h` (`InitSmtAuxModules`).
- `legacy/ui/shell/*`, `legacy/ui/catalog/*`, `legacy/ui/panels/*` — `plugin_msg` / `module_manager` / `adapter/cmd`.
- Domain `*_plug.cpp` — `plugin_msg` + `adapter/cmd`.
- Views path does **not** compile domain `dlg_*.cpp` (full-upgrade Phase B).

CBM note: index may still list pre-relocate `src/plugin/legacy/` / `src/plugin/host/legacy_*.cc`; trust on-disk `src/legacy/plugin/` until a user-requested reindex.

## Deferred (out of this freeze)

- Delete a domain DLL when its Views builtin fully replaces MFC shell (full-upgrade Phase B/D).
- Optional later: nest AuxModule root into `aux/` **only** with a dedicated include migration plan and no shim headers.

## Acceptance

- [x] On-disk tree matches § As-built tree.
- [x] No `plugin/legacy/` include paths remain for these TUs.
- [x] Module README + `docs/README.md` row accurate (this change set).
- [x] No further moves under `src/legacy/plugin/**` without revising this spec.
