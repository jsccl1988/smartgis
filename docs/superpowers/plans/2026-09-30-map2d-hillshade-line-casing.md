<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Map2d hillshade + line casing — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make China 2D basemap show readable dual-stroke roads (casing then fill) and a DEM hillshade underlay when Style enables `hillshade` — capability-aligned with MapLibre look, **without** copying MapLibre Native.

**Architecture:** Extend existing StyleDocument → `ResolvedPaint` → `gis::vista` MapFrame layout → software/`vista/component/map` textured present. Line casing stays **two Style layers** (already in `default_carto_style_json`). Hillshade is **own** DEM slope/aspect → RGBA raster `DrawItem`, not a port of `hillshade_prepare` shaders. Bake clocks / CPU vs Thrust bench: living §DEM / hillshade bake profile.

**Tech Stack:** C++23, `gis::style`, `gis::vista`, `gis::DemRaster`, `vista/component/map`, `build.bat debug`, `map2d_china_loop` / `maplibre_align`.

**Spec:** [`../specs/2026-09-13-render-rhi-scene-design.md`](../specs/2026-09-13-render-rhi-scene-design.md) §Map2d richness P0 · §DEM / hillshade bake profile.  
**Diagram:** [`../diagrams/hillshade-bake-profile.html`](../diagrams/hillshade-bake-profile.html)

## Global Constraints

- Stay on **master**; do **not** `git commit` unless the user explicitly asks.
- No Qt; no `#include` mln/mbgl from product trees; no MapLibre Native product pin.
- New functions `snake_case`; public namespaces ≤ two layers; comments English.
- Copyright `Copyright (c) 2026 The Mogu Authors.` on new/touched files (year current).
- YAGNI: no heatmap / fill-extrusion / full expression engine in this plan.
- Do not invent a second DEM stack — reuse `gis::DemRaster` / china_dem.

---

## File map

| File | Role |
| --- | --- |
| `src/vista/component/map/default_style.cc` | Road casing/fill `minzoom` + cream-safe colors for overview |
| `third_party/maplibre/example/style_align.json` | Add `road-casing` before `road` for dual-still align |
| `src/gis/style/style_types.h` / `paint_resolve.*` | Hillshade paint constants on `ResolvedPaint` (or small fields) |
| `src/vista/component/world/terrain/` (new small helper next to dem) | `shade_dem_rgba(...)` — own Horn/finite-diff shade |
| `src/vista/component/map/layout.cc` / `layout.h` | Emit hillshade raster `DrawItem` under vectors; keep Style layer order for lines |
| `src/vista/component/map/map2d_test.cc` | Unit: casing order + hillshade item when DEM bound |
| `src/gis/style/style_test.cc` | Resolve hillshade paint keys |
| `testing/tools/harness/browser/browser.map2d.china/suite.json` | Pixel gate: road casing contrast; optional hillshade variance |
| `docs/superpowers/industry-gap-matrix.md` | One-line M1 richness note when landed |

---

### Task 1: Line casing overview readability

**Files:**
- Modify: `src/vista/component/map/default_style.cc` (`road-casing` / `road` minzoom + colors)
- Modify: `third_party/maplibre/example/style_align.json` (insert casing layer)
- Test: `src/vista/component/map/map2d_test.cc` (layer order / zoom match)
- Gate: `testing/tools/harness/browser/browser.map2d.china/` suite or existing china suite

**Interfaces:**
- Consumes: existing `default_carto_style_json()`, Style layer order in layout
- Produces: casing visible at china overview zoom (target minzoom ≤ 5, consistent with prior road visibility fix); fill narrower/lighter than casing

- [x] **Step 1:** Write/adjust failing test: at overview zoom (china frame), `layout` emits ≥1 casing line item then ≥1 road fill item for road source-layer (or assert Style layers both `layer_matches_zoom`).
- [x] **Step 2:** Run `build.bat debug` target for `map2d_test` (or the GN name that builds `map2d_test`) — expect FAIL.
- [x] **Step 3:** Lower `road-casing` / `road` `minzoom` (align with china overview; was 13); tune casing `#8a7040` / fill `#e0c06a` (or cream-safe equivalents) so casing is darker and wider.
- [x] **Step 4:** Add `road-casing` layer to `style_align.json` before `road` (same source filters as road).
- [x] **Step 5:** `frame_test` / `style_test` / `dem_raster_test` PASS; score_bmp road_casing gate added. Showcase BMP smoke deferred if china fixture path flaky. Do not commit.

---

### Task 2: Hillshade paint resolve (constants only)

**Files:**
- Modify: `src/gis/style/style_types.h` (`ResolvedPaint` hillshade fields)
- Modify: `src/gis/style/paint_resolve.cc` / `.h`
- Test: `src/gis/style/style_test.cc`

**Interfaces:**
- Consumes: Style layer `type: hillshade` paint map (string values)
- Produces: e.g. `hillshade_illumination_direction` (deg), `hillshade_exaggeration`, `hillshade_shadow_color`, `hillshade_highlight_color`, `hillshade_accent_color` (defaults matching MapLibre Spec **names**, own evaluation)

Paint key names may follow Style Spec strings for JSON compatibility; evaluation and shading math are **owned**.

- [x] **Step 1:** Failing test: parse minimal hillshade JSON → `resolve` → fields match constants.
- [x] **Step 2:** Run style unit test — FAIL.
- [x] **Step 3:** Implement resolve for constant paints only (no expression expansion beyond existing mini eval).
- [x] **Step 4:** PASS (`style_test OK`). Do not commit.

---

### Task 3: DEM → shade RGBA helper

**Files:**
- Create: `src/vista/component/world/terrain/hillshade.h` + `hillshade.cc` (colocated; name may be `dem_hillshade.*` if clearer)
- Modify: terrain `BUILD.gn` / `src/gis` BUILD as needed
- Test: `src/vista/component/world/terrain/dem/dem_raster_test.cc` or new `hillshade_test.cc`

**Interfaces:**
- Consumes: `const DemRaster&`, illumination azimuth/altitude, exaggeration, output w/h
- Produces: `bool shade_dem_rgba(..., std::vector<uint8_t>* rgba, int* w, int* h)` — grayscale or tinted RGBA, transparent where DEM nodata

Algorithm: finite-difference slope/aspect (Horn or equivalent) + Lambertian-ish shade. **Do not** copy MapLibre `hillshade_prepare` GLSL.

- [x] **Step 1:** Failing test on synthetic DEM: shaded buffer non-flat (variance > threshold); nodata stays alpha 0.
- [x] **Step 2:** Implement helper (`dem_hillshade.*`); `dem_raster_test` PASS. Do not commit.

---

### Task 4: MapFrame layout emits hillshade underlay

**Files:**
- Modify: `src/vista/component/map/layout.h` (optional DEM bind / hillshade cache key)
- Modify: `src/vista/component/map/layout.cc`
- Modify: present path that builds MapFrame (showcase / MapViewport) to bind DEM when available
- Test: `src/vista/component/map/map2d_test.cc`

**Interfaces:**
- Consumes: Style hillshade layer + bound `DemRaster` + view extent
- Produces: one `DrawKind` raster/textured item **before** vector fills/lines when hillshade layer matches zoom; skipped if no DEM

- [x] **Step 1:** Failing test: layout with hillshade Style + host `hillshade_tiles` → frame has shaded raster item under vectors.
- [x] **Step 2:** `LayoutInput::hillshade_tiles` + `emit_hillshade` (texture_key path; host supplies bake).
- [x] **Step 3:** Showcase / Map2dFrameCache binds china_dem when present (soft-fail if missing) — `load_gdal_raster` + `shade_dem_rgba` → `hillshade_tiles` + `load_raster` for GDI/GPU.
- [x] **Step 4:** Unit layout PASS (`frame_test`); product DEM bind landed. Do not commit.

---

### Task 5: Gates + docs

**Files:**
- Modify: `testing/tools/harness/browser/browser.map2d.china/suite.json`
- Optional: `testing/tools/harness/_shared/case/align/maplibre_align.py` expectations
- Modify: `docs/superpowers/industry-gap-matrix.md` (richness row)
- Spec § already landed; tick plan checkboxes when done

- [x] **Step 1:** Add pixel checks: `road_casing_frac` + `road_gold+casing` in `score_map2d_china`.
- [x] **Step 2:** Full china loop / align still — run when china_city fixture + Views BMP path confirmed. Tracked as gap-pin **P0-2** in [`2026-09-30-map2d-gap-pin.md`](2026-09-30-map2d-gap-pin.md) / matrix §8. `map2d.china` loop exit 0 (2026-09-30): `road_gold+casing` + `hillshade_soft_ok` green; fixtures via `//testing/data:china_map_samples` → `out/data`.
- [x] **Step 3:** industry-gap-matrix richness row updated. Soft hillshade variance gate added to `score_map2d_china`. Do not commit unless user asks.

---

### Task 6: Bake profile + equal-profile CPU vs Thrust bench

**Spec:** living §DEM / hillshade bake profile + bench · **Diagram:** [`../diagrams/hillshade-bake-profile.html`](../diagrams/hillshade-bake-profile.html)

**Files:**
- Modify: `src/vista/component/map/hillshade_bake.{h,cc}` (`HillshadeBakeSample` + phase clocks)
- Modify: `src/vista/component/world/terrain/process/dem_hillshade.*` (`BAKE_BACKEND`)
- Modify: `src/vista/component/world/terrain/process/land_mask.*` (`LandMaskBakeSample`)
- Modify: `src/content/browser/present/map2d/frame/map2d_layout_build.cc` (`cat=bake` not `startup`)
- Modify: `src/app/views/harness/showcase/map2d/present/fps_bench.cc` (echo bake sample)
- Test: `dem_raster_test` / `land_mask_test` when `BAKE_BENCH=1`
- Create: `testing/tools/harness/browser/run_hillshade_bake_bench.py`

Locked profile: `china_dem`, `max_edge=768`, illumination 335/32, exaggeration 0.5. CUDA optional.

- [x] **Step 1:** Living § + HTML diagram + this checklist (no twin plan).
- [x] **Step 2:** Instrument `bake_hillshade_slot` / `shade_dem_rgba` / `fill_lonlat_mask`; `cat=bake` spans.
- [x] **Step 3:** Equal-profile bench in existing test exes + harness JSON/table under `captures/analysis/hillshade_bake/`.
- [ ] **Step 4:** Run `py -3 testing/tools/harness/browser/run_hillshade_bake_bench.py` on a machine with `china_dem` (and CUDA when `has_cuda`); paste table. Optional — agent loop / later.

---

## Done when

1. China overview roads show casing then fill (Style dual layers), readable on cream land.
2. With DEM fixture: hillshade underlay draws from Style `hillshade` layer via owned shade helper.
3. Unit tests + map2d china (and align product still) green; **no** MapLibre Native link in product.
4. Living § success criteria met.

## Out of scope (later phases)

- Heatmap, fill-extrusion → mesh, SDF lines/glyphs, collision index, full `interpolate` expression engine.
