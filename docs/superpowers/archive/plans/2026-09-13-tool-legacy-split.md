<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Tool legacy split Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move leftover `SmtIATool` / `SmtGroupTool` from `src/tool` into `src/legacy/tool`, leave only session dispatch in `src/tool`.

**Architecture:** Same pattern as `legacy_render`: physical move + full include/GN rename, no forwarding headers; `src_all` keeps only `//src/tool:dispatch`.

**Tech Stack:** GN/Ninja, existing `smt_shared_library`, C++ leftover ABI unchanged.

## Global Constraints

- Stay on `master`; no new branch.
- No forwarding headers under `src/tool/`.
- `dll_stem` / export ABI unchanged.
- Do not rewrite leftover tool business logic.
- New/changed comments in English; user-facing docs in Chinese where applicable.
- Copyright year 2026 on touched Mogu headers.

---

### Task 1: Move leftover sources

**Files:**
- Move: `src/tool/t_*`, `src/tool/tool_export.h` → `src/legacy/tool/`
- Move: `src/tool/group/**` → `src/legacy/tool/group/`
- Create: `src/legacy/tool/BUILD.gn` (`smt_shared_library("tool")` + `group("legacy_tool_all")`)
- Rewrite: `src/tool/BUILD.gn` (dispatch + test only)

- [x] **Step 1:** `git mv` leftover files into `src/legacy/tool/` (preserve history)
- [x] **Step 2:** Write `src/legacy/tool/BUILD.gn`; update `group/BUILD.gn` deps to `//src/legacy/tool:tool` and `//src/tool:dispatch`
- [x] **Step 3:** Strip leftover sources from `src/tool/BUILD.gn`

### Task 2: Rewrite includes and GN deps

- [x] **Step 1:** Replace `"tool/t_` → `"legacy/tool/t_` and `"tool/group/` → `"legacy/tool/group/` across `src/`
- [x] **Step 2:** Replace GN labels `//src/tool:tool` → `//src/legacy/tool:tool`, `//src/tool/group:tool_group` → `//src/legacy/tool/group:tool_group`
- [x] **Step 3:** `src/BUILD.gn` `src_all`: drop `//src/tool:tool`, keep `//src/tool:dispatch`
- [x] **Step 4:** Root `BUILD.gn` `tool_group` group → new label; optionally expose `legacy_tool_all`

### Task 3: Docs

- [x] **Step 1:** Update `docs/build/src-layout.md`, `src/README.md`, `src/tool/README.md`
- [x] **Step 2:** Patch leftover path lines in `docs/superpowers/specs/2026-09-13-tool-event-dispatch-design.md`
- [x] **Step 3:** Add index row in `docs/README.md` for this spec/plan

### Task 4: Smoke

- [x] **Step 1:** Confirm no residual `"tool/t_` / `"tool/group/` under `src/`
- [x] **Step 2:** `ninja -C out src/tool:dispatch src/legacy/tool:tool` OK；`build.bat te` app 侧既有 ABI 断链与本轮无关（spec 允许）
