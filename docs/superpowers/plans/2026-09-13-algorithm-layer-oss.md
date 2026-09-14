<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Algorithm-layer modernization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> This repo assigned **sibling agents per package**. Execute **Task 1 (geo) first**. Tasks 2a/2b/2c may run in parallel after Task 1 is on `master`. Task 3 (dem) waits for Task 2b.

**Goal:** Wrap shipped gdal_sdk GEOS + PROJ 9; merge math/math3d/geo3d into `SmtGeoCore`; replace in-tree projections and TIN; remove the DEM algorithm DLL; move MFC charts to `src/ui/chart`.

**Architecture:** One `smt_shared_library("geo")` compiles today’s geo + geo3d + math + math3d sources. Old GN labels become `group()` with `public_deps` on geo. Algorithms use C++20 `geo::geometry_traits` (instance coordinateDimension 2|3) and `geo::vector_traits` (compile-time dim). proj/tin are still their own DLLs. See the spec.

**Tech Stack:** C++20 MSVC v145, `geos_c` + PROJ 9 from `//third_party/gdal_sdk`, GN `smt_shared_library` + `test()`, optional header-only CDT.

**Spec:** `docs/superpowers/specs/2026-09-13-algorithm-layer-oss-design.md`

## Global Constraints

- Stay on `master`; do not create topic branches; do not commit unless the user asks.
- Copyright: `// Copyright (c) 2026 The Mogu Authors.` on every new/touched engineering file; bump year on existing Mogu headers.
- New C++: `snake_case` functions; at most two public namespace levels (`geo`, `proj`, `tin`); internals in `detail` or an anonymous namespace. Types PascalCase.
- **Traits:** match `.cursor/rules/style/cxx-standard.mdc`. Geometry dim is **instance** 2 or 3. Compile-time dim only on vectors. No `Geometry2`/`Geometry3`.
- `cc_std = "c++20"`. No `/std:c++17`, no `/std:c++latest`.
- Comments in English.
- Do not vendor a second GEOS/PROJ, glm, Eigen, CGAL, Triangle, or Qt.
- Do not change `content/public`. Do not put projection math in `sdb/crs`.
- Build output only under repo-root `out/` via `build.bat`. Do not use `out/Default`.
- No `Co-authored-by: Cursor`.
- Keep remaining `dll_stem` values: `SmtGeoCore`, `SmtGisPrj`, `SmtTinMesh`, `SmtBAOrthGrid`, `SmtStaCore`, `SmtStaDiagram`.
- Agent path ownership is exclusive (see spec “What sibling agents own”).

## File map

| Path | Responsibility |
| --- | --- |
| `src/algorithm/geo/BUILD.gn` | Union sources; `dll_stem = "geo"` |
| `src/algorithm/{math,math3d,geo3d}/BUILD.gn` | `group` → `//src/algorithm/geo:geo` |
| `src/algorithm/BUILD.gn` | `algorithm` = geo, proj, tin, baogrid, stat (no dem, no chart) |
| `src/algorithm/geo/geometry_traits.h` | `geometry_traits` + `geometry_like` |
| `src/algorithm/geo/vector_traits.h` | `vector_traits` for Vector/Vector2/3/4 |
| `src/algorithm/geo/geos_backend.h/.cc` | `geos_backend_traits`; `buffer` / predicates |
| `src/algorithm/geo/geo_traits_test.cc` | instance vs compile-time dim |
| `src/algorithm/geo/geo_geos_test.cc` | Intersects + buffer via GEOS |
| math/geo3d export headers | `#pragma comment(lib)` → `SmtGeoCore`; define old `Export_Smt*` when geo exports |
| `src/algorithm/proj/*` | Delete gauss/lambert; PROJ 9 adapter |
| `src/algorithm/tin/*` | One backend; `read_xyz_points`; drop in-tree Delaunay `.cpp` |
| `third_party/cdt/` | Only if `geos_c.h` lacks required Delaunay symbols |
| `src/ui/chart/` | Moved `stat_chart` |
| `src/plugin/dem/` | GDAL raster + tin XYZ; drop dem DLL dep |
| `src/algorithm/dem/` | Delete after plugin retarget |
| `build/BUILD.gn` | include dir chart → `ui/chart`; drop `algorithm/dem` |
| Root `BUILD.gn` | register new `test()` targets |
| `src/BUILD.gn` + root `README.md` | DLL count after merge lands |

---

### Task 1: geo merge, traits, GEOS (geo agent)

**Files:**
- Modify: `src/algorithm/geo/BUILD.gn`
- Modify: `src/algorithm/math/BUILD.gn`
- Modify: `src/algorithm/math3d/BUILD.gn`
- Modify: `src/algorithm/geo3d/BUILD.gn`
- Modify: `src/algorithm/BUILD.gn`
- Create: `src/algorithm/geo/export_compat.h`
- Create: `src/algorithm/geo/geometry_traits.h`
- Create: `src/algorithm/geo/vector_traits.h`
- Create: `src/algorithm/geo/geos_backend.h`
- Create: `src/algorithm/geo/geos_backend.cc`
- Create: `src/algorithm/geo/geo_traits_test.cc`
- Create: `src/algorithm/geo/geo_geos_test.cc`
- Modify: export/`#pragma comment` headers under `algorithm/{geo,geo3d,math,math3d}`
- Modify: `third_party/BUILD.gn` (geos_c import-lib config next to existing `gdal`)
- Modify: root `BUILD.gn` (`test_all`)

**Interfaces:**
- Consumes: existing sources listed in the four `BUILD.gn` files; `SmtGeometry::GetCoordinateDimension`; `geos_c.h` from gdal_sdk
- Produces:
  - `//src/algorithm/geo:geo` → `SmtGeoCore.dll`
  - `group("math"|"math3d"|"geo3d")` `public_deps` geo
  - `geo::geometry_traits<G>::coordinate_dimension(const G&) -> int`
  - `geo::vector_traits<V>::dimension` (`constexpr` 2, 3, or 4)
  - `geo::buffer(const G&, coordinate_type)`
  - `geo::geos_backend_traits<Smt_Geo::SmtGeometry>` (and point/polygon specializations as needed)

- [ ] **Step 1: Write the failing traits test**

Create `src/algorithm/geo/geo_traits_test.cc`:

```cpp
// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "geometry_traits.h"
#include "vector_traits.h"
#include "geometry.h"
#include "mathlib.h"
#include "mathlib_3d.h"

#include <cstdio>

int main() {
  static_assert(geo::vector_traits<Smt_3DMath::Vector3>::dimension == 3);
  static_assert(geo::vector_traits<Smt_Math::Vector>::dimension == 2);
  Smt_Geo::SmtPoint p2;
  if (geo::geometry_traits<Smt_Geo::SmtGeometry>::coordinate_dimension(p2) != 2) {
    std::fprintf(stderr, "default point dim\n");
    return 1;
  }
  p2.SetCoordinateDimension(3);
  if (geo::geometry_traits<Smt_Geo::SmtGeometry>::coordinate_dimension(p2) != 3) {
    std::fprintf(stderr, "set dim 3\n");
    return 1;
  }
  return 0;
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `build.bat te` (or `ninja -C out geo_traits_test` after adding the `test()` target).  
Expected: FAIL (missing headers or unresolved `geo::`).

- [ ] **Step 3: Merge GN + traits + GEOS backend**

`geo/BUILD.gn` `sources` = current geo list **plus** every file from `math/BUILD.gn`, `math3d/BUILD.gn`, and `geo3d/BUILD.gn`. `deps` = `//src/base:base`, `//src/base:core`, `//third_party:gdal`, plus the geos_c import-lib config. Drop `//src/algorithm/math:math` from geo’s deps (those objects are in this DLL).

`math/BUILD.gn`, `math3d/BUILD.gn`, `geo3d/BUILD.gn`:

```gn
group("math") {
  public_deps = [ "//src/algorithm/geo:geo" ]
}
```

`algorithm/BUILD.gn` `group("algorithm")` deps: geo, proj, tin, baogrid, stat only. `group("geom")` deps: geo only.

When `GEO_EXPORT` is defined, define `Export_SmtMathLib`, `Export_Smt3DMathLib`, `GEO_EXPORT`. Point `#pragma comment(lib, …)` at `SmtGeoCore` / `SmtGeoCoreD`.

Land traits headers as in the spec (same signatures as `cxx-standard.mdc`). `geos_backend_traits<SmtGeometry>::intersects` / `buffer` call `geos_c`. Wire `SmtGeometry::Intersects` and `SmtGeometry::Buffer` to those functions so existing virtual ABI uses GEOS.

- [ ] **Step 4: Write `geo_geos_test` and run both tests**

`geo_geos_test`: build two overlapping unit squares as `SmtPolygon`, assert `Intersects` is true; `buffer` a `SmtPoint` and assert the result is non-empty.

Run: `build.bat te`  
Expected: `geo_traits_test` and `geo_geos_test` PASS. `//:all` still green; outputs no longer include `SmtMathLib.dll`, `Smt3DMathLib.dll`, `Smt3DGeoCore.dll`.

- [ ] **Step 5: Docs count (same change)**

Set `src/BUILD.gn` “31 DLLs” to the post-merge count. If root `README.md` still says 31, update that sentence and **最后更新**.

---

### Task 2a: PROJ 9 adapter (proj agent)

**Files:**
- Delete: `src/algorithm/proj/gaussprj.cpp`, `gaussprj.h`, `lambertprj.cpp`, `lambertprj.h`
- Modify: `src/algorithm/proj/projection_api.h`, `projection_api.cpp`, `projection.h`, `projection.cpp`, `prjx.h`, `BUILD.gn`
- Create: `src/algorithm/proj/proj_backend.h`, `proj_backend.cc`, `proj_transform_test.cc`

**Interfaces:**
- Consumes: `//src/algorithm/geo:geo` (or the math/geo groups), PROJ 9 `proj.h` from gdal_sdk
- Produces:
  - `SmtLoadProjectionStringEPSG(SmtProjection*, const char* epsg)` using PROJ 9
  - `SmtProjectPoint` via `proj_trans`
  - `Projection::PrjLB2XY` / `PrjXY2LB` via the same adapter
  - `GetPrjX()` returns `nullptr`
  - `proj::transform_xy(src_crs, dst_crs, x, y) -> long` (`SMT_ERR_*`)

- [ ] **Step 1: Write the failing transform test**

`proj_transform_test.cc`: load EPSG 4326 and 3857; transform `(116.3883, 39.9289)`; require both outputs within 1.0 of `(12958573.0, 4853957.0)`. Second case: empty CRS string fails.

- [ ] **Step 2: Run the test to verify it fails**

Expected: FAIL (PROJ.4 ifdef / missing adapter / Gauss still in the graph).

- [ ] **Step 3: Implement the PROJ 9 adapter; delete Gauss/Lambert**

No `proj_api.h`. No `USE_PROJ4`. `SmtProjection` holds `PJ*`. `sdb/crs` stays identity-only (do not edit `crs.h` except a comment that still claims PROJ.4).

- [ ] **Step 4: Run `proj_transform_test`**

Expected: PASS. `gaussprj.cpp` / `lambertprj.cpp` are not in `BUILD.gn`.

---

### Task 2b: TIN backend (tin agent)

**Files:**
- Create: `src/algorithm/tin/tin_backend.h`, `tin_backend.cc`, `xyz_points.h`, `xyz_points.cc`, `tin_delaunay_test.cc`, `tin_xyz_test.cc`
- Modify: `src/algorithm/tin/tin_api.cpp`, `tin_api.h`, `BUILD.gn`
- Delete: `tin_delaunay_divide.cpp`, `tin_delaunay_incremental.cpp` (and their `.h` if nothing else includes them) after the backend passes
- Create: `third_party/cdt/` **only if** `geos_c.h` lacks `GEOSDelaunayTriangulation_r` / `GEOSConstrainedDelaunayTriangulation_r` for the path that needs it

**Interfaces:**
- Consumes: `Vector3` / `SmtTin` / `Smt3DSurface` from geo; geos_c and/or CDT
- Produces:
  - `CreateDelaunayTin_Div` and `CreateDelaunayTin_Inc` — same backend
  - `tin::read_xyz_points(const char* path, int skip_header_lines, char separator, int x_col, int y_col, int z_col, std::vector<Smt_3DMath::Vector3>* out) -> long`

- [ ] **Step 1: Write failing tests**

Square of four points → at least two triangles. Temp XYZ of three lines → `read_xyz_points` size 3.

- [ ] **Step 2: Run tests to verify they fail**

Expected: FAIL (API / backend missing).

- [ ] **Step 3: Implement backend + XYZ; point both Create* exports at it**

Follow the spec’s GEOS-then-CDT selection. Do not keep the old Delaunay `.cpp` as a fallback.

- [ ] **Step 4: Run `tin_delaunay_test` and `tin_xyz_test`**

Expected: PASS.

---

### Task 2c: Move chart to `src/ui/chart` (chart agent)

**Files:**
- Move: entire `src/algorithm/chart/` → `src/ui/chart/`
- Modify: `src/ui/chart/BUILD.gn` (`stat_chart`, `dll_stem = "stat_chart"`)
- Modify: `build/BUILD.gn` include dir `algorithm/chart` → `ui/chart`
- Delete: `src/algorithm/chart/` after the move

**Interfaces:**
- Consumes: `//src/algorithm/stat:stat`, `//src/legacy/ui/xview:xview`, `//src/base:core`
- Produces: `//src/legacy/ui/chart:stat_chart` → `SmtStaDiagram.dll` (MFC; not in `src_all`)

- [ ] **Step 1: Move sources and keep the same GN target name `stat_chart`**

- [ ] **Step 2: Confirm `group("algorithm")` does not list chart**

- [ ] **Step 3: `gn gen out` + ninja `stat_chart` only when `smt_build_app` is on (`build.bat app` graph). `build.bat` (`//:all`) stays green without chart.**

Expected: no `algorithm/chart` path; include `"chart.h"` still works via `smt_legacy` + `ui/chart`.

---

### Task 3: Remove algorithm/dem (dem agent; after Task 2b)

**Files:**
- Modify: `src/plugin/dem/dlg_grid_loader.cpp`, `dlg_tin_loader.cpp`, `BUILD.gn`
- Delete: `src/algorithm/dem/` (sources + `BUILD.gn`)
- Modify: `build/BUILD.gn` — remove `$smt_src/algorithm/dem`
- Modify: `src/algorithm/BUILD.gn` if dem is still listed (should already be gone after Task 1)

**Interfaces:**
- Consumes: `GDALOpen` / `RasterIO`; `tin::read_xyz_points`; `CreateDelaunayTin_*`
- Produces: plugin dialogs with no `//src/algorithm/dem:dem` dep; no `SmtDemCore.dll`

- [ ] **Step 1: Retarget grid dialog to GDAL**

Replace `Smt3DGridLoader::LoadDataFromHeightMap` (private BMP) with `GDALOpen` + first-band `RasterIO`. Pixel → `Vector3` using the dialog’s start/scale or `GetGeoTransform`. Then the existing surface/TIN fill.

- [ ] **Step 2: Retarget TIN dialog to `tin::read_xyz_points`**

- [ ] **Step 3: Drop dem from plugin `BUILD.gn`; delete `src/algorithm/dem`**

- [ ] **Step 4: `build.bat` (`//:all`) has no `SmtDemCore`. Plugin still compiles on the `build.bat app` graph.**

---

## Self-review

- Spec sections map to tasks: merge/traits/GEOS → Task 1; proj → 2a; tin → 2b; chart → 2c; dem → 3; baogrid/stat need no task.
- No TBD. Gauss/Lambert deletion, CDT license, DEM ownership, and Geometry-vs-Smt3DGeometry are locked in the spec.
- Names match: `CreateDelaunayTin_Div` / `_Inc`, `tin::read_xyz_points`, `geo::geometry_traits`, `SmtGeoCore`.
