<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/legacy/tool/group` subdirectory layout


> **Status: superseded** (2026-09-28 merge). Merged into umbrella §SP1 + `2026-09-13-tool-event-dispatch-design.md`. Do not revise here except mechanical link fixes.

**Date:** 2026-09-27  
**Status:** active  
**Implementation note (2026-09-27):** Tree / includes / GN paths moved on master working tree; human still needs `.\build.bat` (and optional `legacy_app`) before archive.  
**Scope:** Physical + GN layout inside leftover `src/legacy/tool/group/` only. No behavior / `Smt*` / `ui_legacy` link graph change.  
**Supersedes (partial):** archived [`../archive/specs/2026-09-27-legacy-tool-subdirectory-layout-design.md`](../archive/specs/2026-09-27-legacy-tool-subdirectory-layout-design.md) decision that `group/` stays **flat** — that package-level split (`iatool` / `adapter` / `group`) remains landed; this spec nests **inside** `group/`.  
**Related:**

| Topic | Doc | Relation |
| --- | --- | --- |
| Leftover tool package | [`../archive/specs/2026-09-27-legacy-tool-subdirectory-layout-design.md`](../archive/specs/2026-09-27-legacy-tool-subdirectory-layout-design.md) | **landed** — `iatool` + `adapter` + `group` modules; flat-`group` non-goal superseded here |
| Earlier split non-goal | [`../archive/specs/2026-09-13-tool-legacy-split-design.md`](../archive/specs/2026-09-13-tool-legacy-split-design.md) | Rejected view/select/edit under `group/` at package-move time; reopened now that package root is clean |
| Endgame tool | [`2026-09-27-tool-subdirectory-layout-design.md`](2026-09-27-tool-subdirectory-layout-design.md) | Unchanged; leftover only |
| Peer leftover layout | [`2026-09-27-legacy-app-subdirectory-layout-design.md`](2026-09-27-legacy-app-subdirectory-layout-design.md) | Same idea: responsibility dirs + break includes + no shims |
| As-built | [`../../build/src-layout.md`](../../build/src-layout.md), [`../../../src/legacy/tool/README.md`](../../../src/legacy/tool/README.md) | Update in the landing change |

**Plan:** [`../plans/2026-09-27-legacy-tool-group-subdirectory-layout.md`](../plans/2026-09-27-legacy-tool-group-subdirectory-layout.md)

---

## 1. Goal / Non-goals

### 1.1 Goal

Split the flat `Smt*Tool` dump under `src/legacy/tool/group/` into responsibility modules:

1. **Directory = role** — `base` / `view` / `select` / `input` / `factory`.
2. **Headers move with sources** (colocated); callers include `legacy/tool/group/<role>/….h`.
3. **Shared package surface stays at `group/` root:** `defs.h`, `resource.h`, `group_tool_core.rc`, `res/`.
4. **GN labels frozen:** `//src/legacy/tool/group:tool_group_sources` and `:tool_group` (still compile into `ui_legacy`).

### 1.2 Non-goals

- **Do not** rename `Smt*` / `GroupToolType` / export ABI / Notify behavior.
- **Do not** change strangler semantics (`bind_workspace`, Draft apply, `try_execute_gt_msg`).
- **Do not** leave forwarding shim headers at old `legacy/tool/group/<file>.h` paths.
- **Do not** touch `iatool/`, `adapter/`, endgame `src/tool/`, or `src/legacy/render/**`.
- **Do not** introduce Qt or snake_case renames of leftover methods.
- **Do not** deepen past `src/legacy/tool/group/<role>/…` (no `input/point/…` public trees).

---

## 2. Locked decisions

| # | Decision |
| --- | --- |
| 1 | **Scheme B (now chosen):** nest tools by role under `group/<role>/`. |
| 2 | **No shim headers** at former flat paths. |
| 3 | Aggregate labels stay **`tool_group_sources`** / **`tool_group`**. Optional fine-grained `source_set`s under roles are allowed; v1 keeps one `source_set` listing new paths. |
| 4 | **Root freeze:** `defs.h`, `resource.h`, `group_tool_core.rc`, `res/` stay at `group/`. |
| 5 | Public leftover C++ names stay **`Smt*`**. |
| 6 | Git: **master** only; no feature branch; commit only when user asks. |
| 7 | Agent does **not** run `build.bat` / ninja / exe. |

---

## 3. Target tree

```
src/legacy/tool/group/
  BUILD.gn                 # source_set("tool_group_sources") + group("tool_group")
  README.md
  defs.h                   # shared enums / GT_MSG_* (unchanged path)
  resource.h
  group_tool_core.rc
  res/

  base/
    basetool.h  basetool.cpp
    base3dtool.h  base3dtool.cpp

  view/
    viewctrltool.h  viewctrltool.cpp
    3dviewctrltool.h  3dviewctrltool.cpp

  select/
    selecttool.h  selecttool.cpp
    flashtool.h  flashtool.cpp

  input/
    inputpointtool.h  inputpointtool.cpp
    inputlinetool.h  inputlinetool.cpp
    inputregiontool.h  inputregiontool.cpp
    appendfeaturetool.h  appendfeaturetool.cpp

  factory/
    grouptoolfactory.h  grouptoolfactory.cpp
```

**Forbidden after land:** public tool headers at flat `group/*.h` except `defs.h` / `resource.h`.

### 3.1 Include migration map

| Old | New |
| --- | --- |
| `legacy/tool/group/basetool.h` | `legacy/tool/group/base/basetool.h` |
| `legacy/tool/group/base3dtool.h` | `legacy/tool/group/base/base3dtool.h` |
| `legacy/tool/group/viewctrltool.h` | `legacy/tool/group/view/viewctrltool.h` |
| `legacy/tool/group/3dviewctrltool.h` | `legacy/tool/group/view/3dviewctrltool.h` |
| `legacy/tool/group/selecttool.h` | `legacy/tool/group/select/selecttool.h` |
| `legacy/tool/group/flashtool.h` | `legacy/tool/group/select/flashtool.h` |
| `legacy/tool/group/inputpointtool.h` | `legacy/tool/group/input/inputpointtool.h` |
| `legacy/tool/group/inputlinetool.h` | `legacy/tool/group/input/inputlinetool.h` |
| `legacy/tool/group/inputregiontool.h` | `legacy/tool/group/input/inputregiontool.h` |
| `legacy/tool/group/appendfeaturetool.h` | `legacy/tool/group/input/appendfeaturetool.h` |
| `legacy/tool/group/grouptoolfactory.h` | `legacy/tool/group/factory/grouptoolfactory.h` |
| `legacy/tool/group/defs.h` | **unchanged** |
| `legacy/tool/group/resource.h` | **unchanged** |

### 3.2 Known call sites (blast radius)

Rewrite in the landing change:

- Internal: all moved TUs + `factory/grouptoolfactory.*` + `base/basetool.cpp` (factory include)
- UI: `src/legacy/ui/xview/{view_2d,view_2d_edit,view_3d}.*`
- Plugins that only include `defs.h` need **no** path change
- Comment in `src/legacy/tool/adapter/msg.h` may keep pointing at `group/defs.h`

---

## 4. Approaches considered

| Scheme | Idea | Verdict |
| --- | --- | --- |
| A | Keep flat | Rejected — user requested subdirectory layout; package root already cleaned |
| **B (chosen)** | Role dirs: base / view / select / input / factory | Matches `GroupToolType` families; small external blast (`defs.h` stays) |
| C | Mirror endgame `tool/{nav,draft,…}` names | Rejected — leftover ABI names stay; false symmetry with endgame |

---

## 5. Done when

1. Target tree matches §3; zero flat tool headers except `defs.h` / `resource.h`.
2. All in-tree includes use §3.1 paths; no shim headers.
3. `:tool_group_sources` / `:tool_group` labels unchanged; sources list new paths.
4. `src/legacy/tool/README.md`, `src/legacy/README.md`, Tool/group rows in `docs/build/src-layout.md` updated.
5. Human verifies with `.\build.bat` (or `ui_legacy` / `legacy_app` as gated).

---

## 6. Spec self-review

| Check | Result |
| --- | --- |
| Placeholders | None |
| Contradictions | Explicitly supersedes flat-`group` non-goal only; package `iatool`/`adapter` unchanged |
| Scope | Layout + includes + GN paths only |
| Nesting | Cap `group/<role>/` |
| Colocation | Headers move with `.cpp` |
