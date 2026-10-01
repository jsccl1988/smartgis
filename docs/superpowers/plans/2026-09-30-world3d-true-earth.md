<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# world3d True Earth (完整真3D) — Implementation Plan

> **For agentic workers:** Stay on **master**. Do **not** create a parallel `earth3d` package. Spec: [`../specs/2026-09-13-plugin-host-design.md`](../specs/2026-09-13-plugin-host-design.md) §world3d True Earth. Render stack: [`../specs/2026-09-13-render-rhi-scene-design.md`](../specs/2026-09-13-render-rhi-scene-design.md).

**Goal:** Ship a Google-Earth-class **MVP product face** on `smartgis.world3d`: China-scale DEM terrain + atmosphere (sky/ocean/cloud/fog) + orbit fly-to + optional city 3D Tiles attach + existing pointcloud hook, demoed via plugin showcase BMP.

**Architecture:** Approach **A** — extend `product/world3d` + `World3dSceneWriter` seams; Browser installs `open_earth` / `fly_to` / `attach_tileset`; compose existing `apply_china_scene3d_product_defaults`, `Scene3dGpuPresent::attach_tileset_json`, orbit extent framing. No Cesium Native; no new UI toolkit.

**Tech Stack:** C++23, Views shell, Scene3dPresenter, AtmosphereSession, TilesetStreamSession, GN/`build.bat debug`.

## Global Constraints

- Stay on `master`; compile via `build.bat` + `out/.build.lock`; set `SMARTGIS_BUILD_OWNER`.
- Two-level namespaces (`plugin`); functions `snake_case`; English comments; Mogu copyright 2026.
- No Qt; no new dated `*-design.md`; revise living umbrella § only.
- Do not invent a second product id — keep `smartgis.world3d`.

## Assumptions (user unavailable — defaulted)

1. Product package stays **`world3d`** (not `earth` / `globe`).
2. MVP = China framing + atmosphere ON + fly_to + optional m3 tileset fixture + showcase capture (not global Ion / spherical WGS84 globe mesh).
3. Planetary “globe ball” is a documented gap; orbit-normalized China DEM is the shippable true-3D browse today.

---

### Task 1 — Writer API + commands / processing

**Files:**
- Modify: `src/plugin/product/world3d/commands.h`
- Modify: `src/plugin/product/world3d/scene_commands.cc`
- Modify: `src/plugin/product/world3d/manifest/plugin.json`
- Modify: `src/plugin/product/world3d/tests/scene_writer_test.cc`

- [x] Step 1: Add `World3dSceneWriter::{open_earth,fly_to,attach_tileset}`
- [x] Step 2: Register `world3d.open_earth` / `world3d.fly_to` / `world3d.attach_city_tileset` (command + processing)
- [x] Step 3: Unit test: refuse without writer (`no_scene_device`); succeed with stubs
- [x] Step 4: `build.bat debug src/plugin/product/world3d:world3d_scene_writer_test` (exit 0)

**Done when:** processing ids green with/without writer.

---

### Task 2 — Browser seams

**Files:**
- Modify: `src/app/views/shell/browser/browser.cc` (world3d writer install only)

- [x] Step 1: `open_earth` → select Scene3D tab + `apply_china_scene3d_product_defaults`
- [x] Step 2: `fly_to(lon,lat,distance,span_deg)` → local orbit extent + distance
- [x] Step 3: `attach_tileset(path)` → read JSON → `scene3d()->gpu().attach_tileset_json` (+ content root)

**Done when:** interactive menu / `run_processing` drives Scene3D Earth browse without crash.

---

### Task 3 — Plugin showcase Earth path

**Files:**
- Modify: `src/app/views/shell/harness/showcase/plugin/plugin_showcase.cc`
- Create: `src/plugin/product/world3d/README.md`

- [x] Step 1: Showcase uses atmosphere product defaults (not land-only off) + DEM orbit + pointcloud overlay
- [x] Step 2: Best-effort attach `m3_city_tileset.json`; mark `earth-atmo` / `earth-tiles` / `bmp-ok`
- [x] Step 3: Document run: `SmartGisViews.exe --plugin-showcase=world3d`
- [ ] Step 4: Live GPU BMP green — **blocked 2026-09-30**: `SmartGisViews.exe --plugin-showcase=*` currently exits `0xC0000409` (GS) during `Browser::init` before marks (also repro on `--plugin-showcase=mine`; not specific to Earth path). Unit `world3d_scene_writer_test` green.

**Done when:** GPU path writes `plugin-showcase-world3d.bmp` with visible signal + atmosphere marks.

---

### Task 4 — Docs

**Files:**
- Modify: `docs/superpowers/specs/2026-09-13-plugin-host-design.md` (§world3d True Earth)
- Modify: `docs/superpowers/README.md` (Active plan link)
- Optional note under render-rhi-scene Atmosphere / tiles

- [x] Step 1: Living § + this plan linked
- [x] Step 2: Honest gap list vs Google Earth in § Non-goals / Remaining

---

## Verify

```bat
set SMARTGIS_BUILD_OWNER=world3d-true-earth
build.bat debug src/plugin/product/world3d:world3d_scene_writer_test
build.bat debug SmartGisViews
REM out\Debug\SmartGisViews.exe --plugin-showcase=world3d
```

## Non-goals (locked)

- Cesium Native / global terrain Ion
- Full WGS84 spherical globe mesh
- New product package id
- Web GIS
