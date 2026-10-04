<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# world3d True Earth (完整真3D) — Implementation Plan

> **For agentic workers:** Stay on **master**. Do **not** create a parallel `earth3d` package. Spec: [`../specs/2026-09-13-plugin-host-design.md`](../specs/2026-09-13-plugin-host-design.md) §world3d True Earth. Render stack: [`../specs/2026-09-13-render-rhi-scene-design.md`](../specs/2026-09-13-render-rhi-scene-design.md).

**Goal:** Ship a Google-Earth-class **MVP product face** on `smartgis.world3d`: DEM terrain (China sample + optional global GeoTIFF) + atmosphere (sky/ocean/cloud/fog) + satellite cloud cover (field or procedural) + orbit fly-to + optional city 3D Tiles attach + existing pointcloud hook, demoed via plugin showcase BMP.

**Architecture:** Approach **A** — extend `product/world3d` + `World3dSceneWriter` seams; Browser installs `open_earth` / `load_global_dem` / `set_satellite_cloud` / `set_atmosphere` / `fly_to` / `attach_tileset`; compose existing `apply_china_scene3d_product_defaults`, `AtmosphereSession::load_fields`, `gis::set_sample_dem_path_override`, `Scene3dGpuPresent::attach_tileset_json`, orbit extent framing. No Cesium Native; no new UI toolkit.

**Tech Stack:** C++23, Views shell, Scene3dPresenter, AtmosphereSession, TilesetStreamSession, GN/`build.bat debug`.

## Global Constraints

- Stay on `master`; compile via `build.bat` + `out/.build.lock`; set `SMARTGIS_BUILD_OWNER`.
- Two-level namespaces (`plugin`); functions `snake_case`; English comments; Mogu copyright 2026.
- No Qt; no new dated `*-design.md`; revise living umbrella § only.
- Do not invent a second product id — keep `smartgis.world3d`.

## Assumptions (user unavailable — defaulted)

1. Product package stays **`world3d`** (not `earth` / `globe`).
2. MVP = China framing + atmosphere ON + fly_to + optional m3 tileset fixture + showcase capture (not global Ion / spherical WGS84 globe mesh).
3. Planetary “globe ball” is a documented gap; orbit-normalized China DEM is the shippable true-3D browse today; optional global GeoTIFF upgrades via `load_global_dem`.
4. Satellite cloud without GeoTIFF uses procedural atmosphere clouds (honest stand-in).

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
- [x] Step 3: Document run: `SmartGIS.exe --plugin-showcase=world3d`
- [ ] Step 4: Live GPU BMP green — **blocked 2026-09-30**: `SmartGIS.exe --plugin-showcase=*` currently exits `0xC0000409` (GS) during `Browser::init` before marks (also repro on `--plugin-showcase=mine`; not specific to Earth path). Unit `world3d_scene_writer_test` green.

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

### Task 5 — P0b global DEM + satellite cloud + atmosphere toggles (2026-10-02)

**Files:**
- Modify: `src/plugin/product/world3d/commands.h` / `scene_commands.cc` / `manifest/plugin.json` / `README.md` / `BUILD.gn`
- Modify: `src/app/views/shell/browser/plugin/analysis_writer_world3d.cc`
- Modify: `src/vista/world/terrain/dem/dem_raster.{h,cc}` (`set_sample_dem_path_override`)
- Modify: `src/content/browser/present/scene3d/frame/terrain_mesh.cc`
- Modify: living § under plugin-host + render-rhi-scene

- [x] Step 1: `World3dSceneWriter::{load_global_dem,set_satellite_cloud,set_atmosphere}` + command/processing ids
- [x] Step 2: Browser writers; China / procedural stand-ins with JSON hints when data missing
- [x] Step 3: DEM path override honored by Scene3d terrain rebuild
- [x] Step 4: Unit test coverage in `world3d_scene_writer_test`
- [x] Step 5: Live GPU BMP with optional `out/data/global_dem.tif` / `global_terrain.png` / `satellite_cloud.tif` (data via `testing/data/build_globe_terrain.py`)

**Done when:** processing ids green with/without writer; missing data returns structured stand-in JSON.

---

## Verify

```bat
set SMARTGIS_BUILD_OWNER=world3d-true-earth
build.bat debug src/plugin/product/world3d:world3d_scene_writer_test
build.bat debug views
REM out\Debug\SmartGIS.exe --plugin-showcase=world3d
```

## Non-goals (locked)

- Cesium Native / global terrain Ion
- Full WGS84 spherical globe mesh
- New product package id
- Web GIS
