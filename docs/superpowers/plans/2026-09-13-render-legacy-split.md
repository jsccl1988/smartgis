<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `legacy_render` split Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move leftover render engines from `src/render/` into `src/legacy_render/`, rewrite includes/GN labels (no shims), keep `src_all` on endgame render only.

**Architecture:** New layer `src/legacy_render/<module>`. Full Bridge + leftover_* live in `legacy_render/bridge`. Endgame `src/render` keeps `rhi` / GpuScene / `skia` / `math` + thin `SmtRender` stub. Optional `//src/legacy_render:legacy_render_all` is not in `src_all`.

**Tech Stack:** C++23 product, GN/Ninja (`build.bat`), existing leftover `dll_stem`.

## Global Constraints

- Work on `master` only. Do not create branches or worktrees.
- Do **not** `git commit` unless the user explicitly asks.
- Copyright: `Copyright (c) 2026 The Mogu Authors.` on new/touched Mogu engineering files.
- Nesting: `src/<layer>/<module>` only. No `src/render/leftover/...`.
- No Qt. No D3D9. No forward headers under old `src/render/<leftover>` paths.
- `dll_stem` / `Smt_*` ABI unchanged where possible.
- Output only under repo-root `out/`.
- Spec: `docs/superpowers/specs/2026-09-13-render-legacy-split-design.md`.

## File map

| Path | Responsibility |
| --- | --- |
| `src/legacy_render/{gdi,gdi_simple,gl,render3d,scene3d,model3d,terrain,pointcloud}/` | Moved leftover modules |
| `src/legacy_render/bridge/` | renderdevice, renderer, leftover_session, leftover_mesh, leftover_record + tests |
| `src/legacy_render/BUILD.gn` | `legacy_render_all` group |
| `src/render/BUILD.gn` | Lean `render_all` + thin `render` stub |
| `src/render/scene/` | GpuScene only |
| Call sites under `src/{plugin,ui,tool,app,sdb,...}` | Include + deps → `legacy_render` |
| `docs/build/src-layout.md` + umbrella spec §6.4 | Path accuracy |

---

### Task 1: `git mv` modules + bridge

**Files:** create `src/legacy_render/`; move directories listed in spec; move bridge sources from `src/render` and `src/render/scene`.

- [ ] **Step 1:** `mkdir src/legacy_render` and `git mv` leftover module dirs.
- [ ] **Step 2:** `git mv` bridge sources into `src/legacy_render/bridge/`.
- [ ] **Step 3:** Confirm `src/render` only has endgame dirs + thin stubs to write later.

---

### Task 2: Rewrite includes and GN labels

**Files:** all `#include "render/(gdi|…)"` and `//src/render/(gdi|…)` → `legacy_render` / `//src/legacy_render/…`; internal includes inside moved trees; bridge includes.

- [ ] **Step 1:** Bulk rewrite include strings and BUILD.gn deps.
- [ ] **Step 2:** Fix `legacy_render/*/BUILD.gn` paths and deps among themselves.
- [ ] **Step 3:** Wire `src/legacy_render/BUILD.gn` `legacy_render_all`.

---

### Task 3: Slim endgame `src/render`

**Files:** `src/render/BUILD.gn`, thin stub sources if needed, `src/render/scene/BUILD.gn` without leftover_*, `src/BUILD.gn` / root `BUILD.gn` test wiring.

- [ ] **Step 1:** `render_all` = rhi + scene(GpuScene) + math (+ skia opt-in stays out).
- [ ] **Step 2:** Thin `smt_shared_library("render")` without legacy deps (or drop if unused — prefer stub that still exports minimal symbols if LoadLibrary expects stem).
- [ ] **Step 3:** Move leftover_* tests out of root `test_all` or point at `legacy_render` labels; keep `rhi_test` / `scene_gpu_test` / `unified_draw_test` on endgame.

---

### Task 4: Docs

**Files:** `src-layout.md`, model-render-compute §6.4, `src/render/README.md`, optional root README, this plan checkboxes.

- [ ] **Step 1:** Update path tables to `legacy_render`.
- [ ] **Step 2:** Note present seam may be broken this pass.

---

### Task 5: Verify

- [ ] **Step 1:** `.\build.bat` (or `build.bat te` focused) — `src_all` / endgame render green.
- [ ] **Step 2:** Optionally try `ninja -C out legacy_render_all` — fix only blockers that prevent the group from loading; app graph may still fail.
