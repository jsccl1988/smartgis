<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/legacy/tool` subdirectory layout

**Date:** 2026-09-27  
**Status:** landed  
**Implementation note (2026-09-27):** Tree / includes / GN landed on master; `ninja -C out legacy_tool` LINK OK. Package root is only `BUILD.gn` + `README.md` + `iatool/` + `adapter/` + `group/`.  
**Scope:** Physical + GN layout of leftover `src/legacy/tool` only (`SmtIATool` ABI, group tools, GT_MSG adapter). No behavior or DLL ABI change.  
**Related:**

| Topic | Doc | Relation |
| --- | --- | --- |
| Leftover physical split (landed) | [`2026-09-13-tool-legacy-split-design.md`](2026-09-13-tool-legacy-split-design.md) | Placed leftover under `legacy/tool/`; **partially superseded** here for root `t_*` nesting only |
| Endgame tool layout | [`../../specs/2026-09-27-tool-subdirectory-layout-design.md`](2026-09-27-tool-subdirectory-layout-design.md) | Modern `src/tool/<module>/`; adapter already at `legacy/tool/adapter/` |
| SP1 Workspace strangler | [`../../specs/2026-09-19-legacy-tool-workspace-strangler-design.md`](2026-09-19-legacy-tool-workspace-strangler-design.md) | Dual-run activation unchanged; include paths for `t_*` updated |
| SP1b behavior migration | [`../../specs/2026-09-19-tool-behavior-migration-design.md`](2026-09-19-tool-behavior-migration-design.md) | Behavior ownership unchanged |
| Peer precedent | [`../../specs/2026-09-27-gpu-subdirectory-layout-design.md`](2026-09-27-gpu-subdirectory-layout-design.md); `src/legacy/render/{bridge,gdi,…}` | Root = aggregate BUILD + module dirs; no shim headers |
| As-built | [`../../src-layout.md`](../../src-layout.md), [`../../../../src/legacy/README.md`](../../../../src/legacy/README.md), [`../../../../src/legacy/tool/README.md`](../../../../src/legacy/tool/README.md) | Updated in the landing change |

**Plan (landed):** [`../plans/2026-09-27-legacy-tool-subdirectory-layout.md`](../plans/2026-09-27-legacy-tool-subdirectory-layout.md)

---

## 1. Goal / Non-goals

### 1.1 Goal

Finish the leftover tool tree so the package root is only aggregate BUILD + README + module directories:

1. **`iatool/`** — LoadLibrary / `SmtIATool` / manager / `t_msg` / `tool_export` (today flat at `legacy/tool/`).
2. **`adapter/`** — keep (already parked): `GT_MSG_*` → `tool::Workspace` (`source_set`, not `legacy_tool` DLL).
3. **`group/`** — keep **flat** (all `Smt*Tool` + factory + RC); do **not** split by view/select/edit (same non-goal as the 2026-09-13 split).

Callers include `legacy/tool/iatool/….h` (break includes; **no** shim at old `legacy/tool/t_*.h`). Aggregate GN labels `//src/legacy/tool:legacy_tool` / `:tool` / `:legacy_tool_all` stay stable.

### 1.2 Non-goals

- **Do not** rename `Smt_*` / `dll_stem=legacy_tool` / export macros / Notify ABI.
- **Do not** change strangler / dual-run semantics (`bind_workspace`, `try_execute_gt_msg`, Draft apply).
- **Do not** nest `group/` into view/select/edit (or deeper than `legacy/tool/group/<file>`).
- **Do not** touch `src/legacy/render/**` or endgame `src/tool/<module>/` layout (except docs cross-links).
- **Do not** put `adapter` into the `legacy_tool` DLL; keep `//src/legacy/tool/adapter:adapter` outside parent-package `assert_no_deps` patterns that already treat it as its own BUILD package.
- **Do not** introduce Qt, a second widget kit, or snake_case renames of leftover methods.
- **Do not** deepen beyond `src/legacy/tool/<module>/…` for public headers.

---

## 2. Locked decisions

| # | Decision |
| --- | --- |
| 1 | **Scheme A:** three modules — `iatool` + `adapter` + `group`. |
| 2 | **No shim headers** at `legacy/tool/t_*.h` / `tool_export.h`. Migrate every in-tree include in the same change. |
| 3 | Aggregate labels stay **`//src/legacy/tool:legacy_tool`**, **`:tool`**, **`:legacy_tool_all`**. Fine-grained `//src/legacy/tool/iatool:tool_sources` holds the IATool TUs. |
| 4 | **`group/` stays flat**; still `tool_group_sources` → `ui_legacy` (cycle avoidance unchanged). |
| 5 | **`adapter/` path and label unchanged.** |
| 6 | Public leftover C++ names stay **`Smt*`**; adapter API stays **`namespace tool`**. |
| 7 | Git: work on **`master`**; no feature branch. |
| 8 | Parallel agents may own `src/legacy/render` — **out of scope** for this change. |

---

## 3. Current vs target tree

### 3.1 Current (2026-09-27)

```
src/legacy/tool/
  BUILD.gn                 # tool_sources + legacy_tool DLL + groups
  tool_export.h
  t_iatool.{h,cpp}
  t_iatoolmanager.{h,cpp}
  t_msg.{h,cpp}
  adapter/                 # legacy_msg (source_set)
  group/                   # flat Smt* tools + factory + RC
```

### 3.2 Target

```
src/legacy/tool/
  BUILD.gn                 # smt_shared_library + groups; deps iatool:tool_sources
  README.md                # as-built pointer

  iatool/
    BUILD.gn               # source_set("tool_sources")
    tool_export.h
    t_iatool.h  t_iatool.cpp
    t_iatoolmanager.h  t_iatoolmanager.cpp
    t_msg.h  t_msg.cpp

  adapter/                 # unchanged paths / label
    BUILD.gn
    msg.h  msg.cc

  group/                   # unchanged paths / flat layout
    BUILD.gn
    basetool.*  base3dtool.*  …  grouptoolfactory.*
    res/  *.rc  …
```

**Forbidden after land:** public headers at `src/legacy/tool/*.h` (only `BUILD.gn` + `README.md` at package root).

### 3.3 Include migration map

| Old | New |
| --- | --- |
| `legacy/tool/t_iatool.h` | `legacy/tool/iatool/t_iatool.h` |
| `legacy/tool/t_iatoolmanager.h` | `legacy/tool/iatool/t_iatoolmanager.h` |
| `legacy/tool/t_msg.h` | `legacy/tool/iatool/t_msg.h` |
| `legacy/tool/tool_export.h` | `legacy/tool/iatool/tool_export.h` |
| `legacy/tool/adapter/…` | unchanged |
| `legacy/tool/group/…` | unchanged |

### 3.4 Known call sites (blast radius)

Rewrite in the landing change (re-grep `legacy/tool/t_` at land time):

- Internal: `src/legacy/tool/iatool/*`, `src/legacy/tool/group/{basetool,base3dtool,selecttool,viewctrltool}.*`
- UI: `src/legacy/ui/shell/*`, `src/legacy/ui/catalog/*`
- App: `src/legacy/app/smtapp.cpp`
- Plugins: `src/legacy/plugin/{dem,orthogrid,print,proj,model3d}/…` (and any other hit)

GN: root `BUILD.gn` deps `//src/legacy/tool/iatool:tool_sources`; `group/BUILD.gn` still deps `//src/legacy/tool:tool` (DLL), not sources.

---

## 4. Approaches considered

| Scheme | Idea | Verdict |
| --- | --- | --- |
| **A (chosen)** | Nest root ABI → `iatool/`; keep `adapter` + flat `group` | Matches `legacy/render` module dirs; small include blast; honors 2026-09-13 “no group subtype split” |
| B | Also split `group/` into view/select/edit/base | Rejected — large churn, prior explicit non-goal, strangler treats tools as peers |
| C | Keep `t_*` at package root; only add README | Rejected — does not finish “directory = responsibility”; root stays a dump |

---

## 5. Dual-run / strangler notes

- Endgame `src/tool/{command,interaction,draft,nav,workspace}` unchanged.
- `adapter` remains the only intentional bridge (`namespace tool`); still a separate BUILD package so Views/`src_all` `assert_no_deps = ["//src/legacy/tool:*"]` continue to allow the adapter target.
- Group tools keep compiling into `ui_legacy` with deps on `//src/legacy/tool:tool` + `//src/legacy/tool/adapter:adapter` + `//src/tool:dispatch`.

---

## 6. Done when

- Package root has no `t_*.{h,cpp}` / `tool_export.h`.
- All in-tree includes use `legacy/tool/iatool/…` (no shims).
- `ninja -C out legacy_tool` (and existing `ui_legacy` / plugin consumers that already build) stay green for touched TUs.
- `src/legacy/tool/README.md`, `src/legacy/README.md`, `docs/superpowers/src-layout.md` Tool/leftover rows, and `docs/README.md` index updated.
- This spec → **landed** and moved to archive with the plan when verification completes.
