<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Tool dispatch + event/operation split Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Land session-scoped Command / Input / EditSession / EventBus so new map chrome does not go through `SMT_POST_IATOOL_MSG`.

**Architecture:** `content::EventBus` is typed pub/sub. `tool::Workspace` owns catalog, dispatcher, interaction stack, and input router (per view). `sdb::MemoryEditSession` is the v1 write log. Leftover `SmtIATool` DLLs stay; `command_id_from_gt_msg` is the only bridge.

**Tech Stack:** C++20, GN/Ninja (`build.bat`), `testing/test.gni` `expect`/`main`. No Qt, no extra event libraries.

## Global Constraints

- Work on `master` only. Do not create branches or worktrees.
- Do not `git commit` unless the user explicitly asks.
- Copyright: `Copyright (c) 2026 The Mogu Authors.` on every new engineering file; bump year on touched Mogu headers.
- Public namespaces: two levels (`content`, `tool`, `sdb`). Deeper = `detail` or anonymous.
- New functions `snake_case`. Types PascalCase.
- Comments in English. Class purpose in one or two sentences.
- No Qt. No HWND / `SmtMap*` / `LPRENDERDEVICE` on new public headers.
- No new `*Manager` singleton. No EventBus under `src/tool`.
- Product C++20. Output only under repo-root `out/`.
- Exact APIs: copy from `docs/superpowers/specs/2026-09-13-tool-event-dispatch-design.md`.
- Leftover `SmtToolCore` / `SmtGroupToolCore` keep compiling unchanged.

## File map

| Path | Responsibility |
| --- | --- |
| `src/content/public/event_bus.h` | Typed EventBus |
| `src/content/public/events.h` | SelectionChanged / ExtentChanged |
| `src/tool/command.h` `.cc` | Catalog + Dispatcher |
| `src/tool/interaction.h` `.cc` | Interaction, registry, stack, router, always-on |
| `src/tool/workspace.h` `.cc` | Per-view composition root |
| `src/tool/legacy_msg.h` `.cc` | GT_MSG_* → command id |
| `src/sdb/edit/edit_session.h` `.cc` | EditSession + MemoryEditSession |
| `src/tool/dispatch_test.cc` | One test exe |
| `src/tool/BUILD.gn` | `source_set("dispatch")` + test |
| `src/sdb/edit/BUILD.gn` | `source_set("edit")` |
| `src/sdb/BUILD.gn` / `src/BUILD.gn` / root `BUILD.gn` | wire groups + `test_all` |
| docs listed in the spec | layout accuracy |

Tasks 1–3 have **no shared files** and may run in parallel. Task 4 depends on 1–3. Task 5 is docs.

---

### Task 1: EventBus + domain events

**Files:**
- Create: `src/content/public/event_bus.h`
- Create: `src/content/public/events.h`
- Modify: `src/content/BUILD.gn` (add headers to `source_set("content")` sources)

**Interfaces:**
- Consumes: `content::FeatureId`, `Extent2` from `map_types.h`
- Produces: spec `EventBus` / `Connection` / `SelectionChanged` / `ExtentChanged`

- [x] **Step 1: Add `events.h` and `event_bus.h` as specified in the spec.** Connection is move-only. `publish` copies the matching slot list before invoking. Empty `std::function` subscribe returns a disengaged Connection.

- [x] **Step 2: Add both headers to `//src/content:content` sources** so they ship with the public ABI set.

---

### Task 2: Command catalog + dispatcher

**Files:**
- Create: `src/tool/command.h`
- Create: `src/tool/command.cc`

**Interfaces:**
- Consumes: none
- Produces: `tool::CommandArgs`, `CommandCatalog::add/find/contains`, `CommandDispatcher::execute`

- [x] **Step 1: Implement catalog as `std::map<std::string, CommandHandler>`.** Empty or duplicate id → `add` false. `execute` false if missing.

---

### Task 3: EditSession (memory)

**Files:**
- Create: `src/sdb/edit/edit_session.h`
- Create: `src/sdb/edit/edit_session.cc`
- Create: `src/sdb/edit/BUILD.gn`

**Interfaces:**
- Consumes: `content::FeatureId`
- Produces: `sdb::EditSession` / `MemoryEditSession`

- [x] **Step 1: `commit` rejects `id.len == 0`.** Undo pops committed → undo stack conceptually: keep `done_` and `redo_` vectors. `commit` clears `redo_`.

---

### Task 4: Interaction, Workspace, legacy ids, test, GN, docs

**Files:**
- Create: `src/tool/interaction.h` `.cc`
- Create: `src/tool/workspace.h` `.cc`
- Create: `src/tool/legacy_msg.h` `.cc`
- Create: `src/tool/dispatch_test.cc`
- Modify: `src/tool/BUILD.gn`
- Modify: `src/sdb/BUILD.gn`, `src/BUILD.gn`, `BUILD.gn`
- Modify: `docs/README.md`, `docs/build/src-layout.md`, `src/README.md`

**Interfaces:**
- Consumes: Tasks 1–3 types
- Produces: `Workspace::execute/activate/dispatch_input`, `command_id_from_gt_msg`, `tool_dispatch_test`

- [x] **Step 1: Always-on `WheelZoom` (handle `kWheel` only) and `HoverCursor` (`kMouseMove`, return false).**
- [x] **Step 2: `Workspace` ctor registers v1 command ids** as in the spec table. Commands that activate tools call `stack.activate`. `selection.clear` publishes empty `SelectionChanged` when `events` is non-null.
- [x] **Step 3: `legacy_msg.cc` includes `group/defs.h` and maps CMD constants.**
- [x] **Step 4: `dispatch_test.cc` covers the spec Testing list.**
- [x] **Step 5: Wire `//src/tool:dispatch` into `src_all` via `//src/tool:tool` public_dep or a parallel dep; add `//src/sdb/edit:edit` to `src_all` and `sdb` group. Add test to `//:test_all`.**
- [x] **Step 6: Run `build.bat te` (or ninja the new test) and confirm `tool_dispatch_test: ok`.**

Do not commit unless the user asks.
