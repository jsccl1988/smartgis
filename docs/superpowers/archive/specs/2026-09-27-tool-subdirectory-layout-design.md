<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/tool` subdirectory layout (scheme C, break includes)


> **Status: superseded** (2026-09-28 merge). Merged into `2026-09-13-tool-event-dispatch-design.md` §Subdirectory layout. Do not revise here except mechanical link fixes.

**Date:** 2026-09-27  
**Status:** active  
**Implementation note (2026-09-27):** Tree / includes / GN / as-built docs are on master working tree. Full `draft_test` + `tool_dispatch_test` **link** still blocked by parallel WIP deleting `src/gis/datasource/gdal/*` while `//src/gis/model/edit:edit` → `//src/gis:gis` still pulls those sources. All tool TUs **compile**; `camera_nav_test` runs PASS. Re-run the two blocked tests and archive when gdal graph is green.  
**Follow-up (2026-09-27):** `adapter/` (`msg`) relocated from `src/tool/adapter/` to **`src/legacy/tool/adapter/`** (`//src/legacy/tool/adapter:adapter`, include `legacy/tool/adapter/msg.h`). Removed from `//src/tool:dispatch`. Public C++ API stays **`namespace tool`**. Own BUILD.gn package so `src_all` / Views `assert_no_deps = ["//src/legacy/tool:*"]` still allow the adapter source_set (parent-package pattern only). File stem is `msg` (no `legacy_` prefix — path already marks leftover).  
**Scope:** Physical + GN layout of the endgame `src/tool` dispatch tree only. One implementation plan after this spec is approved.  
**Related:**

| Topic | Doc | Relation |
| --- | --- | --- |
| Session dispatch API | [`2026-09-13-tool-event-dispatch-design.md`](2026-09-13-tool-event-dispatch-design.md) | **accepted** — API/contracts stay; path table in Components is superseded by this layout |
| SP1b behavior migration | [`2026-09-19-tool-behavior-migration-design.md`](2026-09-19-tool-behavior-migration-design.md) | **active** — behavior ownership unchanged; file paths update when this lands |
| SP1 Workspace strangler | [`2026-09-19-legacy-tool-workspace-strangler-design.md`](2026-09-19-legacy-tool-workspace-strangler-design.md) | **active** — adapters stay; include paths for `legacy_msg` / `Workspace` change |
| Leftover physical split | [`../archive/specs/2026-09-13-tool-legacy-split-design.md`](../archive/specs/2026-09-13-tool-legacy-split-design.md) | **landed** — “终局扁平保留 `tool/*.h`” is **partially superseded** by this spec |
| Product as-built | [`../../build/src-layout.md`](../../build/src-layout.md), [`../../../src/tool/README.md`](../../../src/tool/README.md) | Update in the same change that lands the move |
| Peer precedent | Plugin L1 [`2026-09-14-plugin-subdir-layout-design.md`](2026-09-14-plugin-subdir-layout-design.md); RHI plan [`../archive/plans/2026-09-27-rhi-subdirectory-split.md`](../archive/plans/2026-09-27-rhi-subdirectory-split.md) | Same idea: stable aggregate GN, break includes, no shim headers |

---

## 1. Goal / Non-goals

### 1.1 Goal

Split the flat `src/tool/` dump into clear modules under the nesting cap `src/tool/<module>/`, so:

1. **Directory = responsibility** (command / interaction / draft / nav / workspace). `GT_MSG` mapping lives under **`src/legacy/tool/adapter/`** (see follow-up note).
2. **Callers include** `tool/<module>/….h` — **no** root umbrella headers at `src/tool/*.h`. Adapter callers use `legacy/tool/adapter/….h`.
3. **GN** stays a single aggregate `//src/tool:dispatch` (`source_set` / `group` of source_sets), still in `src_all`, **not** a product DLL. Adapter is a separate `//src/legacy/tool/adapter:adapter` source_set.
4. Rename the overloaded `gestures` surface to **`draft`**; move camera math to **`nav`**; move `GT_MSG_*` mapping to **`legacy/tool/adapter`**.

### 1.2 Non-goals

- **Do not** change leftover `dll_stem` or `SmtIATool` ABI in *this* change. Leftover package nesting: [`../archive/specs/2026-09-27-legacy-tool-subdirectory-layout-design.md`](../archive/specs/2026-09-27-legacy-tool-subdirectory-layout-design.md) (`iatool/` + `adapter/` + flat `group/`).
- **Do not** change Command / Interaction / Draft / Workspace **runtime semantics** (ids, Draft flags, EventBus/EditSession policy) in this change — layout + include/GN only.
- **Do not** keep transitional umbrella headers under `src/tool/*.h`.
- **Do not** put `content::EventBus` under `src/tool`.
- **Do not** introduce Qt, a second widget kit, or a tool product DLL.
- **Do not** deepen beyond `src/tool/<module>/…` (no `src/tool/draft/stroke/…` public trees). Internals stay in the module file or `tool::detail` / anonymous namespace.
- **Do not** solve the content ↔ tool soft cycle in this spec (optional follow-up only).

---

## 2. Locked decisions

| # | Decision |
| --- | --- |
| 1 | **Scheme C layout** with modules listed in §3. |
| 2 | **No umbrella / shim headers** at `src/tool/*.h`. Migrate every in-tree `#include "tool/….h"` to `tool/<module>/….h` in the same change. |
| 3 | Aggregate GN label **`//src/tool:dispatch`** remains the public dep for `src_all` and most callers. Fine-grained `source_set`s under modules are allowed; root `BUILD.gn` re-exports via `group` or equivalent. |
| 4 | Still **`source_set`**, not a shared library / `dll_stem`. |
| 5 | **Leftover IATool ABI** under `src/legacy/tool/{t_*,group}` stays out of the endgame layout change. The **`legacy_msg` bridge** later moved to `src/legacy/tool/adapter/` (own package; not `legacy_tool` DLL). |
| 6 | **`gestures` → `draft`**: POD `Draft` / `draft_flags` + select/draw/view/view3d factories live under `draft/`. Drop the name `gestures` from public paths. |
| 7 | **`camera_nav` → `nav/`** (leaf; no Workspace types on headers). |
| 8 | **`legacy_msg` → `legacy/tool/adapter/`** (bridge out of endgame `src/tool`; API namespace remains `tool`). |
| 9 | Public C++ namespace stays **`tool`** (two layers). Helpers in **`tool::detail`** or anonymous namespace — not a third semantic namespace. |
| 10 | Git: work on **`master`**; no feature branch for this layout. |
| 11 | Tests move with sources or stay co-located under the owning module; targets may keep output names `tool_dispatch_test` / `camera_nav_test` / `gestures_test` **or** rename `gestures_test` → `draft_test` in the same change (prefer rename for honesty). |

---

## 3. Target tree

```
src/tool/
  BUILD.gn                      # group("dispatch") + tests; re-exports module source_sets
  README.md                     # as-built pointer (update when landing)

  command/
    BUILD.gn                    # optional; or sources listed from root
    command.h
    command.cc

  interaction/
    interaction.h
    interaction.cc

  draft/                        # was gestures.*
    draft.h                     # Draft, DraftKind, draft_flags, DraftCallback, make_* factories
    draft.cc

  nav/                          # was camera_nav.*
    camera_nav.h
    camera_nav.cc

  workspace/
    workspace.h
    workspace.cc
    builtins.cc                 # optional extract of register_builtins(); same module

  # (adapter / legacy_msg moved to src/legacy/tool/adapter/ — see follow-up note)

  # Tests: under module dirs or src/tool/*_test.cc — pick one style and match peers;
  # recommended: co-locate (e.g. draft/draft_test.cc, nav/camera_nav_test.cc,
  # workspace/dispatch_test.cc or tool/dispatch_test.cc at root if multi-module).
```

**Forbidden after land:** any public header at `src/tool/*.h` except none — only `BUILD.gn`, `README.md`, and optional root-level multi-module test `.cc` if kept. No `gestures.h` / `gestures.cc` filenames.

### 3.1 Include migration map

| Old | New |
| --- | --- |
| `tool/command.h` | `tool/command/command.h` |
| `tool/interaction.h` | `tool/interaction/interaction.h` |
| `tool/gestures.h` | `tool/draft/draft.h` |
| `tool/camera_nav.h` | `tool/nav/camera_nav.h` |
| `tool/workspace.h` | `tool/workspace/workspace.h` |
| `tool/legacy_msg.h` | `legacy/tool/adapter/msg.h` (was briefly `tool/adapter/…` / `legacy_msg.h`) |

Internal includes among modules use the same `tool/<module>/…` paths. `workspace.h` may continue to include `command`, `interaction`, and `draft` headers; prefer forward declarations where headers today only need pointers.

### 3.2 Known external include call sites (blast radius)

Must be rewritten in the landing change (non-exhaustive inventory from exploration; re-grep `tool/` at land time):

- `src/content/**` (`view_host`, `renderer_main`, tests, `plugin_host.h`)
- `src/app/views/**` (`map_scene`, `browser_view`, `scene3d_controller`, `map_hwnd_gestures`, `blit_frame_cache`, `plugin_shell`, tests)
- `src/ui/views/**`
- `src/plugin/**` (host, domain `*_commands`, python)
- `src/legacy/ui/xview/**`
- `src/legacy/tool/group/**` (endgame headers only: `Workspace`, `Draft`, `legacy_msg`)
- `src/tool/**` itself + tests

---

## 4. GN shape

```
//src/tool:command       source_set   (optional leaf)
//src/tool:interaction   source_set
//src/tool:draft         source_set   deps: :interaction
//src/tool:nav           source_set   (leaf)
//src/tool:workspace     source_set   deps: :command :interaction :draft :nav
                         + //src/gis/model/edit:edit
//src/tool:dispatch      group        public_deps → command/interaction/draft/nav/workspace
//src/legacy/tool/adapter:adapter
                         source_set   deps: //src/tool:command + :workspace
```

**Locked for callers:** prefer `deps = [ "//src/tool:dispatch" ]` unless a leaf-only consumer is clearly safer (e.g. a unit that only needs `nav` math) — leaf deps are allowed but not required in v1. Callers of `try_execute_gt_msg` / `command_id_from_gt_msg` also dep `//src/legacy/tool/adapter:adapter`.

**Tests:** keep under `//src/tool:…_test`; wire into existing `test_all` the same way as today.

**Dependency rules (unchanged product layering):**

- Endgame `src/tool/**` **must not** `#include "legacy/…"`.
- `//src/legacy/tool/group` **may** depend on `//src/tool:dispatch`.
- `nav` must not depend on `workspace` / `adapter` / `content` EventBus.

---

## 5. Public vs detail

| Public (`namespace tool`) | Internal |
| --- | --- |
| `CommandArgs`, `CommandCatalog`, `CommandDispatcher` | Catalog map storage details |
| `Interaction`, `InteractionRegistry`, `InteractionStack`, `InputRouter` | Always-on handler classes |
| `Draft`, `DraftKind`, `draft_flags`, `DraftCallback`, `make_select_*` / `make_draw_*` / `make_view_*` / `make_view3d_*` | Stroke state machines in `.cc` |
| `Workspace` | `register_builtins`, wheel always-on Interaction |
| `command_id_from_gt_msg`, `try_execute_gt_msg`, GT enum constants | — |
| `camera_nav` free functions / POD (`WorldExtent`, `BlitDestRect`, …) | — |

No third public namespace segment (`tool::draft` types stay in `tool`).

---

## 6. Migration phases (for the future plan — not started here)

| Phase | Work | Gate |
| --- | --- | --- |
| 0 | Spec approved (this file) | User review |
| 1 | Create module dirs; `git mv` sources; rename `gestures` → `draft`; update all includes + internal `#include`s | `rg` shows no `"tool/command.h"` / `"tool/gestures.h"` etc. (except archive docs) |
| 2 | Split / wire GN source_sets + `:dispatch` group; update `src/tool/README.md` + one-line `src-layout` Tool row | `ninja` / `build.bat` green for `dispatch` + tool tests + dependents that were touched |
| 3 | Run `tool_*_test` (+ existing ViewHost / SP1b coverage as already in tree) | Tests pass |

Behavior migration (SP1b) and leftover group thinning are **orthogonal**; if both are in flight, coordinate file ownership — this layout change should be mechanical.

---

## 7. Done when

1. Spec approved; implementation plan written under `docs/superpowers/plans/` (separate step).
2. Target tree matches §3; **zero** public `src/tool/*.h` umbrellas.
3. All product includes use `tool/<module>/…` per §3.1; no leftover `#include "tool/gestures.h"`.
4. `//src/tool:dispatch` still aggregates; still in `src_all`; no new tool DLL.
5. Leftover package layout: [`../archive/specs/2026-09-27-legacy-tool-subdirectory-layout-design.md`](../archive/specs/2026-09-27-legacy-tool-subdirectory-layout-design.md) (except `adapter/` already parked here).
6. `src/tool/README.md` and Tool row in `docs/build/src-layout.md` describe the new layout.
7. Tool unit tests green.

---

## 8. Spec self-review

| Check | Result |
| --- | --- |
| Placeholders | None (no TBD/TODO). |
| Contradictions | Explicitly supersedes archive “flat endgame headers”; does not reopen dispatch API semantics. |
| Scope | Layout + includes + GN only; legacy_tool and EventBus ownership out. |
| Ambiguity | Include break locked (no Phase-1 shims). `builtins.cc` optional. Test file rename to `draft_test` preferred but not ABI. |
| Nesting | Modules are `src/tool/<module>`; no deeper public taxonomy. |
| Namespace | `tool` + `tool::detail` only. |
