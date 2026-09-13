<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Plugin full upgrade Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Wire SmartGisViews to Registry/PluginHost (builtins + ManagerView), retire MFC dialogs from the Views path, align `src/plugin` style/ABI on the new path, and replace domain processing stubs with real kernels.

**Architecture:** `BrowserView` owns `app::PluginChrome` (separate TU to avoid dual `content::MapContents` headers). Five builtin `register_*` hooks start on launch. DEM loaders become MFC-free for `dem_views`. Parallel lanes edit disjoint trees on `master`.

**Tech Stack:** C++23, GN/Ninja (`out/` only), existing `plugin_host_test`, no Qt, no new Manager singleton.

## Global Constraints

- Work on `master` only. Do **not** `git commit` unless the user explicitly asks.
- Copyright `Copyright (c) 2026 The Mogu Authors.` on new files; bump year on touched Mogu headers.
- Public namespaces ≤2 (`plugin`, `content`, `app`). New functions `snake_case`. Comments English.
- Spec: `docs/superpowers/specs/2026-09-14-plugin-full-upgrade-design.md`.
- Do not change MFC `InitSmtAuxModules`. Do not auto-scan zip/`*.am` (S1).
- Do not include both `content/public/map_contents.h` and `content/public/plugin_host.h` in the same TU.

## File map

| Path | Responsibility |
| --- | --- |
| `src/app/views/plugin_chrome.h` `.cc` | Registry + PluginHost + pool + builtins + Manager widget |
| `src/app/views/browser_view.*` | Own PluginChrome; Plugins menu |
| `src/app/views/BUILD.gn` | deps |
| `src/plugin/dem/tin_loader_core.*` (or rewrite `tin_loader.cpp` without stdafx) | MFC-free XYZ→TIN |
| `src/plugin/dem/grid_loader_core.*` | MFC-free heightmap→grid |
| `src/plugin/dem/dem_commands.cc` | Real processing factories |
| `src/plugin/dem/BUILD.gn` | `dem_loaders` source_set + `dem_views` deps |
| `src/plugin/proj/*` | Enable PROJ in Views; tests |
| `src/plugin/{model3d,orthogrid,print}/*` | Null-safe deepen |
| `docs/README.md` / `docs/build/src-layout.md` | Index + layout row |

**Parallelism:** Task 1 (chrome) ∥ Task 2 (DEM loaders+commands) ∥ Task 3 (proj/model3d/orthogrid/print) ∥ Task 4 (docs). Task 5 integrate + build.

---

### Task 1: PluginChrome + BrowserView (Phase A)

**Files:**
- Create: `src/app/views/plugin_chrome.h`
- Create: `src/app/views/plugin_chrome.cc`
- Modify: `src/app/views/browser_view.h`
- Modify: `src/app/views/browser_view.cc`
- Modify: `src/app/views/BUILD.gn`

**Interfaces:**
- Consumes: `content::create_plugin_host`, `plugin::Registry`, `plugin::ProcessingPool`, `plugin::register_{dem,proj,print,model3d,orthogrid}`, `plugin::ManagerView`, `content::ViewHost::events`
- Produces: `app::PluginChrome` with `bool init(content::EventBus* events)`, `void shutdown()`, `bool show_manager()`, `content::PluginHost* host()`, `plugin::Registry* registry()`

- [x] **Step 1: Add `plugin_chrome.h`**
- [x] **Step 2: Implement `plugin_chrome.cc`**
- [x] **Step 3: BrowserView**
- [x] **Step 4: BUILD.gn**
- [x] **Step 5: Build** `build.bat views` → `SmartGisViews.exe` linked (2026-09-14). Do not commit.

---

### Task 2: DEM MFC-free loaders + real processing (Phases B+D+C dem)

**Files:**
- Create or rewrite: MFC-free loader TUs under `src/plugin/dem/`
- Modify: `src/plugin/dem/dem_commands.cc`
- Modify: `src/plugin/dem/tin_loader.h` / `grid_loader.h` (rename `SmtTinFileFmt` → `TinFileFmt` on Views path)
- Modify: `src/plugin/dem/BUILD.gn`
- Modify: `src/plugin/host_test.cc` (one processing test if feasible)

**Interfaces:**
- Consumes: `algorithm/tin`, existing loader semantics
- Produces: `plugin::load_ascii_xyz_tin` / `load_heightmap_grid` callable from `dem_views` without MFC

- [x] **Step 1:** MFC-free `tin_loader_core.cc` / `grid_loader_core.cc` + `dem_loaders` source_set.
- [x] **Step 2:** `dem_commands.cc` real processing factories (stubs removed).
- [x] **Step 3:** `BUILD.gn` — `dem_loaders` → `dem_views` / `plugin_dem`.
- [x] **Step 4:** `dem_loader_test` + `plugin_host_test` (integrate verify 2026-09-14).

---

### Task 3: Proj / model3d / orthogrid / print deepen (Phase D)

**Files:**
- `src/plugin/proj/**`
- `src/plugin/model3d/**`
- `src/plugin/orthogrid/**`
- `src/plugin/print/**`

**Do not edit** `src/app/views/**` or `src/plugin/dem/**`.

- [x] **Step 1: Proj** — Views `PLUGIN_PROJ_VIEWS_USE_PROJ_API`; harden empty/non-finite; host_test `proj_factory` green.
- [x] **Step 2: model3d** — null scene/device → false; contribute guards.
- [x] **Step 3: orthogrid** — keep `smartgis.baogrid`; boundary file check + Laplace smoke; no MessageBox on load/save.
- [x] **Step 4: print** — Views `PrintPreviewDialog` only.
- [x] **Step 5:** `plugin_host_test` ok (Task 3 agent).

---

### Task 4: Docs index (all phases)

**Files:**
- Modify: `docs/README.md`
- Modify: `docs/build/src-layout.md` plugin row

- [x] **Step 1:** Index `2026-09-14-plugin-full-upgrade-design.md` + this plan.
- [x] **Step 2:** Layout row: Views owns Registry; MFC `*.am` leftover; orthogrid tree name.
- [x] **Step 3:** Refresh **最后更新** to 2026-09-14 on touched docs.

---

### Task 5: Integrate + verify

- [x] **Step 1:** `plugin_host_test` + `dem_loader_test` ok (2026-09-14).
- [x] **Step 2:** `SmartGisViews` relink ok after domain D.
- [x] **Step 3:** Cross-lane conflicts none for Views deps.
- [ ] **Step 4:** Mark plan checkboxes; leave commit to user.

## Self-review

- Spec A/B/C/D each have tasks (1 / 2 / 2+3 / 2+3).
- No TBD placeholders.
- Commit steps omitted (user must request commits).
