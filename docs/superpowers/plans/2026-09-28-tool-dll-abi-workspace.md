<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Tool DLL ABI + Workspace composition — plan

> **For agentic workers:** Implement task-by-task. Design: [`../specs/2026-09-13-tool-event-dispatch-design.md`](../specs/2026-09-13-tool-event-dispatch-design.md) §DLL ABI + Workspace composition.

**Goal:** Keep `tool.dll`, eliminate C4251 via pimpl + method-level export, thin `Workspace` internals into `DraftPipeline` + `NavBridge`.

**Architecture:** Exported **methods** (not classes) for types that hold `unique_ptr<Impl>` — avoids MSVC C4251. `Workspace::Impl` owns command/interaction graph plus `detail::DraftPipeline` / `detail::NavBridge`. No pragma auto-link.

**Tech Stack:** C++23, GN `smt_shared_library`, MSVC `__declspec`.

## Global Constraints

- Stay on `master`; no new branch
- `TOOL_EXPORT` / `TOOL_EXPORTS` only (no `SMT_TOOL_*`)
- Public namespaces ≤2 layers; helpers in `tool::detail`
- Comments in English

---

## Task 1: pimpl CommandCatalog + Interaction* + drop pragma

- [x] `command.h` / `command.cc` — `CommandCatalog` pimpl + method export
- [x] `interaction.h` / `interaction.cc` — Registry / Stack / InputRouter pimpl + method export
- [x] `tool_export.h` — remove `#pragma comment(lib, …)`

## Task 2: DraftPipeline + NavBridge + Workspace pimpl

- [x] Add `workspace/draft_pipeline.{h,cc}`, `workspace/nav_bridge.{h,cc}`
- [x] Rewrite `workspace.h` / `workspace.cc` as pimpl composing the two
- [x] Update `BUILD.gn` sources

## Task 3: Callers + docs + verify

- [x] Call sites keep `catalog()` / `stack()` accessors (no ViewHost API break)
- [x] `src/tool/README.md`, abi-rename-map TOOL wording, living §
- [x] `build.bat debug tool_dispatch_test draft_test camera_nav_test` — green; no `tool::` C4251
