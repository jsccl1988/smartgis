<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

> **Status: archived checklist** (2026-09-28 merge B). Open work continues on the living umbrella in docs/superpowers/specs/ (see Active table). Do not reopen this as a hot twin.


# `src/legacy/render` subdirectory + dual-run — Implementation Plan


> **Design living:** SP decisions live in [../specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md](../specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md). This file is the checklist only.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Execute on **master**. Do **not** `git commit` unless the user asks. Do **not** create branches. Spec: [`../specs/2026-09-27-legacy-render-subdirectory-dual-run-design.md`](../specs/2026-09-27-legacy-render-subdirectory-dual-run-design.md).

**Goal:** Keep leftover `legacy_render` dual-running safely; delete `gdi_simple`; split fat tops into responsibility subdirs; phase GDI ↔ MapLibre-style parity without blocking Views map2d/RHI.

**Architecture:** Layout **B′ colocated** — each `.h` lives next to its `.cpp`/`.cc` under `legacy/render/<top>/<module>/` (P2 initially used headers-at-top; revised). Thin tops stay flat. One GDI device (`SmtRhi2dRenderDevice`). Modern 2D remains `gis::map2d` → `render::map2d::Pass` → RHI. Present-facade HWND rules unchanged.

**Tech Stack:** C++23, GN/`build.bat`, Windows GDI/GDI+, FlyCube RHI (Views), existing `map_carto2d_test` / `gdi_map_paint_test`.

**Progress (2026-09-27):** P1 landed. P2 landed (gdi → gl → render3d → scene3d; thin tops absorbed; `gdiaux/` instead of Windows-forbidden `aux/`). **P2.1 Task 7b landed** — B′ colocated `.h`+`.cpp`; smoke PASS (`legacy_render` + `map_carto2d_test` + `gdi_map_paint_test`, `realtime non-white samples: 29993`). **P2.2 landed** — `render3d/` → `rhi/`; `model3d`/`terrain`/`pointcloud` into `scene3d/` (single `scene3d_sources`). **P2.3 landed** — `scene3d/` B′ modules `object/` `terrain/` `pointcloud/` (no flat root geo files). **P2.4 landed** — `scene3d/` re-layered `scene/` `primitive/` `feature/` `surface/` `dem/` `bridge/` `test/`. **P2.5 landed** — `rhi/` deep B′; top `gl/` → `rhi/impl/gl/`. **P2.6 landed** — modules under `rhi/public/`; `gl_` file basename prefix stripped under `impl/gl/` (GN `gl_map_paint_test` label kept; source `map_paint_test.cc`; tops = `bridge/` `gdi/` `rhi/` `scene3d/`). P3 Task 8 audit filled; Task 9+ remain open (no shared-constant extract yet).

## Global Constraints

- Work on **master** only; no feature branches; no commit unless user asks
- No Qt; no new `src/render` → `legacy` deps
- Nesting cap `legacy/render/<top>/<module>/`
- Aggregate `//src/legacy/render:legacy_render` / `dll_stem=legacy_render` stays
- English comments on touched code; copyright year 2026
- Do not move files that P1 is deleting; do not start P3 edits inside paths P2 will `git mv`

## File map (end state)

| Path | Responsibility |
| --- | --- |
| `src/legacy/render/BUILD.gn` | DLL; no `gdi_simple`; no `RENDER_GDI_SIMPLE_EXPORTS` |
| `src/legacy/render/README.md` | Dual-run + layout as-built |
| `src/legacy/render/bridge/renderer.cpp` | CreateDevice alias for old simple name |
| `src/legacy/render/gdi/**` | Sole 2D GDI device + carto + common buffers |
| `src/ui/views/map/map_viewport.cc` | Drop obsolete `render_gdi_simple*` LoadLibrary names |
| `docs/build/abi-rename-map.md` | Drop / note retired `render_gdi_simple` stems if still listed |
| *(deleted)* `src/legacy/render/gdi_simple/**` | Gone |

---

## Phase P1 — Delete `gdi_simple`

### Task 1: Inventory and freeze call sites

**Files:**
- Read: `src/legacy/render/gdi_simple/**`, `src/legacy/render/BUILD.gn`, `src/legacy/render/bridge/renderer.cpp`, `src/ui/views/map/map_viewport.cc`
- Search (CBM `search_code` / scoped): `gdi_simple`, `CreateGdiSimple`, `RENDER_GDI_SIMPLE`, `render_gdi_simple`

- [x] **Step 1: List every in-tree reference**

Expected hits include at least:

- `src/legacy/render/BUILD.gn` (`render_gdi_simple_sources`, `RENDER_GDI_SIMPLE_EXPORTS`)
- `src/legacy/render/bridge/renderer.cpp` (`CreateGdiSimpleRenderDevice`)
- `src/ui/views/map/map_viewport.cc` (`render_gdi_simple_d.dll` / `.dll`)
- Docs / `.tmp/cutover/*` mentioning `gdi_simple`
- Also cleared: `src/gpu/legacy_host.cc` (preferred `CreateGdiSimple*` then fallback)

- [x] **Step 2: Confirm product default device string**

`SmtApp::Init` already sets `str2DRenderDeviceName = "SmtRhi2dRenderDevice"`. No override to simple found in product sources.

- [x] **Step 3: Do not commit**

---

### Task 2: Alias CreateDevice + remove GN / sources

**Files:**
- Modify: `src/legacy/render/bridge/renderer.cpp`
- Modify: `src/legacy/render/BUILD.gn`
- Delete: `src/legacy/render/gdi_simple/` (entire tree)
- Modify: `src/ui/views/map/map_viewport.cc` (LoadLibrary list)
- Modify: `src/legacy/render/gdi/README.md` (note: simple removed)
- Create/Modify: `src/legacy/render/README.md`

**Interfaces:**
- Consumes: `CreateRenderDevice` / `DestroyRenderDevice` from `legacy_render` DLL
- Produces: `CreateDevice("SmtGdiSimpleRenderDevice")` succeeds via alias; `CreateGdiSimpleRenderDevice` export gone

- [x] **Step 1: Rewrite `SmtRenderer::CreateDevice` alias**

- [x] **Step 2: Drop `gdi_simple` from root `BUILD.gn`**

- [x] **Step 3: Delete the directory** (`git rm -rf src/legacy/render/gdi_simple`)

- [x] **Step 4: Clean `MapViewport::try_local_device` names**

Probe list is now `legacy_render_d.dll` / `legacy_render.dll` only.

- [x] **Step 5: Write `src/legacy/render/README.md`**

- [x] **Step 6: Build and test** — verified 2026-09-27 (agent smoke; `out/` only)

```bat
.\build.bat legacy_render
.\build.bat map_carto2d_test
.\build.bat gdi_map_paint_test
REM then: out\map_carto2d_test.exe ; out\gdi_map_paint_test.exe
```

Expected: `legacy_render_d.dll` links; both tests PASS; no unresolved `CreateGdiSimple*`. **Result: PASS** (logs: `.tmp/smoke_legacy_render.log`, `.tmp/smoke_map_carto2d_test.log`, `.tmp/smoke_gdi_map_paint_test.log`, `.tmp/smoke_*_run.log`; ninja: `out/build.log`).

- [x] **Step 7: Do not commit** (unless user asks)

---

## Phase P2 — Subdirectory layout

### Task 3: Layout `gdi/`

- [x] **Step 1: `git mv` implementation `.cpp` into subdirs; leave public `.h` at `gdi/` root**

`device/` `thread/` `buffer/` `gdiaux/` (Windows forbids `aux/`) `carto/` `test/`

- [x] **Step 2: Fix `BUILD.gn` `sources` paths; keep target names `gdi_common_sources` / `render_gdi_sources`**

- [x] **Step 3: Build/test** — verified 2026-09-27 with P1 Step 6 smoke (same three targets PASS)

```bat
.\build.bat legacy_render
.\build.bat map_carto2d_test
.\build.bat gdi_map_paint_test
```

- [x] **Step 4: Do not commit** (unless user asks)

---

### Task 4: Layout `gl/`

- [x] **Step 1: `git mv` + update GN sources; keep `3drenderdevice.h` public path stable**

`device/` `caps/` `ext/` `buffer/` `text/` `test/`

- [x] **Step 2: Build** — covered by `.\build.bat legacy_render` smoke PASS 2026-09-27

```bat
.\build.bat legacy_render
```

- [x] **Step 3: Do not commit** (unless user asks)

---

### Task 5: Layout `render3d/`

- [x] **Step 1: `git mv` + GN path update**

`camera/` `gpu/` `material/`; `3drenderer.cpp` stays at root with public headers. `frustum.cpp` under `camera/` (still not in GN sources — unchanged).

- [x] **Step 2: Build `legacy_render`** — covered by smoke PASS 2026-09-27

- [x] **Step 3: Do not commit** (unless user asks)

---

### Task 6: Layout `scene3d/` (coordinate with DEM unify)

- [x] **Step 1: Confirm DEM public header paths with dem-unify plan; prefer moving `.cc` only if headers must stay**

DEM / map_bridge public `.h` remain at `scene3d/` root (`DemHeightField` ABI).

- [x] **Step 2: `git mv` + GN** — `scene/` `dem/` `map_bridge/` `test/`; build/test left to human (`dem_stereo_test`)

- [x] **Step 3: Do not commit** (unless user asks)

---

### Task 7: Thin tops + root README pass

- [x] **Step 1: Update as-built docs only (no forced split of thin tops)**

README tree + `docs/build/src-layout.md` / `abi-rename-map.md` note retired `gdi_simple`.

- [x] **Step 2: Do not commit** (unless user asks)

---

### Task 7b: Colocate `.h` with `.cpp` (revise scheme B → B′)

**Files:** `src/legacy/render/{gdi,gl,render3d,scene3d}/**`, callers under `src/legacy/**` / docs that `#include "legacy/render/<top>/<header>.h"`, per-top `BUILD.gn`, `src/legacy/render/README.md`, design §3/§4/§6.

- [x] **Step 1: Inventory** — headers at fat-top roots whose paired `.cpp`/`.cc` already lives in a submodule; leave RC/`resource.h`, header-only without a clear module owner, and root-paired `3drenderer.*` at top.

- [x] **Step 2: `git mv` headers into matching subdirs** (`device/` `thread/` `buffer/` `gdiaux/` `carto/` / `caps/` `ext/` `text/` / `camera/` `gpu/` `material/` / `scene/` `dem/` `map_bridge/`). Keep `gdiaux/` (not `aux/`). **47 headers moved.**

- [x] **Step 3: Rewrite includes + GN `sources` paths** that listed top-root headers; preserve `Smt_*` / DLL export ABI names. Callers touched only for include paths: `legacy/tool/group/*3d*`, `legacy/ui/shell/view_3d.*`, `legacy/ui/catalog/scenemgr.h`.

- [x] **Step 4: Update design/plan/README** — document B′ colocated as chosen layout.

- [x] **Step 5: Smoke** — verified 2026-09-27 (agent)

```bat
.\build.bat legacy_render
.\build.bat map_carto2d_test
.\build.bat gdi_map_paint_test
REM then: out\map_carto2d_test.exe ; out\gdi_map_paint_test.exe
```

**Result: PASS** — `legacy_render_d.dll` linked; both tests exit 0; `realtime non-white samples: 29993`. Logs: `.tmp/smoke_colocate_*.log`.

- [x] **Step 6: Do not commit** (unless user asks)

---

## Phase P3 — GDI ↔ MapLibre-style parity

### Task 8: Must-row audit (no new engines)

- [x] **Step 1: Fill a short checklist against spec §7 Must column**

| Capability (Must) | GDI status | map2d status | Gap owner |
| --- | --- | --- | --- |
| Painter order (fill → line → symbol; road casing before fill) | GDI road draw: casing then fill (`gdi_renderdevice.cpp`); layer loop order | StyleDocument `road-casing` before `road-fill` (`default_style.cc` + `map2d_test`) | **OK** — readable dual-run |
| GDI+ AA lines/polygons/text | Present (`gdi_gdiplus` + carto) | RHI pass quality separate (Should) | **OK** for Must |
| Road casing + fill from class/kind | `carto2d_road_*` table | Style JSON layers + paint widths | Open: shared width **constant** extract (Should-ish / Task 9) |
| Label fields `anno`/`name`/`text` | `carto2d_label_text` | Layout field pick | **OK** |
| Grid / hash collision + LOD budget | `MapCarto2dFrame` | `gis::map2d::detail` collision (comment: thresholds match) | **OK**; tune shared budgets later |
| Along-line label rotation | `carto2d_line_label_pose` | `line_label_pose` | **OK** |
| Text halo | `carto2d_halo_px` + GDI draw | `halo_width_px` on DrawItem | **OK** |
| Background color / opacity | `carto2d_map_bg` | `MapFrame::background_*` | **OK** |
| Raster XYZ underlay | GDI tile layer path exists; Views TileProvider | Modern primary | GDI only if leftover host needs — no new work now |

Cross-ref comment added on `gdi/map_carto2d.h` pointing at `gis::map2d::detail`.

- [x] **Step 2: Open follow-up tasks only for real gaps**

Follow-ups (Task 9): optional shared road-width table (prefer a neutral helper, **not** `src/render` → legacy); no heatmap/hillshade on GDI.

- [x] **Step 3: Do not commit** (unless user asks)

---

### Task 9: Close Must gaps (phased mini-slices)

**Files:** (only those named by Task 8 gaps)

- [ ] **Step 1: One capability per change** (e.g. shared road width table constant; painter-order comment + test assert)

Deferred: shared width table needs a non-legacy home; dual paths already readable. Comment-only Must cross-ref landed in Task 8.

- [ ] **Step 2: Add/extend tests**

```bat
.\build.bat te map_carto2d_test
.\build.bat te map2d_test
```

- [x] **Step 3: Do not commit** (unless user asks)

---

### Task 10: Should-row backlog only

- [x] **Step 1: Document Should items as Deferred in `src/legacy/render/README.md`** — no heatmap/hillshade/fill-extrusion on GDI

- [x] **Step 2: Do not commit** (unless user asks)

---

## Self-review (spec coverage)

| Spec section | Plan task |
| --- | --- |
| Dual-run policy | Global Constraints + README (Tasks 2, 7) |
| Delete `gdi_simple` | Tasks 1–2 |
| Layout per top (B′ colocated) | Tasks 3–7 + 7b |
| Parity matrix must/should/later | Tasks 8–10 |
| Non-goals / no Qt / no render→legacy | Global Constraints |
| Phased order P1→P2→P3 | Phase headers |

Placeholder scan: none intentional. Alias lifetime left as open follow-up per spec §10.

## Execution handoff

P1 + P2 + P2.1 (Task 7b colocated headers) code/docs landed on master (uncommitted). Smoke verified 2026-09-27 (`legacy_render` + `map_carto2d_test` + `gdi_map_paint_test` PASS). Archive only when Task 9 build gates and remaining Must gaps are closed.
