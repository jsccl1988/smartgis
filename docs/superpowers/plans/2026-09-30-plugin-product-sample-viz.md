<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Plugin product sample + visualization — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Each `src/plugin/product` builtin loads shipped sample data and paints a visible result through the same processing/command ids users run.

**Architecture:** Approach C from plugin-host §product sample + visualization — Browser writers + `testing/data/plugin/` fixtures + interact suites. `model3d` folds into `world3d`; orthogrid/print keep their trees.

**Tech Stack:** GN/Ninja (`build.bat debug`), Views shell `SmartGisViews.exe`, interact DSL, `MapScene::add_triangle_layer`, plugin ProcessingPool.

**Spec:** [`../specs/2026-09-13-plugin-host-design.md`](../specs/2026-09-13-plugin-host-design.md) §product sample + visualization

## Global Constraints

- Stay on `master`; enter compile via `build.bat` + `out/.build.lock`.
- Product namespaces two levels (`plugin`); functions `snake_case`; comments English.
- No Qt; no new dated design specs — revise living umbrella only.
- Do not delete leftover `*.am` trees this plan.
- Processing factories must not touch Views.

---

### Task 0 — Merge `product/model3d` into `product/world3d`

**Files:**
- Modify: `src/plugin/product/world3d/commands.{h,cc}`, `manifest/plugin.json`, `BUILD.gn`
- Move/adapt: model3d scene-writer logic into world3d (`World3dSceneWriter`)
- Modify: `plugin_shell.cc`, `host/tests/host_test.cc`, app `BUILD.gn` deps
- Delete: `src/plugin/product/model3d/` after call sites updated
- Update: `cmd.h` / AM aliases — keep `model3d.*` catalog ids as aliases that hit world3d handlers

- [x] Step 1: Port `Model3dSceneWriter` → `World3dSceneWriter` on world3d; register both `world3d.*` and legacy `model3d.*` command/processing ids from `register_world3d`
- [x] Step 2: Remove `register_model3d` from `PluginShell::start_builtins`; drop `model3d_views` deps
- [x] Step 3: Update `plugin_host_test` + `scene_writer_test` (live under world3d/tests)
- [x] Step 4: `build.bat debug src/plugin/product/world3d:world3d_loader_test` + `plugin_host_test` green
- [x] Step 5: Delete `src/plugin/product/model3d/`

**Done when:** only one builtin (`smartgis.world3d`) owns DEM + former model3d commands; tests green.

---

### Task 1 — Sample fixtures + world3d interact

**Files:**
- Create: `testing/data/plugin/world3d_tin_sample.xyz` (small CRS84 XYZ)
- Create: `testing/tools/harness/plugin/plugin.world3d/plugin.world3d.il` + `testing/tools/harness/plugin/plugin.world3d/suite.json`
- Modify: `testing/data/BUILD.gn` to copy fixtures → `out/data/plugin/`
- Optional: default paths in world3d dialogs when env `SMT_PLUGIN_SAMPLE_DIR` set

- [x] Step 1: Add mini XYZ + document that grid uses `china_dem.tif`, pointcloud uses `pointcloud_public_sample.txt`
- [x] Step 2: Interact: `run_processing(world3d.grid_from_heightmap)` + `world3d.tin_from_xyz` + `world3d.add_pointcloud` with absolute paths under `out/data`
- [x] Step 3: Assert marks / non-empty BMP (reuse map2d score or plugin-specific marks)
- [x] Step 4: `build.bat e2e` suite `plugin.world3d` (or `te` path) green

**Done when:** one command line / suite proves tin+grid+pointcloud paint.

---

### Task 2 — Browser installs `set_world3d_scene_writer`

**Files:**
- Modify: `src/app/views/shell/browser/browser.cc`
- Possibly: thin helpers to ingest pointcloud txt → map points / triangle stand-ins; sphere/water P0 stand-ins

- [x] Step 1: Wire scene writer next to surface writer in `Browser::init`
- [x] Step 2: `add_pointcloud` reads sample path and adds drawable layer(s)
- [x] Step 3: Unit/host test: with writer set, `world3d.add_sphere` (or alias) returns true
- [x] Step 4: Rebuild Views; confirm interact from Task 1 still green

**Done when:** no `no_scene_device` on wired product path for pointcloud + one primitive.

---

### Task 3 — print + orthogrid plugin paths

**Files:**
- Create: `testing/tools/harness/plugin/plugin.print/plugin.print.il`, `plugin.orthogrid.il` (+ suites)
- Modify: orthogrid processing to commit mesh to `MapScene` (writer or document seam) if not already
- Modify: print preview to bind current map content (verify MapPreviewView feed)

- [x] Step 1: print suite — seed china map → `print.preview` → mark/BMP
- [x] Step 2: orthogrid suite — load boundary fixture → `baogrid.create_orth_grid` → map lines visible (distinct from showcase-only path)
- [x] Step 3: e2e green for both suites

**Done when:** all three product trees have a green sample+viz suite.

---

## Execution notes

- Prefer `build.bat debug …` while iterating; set `SMARTGIS_BUILD_OWNER`.
- Order: Task 0 → 2 → 1 → 3 (writer before interact that needs scene).
- Flood inundation is **out of scope** for this plan.
