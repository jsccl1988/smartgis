<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Algorithm-layer modernization (gdal_sdk GEOS + PROJ 9)

**Date:** 2026-09-13  
**Status:** accepted  
**Related:** 三层伞状深度设计（模型 / 渲染 / 计算）见 [`2026-09-13-model-render-compute-design.md`](2026-09-13-model-render-compute-design.md)。产品语言 C++23（traits 写法与本文 C++20 草图兼容）。  
**Scope:** wrap the already-shipped `gdal_sdk` GEOS C API and PROJ 9; merge math + 3D math + 3D geo into one `SmtGeoCore` DLL; retire self-written projections and the DEM algorithm DLL; move MFC charts to legacy UI. Sibling agents implement by package. This document does not implement C++.

## Goal

Stop owning a second geometry kernel, a second projection engine, and algorithm packages that are not algorithms.

Product OGC geometry is **GDAL/OGR** (`OGRGeometry` and children). There is no `using SmtGeometry = OGRGeometry` shim. `src/algorithm/geo` keeps only non-OGR mesh containers (`SmtGrid` / `SmtTin` / `Smt3DSurface`) plus `copy_envelope` / `SmtSpatialRs`. Call OGR for predicates/overlays (GEOS is inside GDAL). Scene Vector/Matrix/Aabb live in `src/render/math` (Eigen). `src/algorithm/proj` is PROJ 9 only. `src/algorithm/tin` is Delaunay. DEM and chart are not algorithm-core packages.

Callers `#include "ogr_geometry.h"` (or `ogrsf_frmts.h`) and name OGR types. Depend on `//src/algorithm/geo:geo` for meshes — `math` / `math3d` / `geo3d` GN labels and directories are gone. Unused `geometry_traits` / `vector_traits` / `geos_backend` / `wkb_codec` / `geos_context` were deleted. Polyline snap helpers live in `src/plugin/orthogrid/detail`.

## Non-goals

- Do not vendor a second GEOS, a second PROJ, glm, Eigen, CGAL, Triangle, SOCI, or Qt (including Qt Charts and any Web chart stack).
- Do not introduce `Geometry2` / `Geometry3` (or `SmtPoint2` / `SmtPoint3`) as public types.
- Do not put compile-time `dimension` on Geometry; that belongs on `Vector` / `Vector2` / `Vector3` / `Vector4` and on backend traits.
- Do not rewrite `Smt_*` virtual ABI or change `dll_stem` values that remain (`SmtGeoCore`, `SmtGisPrj`, `SmtTinMesh`, `SmtBAOrthGrid`, `SmtStaCore`, `SmtStaDiagram`).
- Flatten `algorithm/math`, `math3d`, and `geo3d` **into** `algorithm/geo/` (one module tree). Do not keep those directories as `group()` aliases. Nesting stays `src/<layer>/<module>`.
- Do not add projection math to `src/sdb/crs`. That header stays identity-only (`sdb::CrsId`).
- Do not keep `SmtDemCore` as a DLL or treat DEM as a core algorithm package.
- Do not implement Views + Skia charts in this change. Chart chrome stays the existing MFC modal after the move to `src/ui/chart`.
- Do not change `baogrid` or `stat` behavior except retargeting their GN deps to `//src/algorithm/geo:geo`.
- Do not change `content/public`.
- Qt is banned.

## Architecture

```
callers (sdb / render / plugin / tin / proj / baogrid)
        |
        |  //src/algorithm/geo:geo
        v
src/algorithm/geo          one DLL  SmtGeoCore
        |                  OGRGeometry (instance coordinateDimension 2|3)
        |                  SmtGrid / SmtTin / Smt3DSurface (meshes, not OGR aliases)
        |
        +-- (predicates via OGR; geos_c used by algorithm/tin Delaunay)
        |
        +-- (no homemade Vector/Matrix; no second GEOS)

src/render/math              Eigen-backed Vector3 / Matrix
        |                  leftover names; Aabb/Obb/Plane/Ray
        +-- //third_party:eigen

src/algorithm/proj         DLL  SmtGisPrj
        |                  PROJ 9 adapter only (proj.h, not proj_api.h)
        v
        proj_9_4[_d].dll   (same gdal_sdk prefix)

src/algorithm/tin          DLL  SmtTinMesh
        |                  tin::tin_backend_traits
        +-- GEOS triangulation when the shipped geos_c.h declares it
        +-- else header-only CDT (third_party/cdt, MPL-2.0)

src/algorithm/baogrid      unchanged role
src/algorithm/stat         data only; no chart widgets

src/plugin/dem             MFC UI
        |                  GDAL raster → points; XYZ text → tin::read_xyz_points
        v
        SmtTinMesh + SmtGeoCore + //third_party:gdal

src/ui/chart               MFC modal  SmtStaDiagram  (not algorithm, not src_all)
src/sdb/crs                identity only
```

Approach is **wrap already-shipped gdal_sdk**. `//third_party:gdal` already copies `geos.dll`, `geos_c.dll`, and the PROJ 9 runtime next to `out/`. Geo and proj add GN configs that link the matching import libraries from that same prefix (`gdal_sdk/lib`), following the existing `is_debug` → `gdald.lib` pattern. There is no second install root.

## Components

| Unit | Role | Depends on |
| --- | --- | --- |
| `//src/algorithm/geo:geo` (`SmtGeoCore`) | Mesh containers only: `SmtGrid` / `SmtTin` / `Smt3DSurface` | `//src/base:base`, `//src/base:core`, `//third_party:gdal` |
| `//src/algorithm/math:math` | **removed** (sources under `algorithm/geo/`) | geo |
| `//src/algorithm/math3d:math3d` | **removed** (sources under `algorithm/geo/`) | geo |
| `//src/algorithm/geo3d:geo3d` | **removed** (sources under `algorithm/geo/`) | geo |
| `geo::geometry_traits` / `vector_traits` / `geos_backend_traits` | **removed** (unused theater; call OGR) | — |
| `//src/algorithm/proj:proj` (`SmtGisPrj`) | PROJ 9 adapter; keep `Projection` / `SmtProjectPoint` names | geo group, PROJ 9 import lib |
| `proj::proj_backend_traits` | `transform_xy` via `proj_create_crs_to_crs` + `proj_trans` | PROJ 9 |
| `//src/algorithm/tin:tin` (`SmtTinMesh`) | One triangulation backend; keep `CreateDelaunayTin_*` exports | geo group |
| `tin::tin_backend_traits` | GEOS Delaunay / constrained Delaunay, or CDT | geos_c and/or `//third_party/cdt` |
| `tin::read_xyz_points` | ASCII XYZ → `std::vector` of `Vector3` (no GDAL, no DEM DLL) | math types in geo |
| `//src/algorithm/baogrid:baogrid` | Unchanged BAO grid | geo / gis (via groups) |
| `//src/algorithm/stat:stat` | Unchanged statistics / expressions | core |
| `//src/legacy/ui/chart:stat_chart` | Moved MFC chart DLL `SmtStaDiagram` | stat, xview |
| `//src/plugin/dem:plugin_dem` | DEM UI: GDAL raster → point grid; dialogs call tin | gdal, tin, geo, plugin host |
| `sdb::CrsId` | Layer SRS **name** only | none of proj |

Public C++ for new files stays at two namespace levels: `geo`, `proj`, `tin`. Helpers go in `geo::detail` / `proj::detail` / `tin::detail` or an anonymous namespace. Legacy `Smt_Geo`, `Smt_3DGeo`, `Smt_Math`, `Smt_3DMath`, `Smt_Prj`, `Smt_TinMesh`, `Smt_StaDiagram` namespaces stay for exported types.

### One Geometry hierarchy (locked interpretation)

Product OGC types are **`OGRGeometry` / `OGRPoint` / `OGRLineString` / `OGRPolygon` / …**. `getCoordinateDimension()` is **2 or 3 on the instance**. That is OGC `coordinateDimension`. `getDimension()` remains the topological dimension (0/1/2) and is **not** what traits call `coordinate_dimension`.

Do **not** add `using SmtGeometry = OGRGeometry` (or any other `Smt*` → OGR alias). Callers include OGR headers and name OGR classes. `SmtGrid` / `SmtTin` / `Smt3DSurface` stay as **mesh containers**, not a second geometry tree and not `Geometry3`. New algorithms call OGR (or `algorithm/tin` for Delaunay). Do not add a third virtual geometry hierarchy. Do not reintroduce empty traits headers.

### Traits (C++20, MSVC v145)

Those headers were **deleted** after the OGR-direct migration (no callers). Call OGR and `algorithm/tin`. Do not re-land empty traits theater. The sketch below is historical only.

Windows `cc_std` is **c++23** (`MSVC /std:c++23preview`). Use `.cursor/rules/style/cxx-standard.mdc`. Do not pin `/std:c++17`. Do not use `/std:c++latest`. Do not require fold-expressions-in-`requires` or GCC `__integer_pack`.

```cpp
namespace geo {

template <typename G>
struct geometry_traits {
  using coordinate_type = typename G::coordinate_type;
  static int coordinate_dimension(const G& g);  // 2 or 3, instance
};

template <typename V>
struct vector_traits {
  using coordinate_type = typename V::coordinate_type;
  static constexpr int dimension = V::dimension;  // 2, 3, or 4
};

template <typename G>
concept geometry_like = requires(const G& g) {
  typename geometry_traits<G>::coordinate_type;
  geometry_traits<G>::coordinate_dimension(g);
};

template <typename G>
struct geos_backend_traits;  // specialize per product type; call geos_c

template <geometry_like G>
void buffer(const G& g, typename geometry_traits<G>::coordinate_type width) {
  geos_backend_traits<G>::buffer(g, width);
}

}  // namespace geo
```

Specializations the geo agent must land (header-only is fine):

| Type | Traits | Notes |
| --- | --- | --- |
| `Smt_Math::Vector` | `vector_traits` dim 2 | 2D math vector; no `dimension` member on the class — the trait supplies it |
| `Smt_3DMath::Vector2` | dim 2 | compile-time only |
| `Smt_3DMath::Vector3` | dim 3 | compile-time only |
| `Smt_3DMath::Vector4` | dim 4 | compile-time only |
| `OGRGeometry` (and OGR children) | `geometry_traits` | `coordinate_type = double`; `coordinate_dimension(g)` = `g.getCoordinateDimension()` |

Do not invent a parallel `geo::Geometry` class or `Smt*` aliases for OGR.

GEOS is XY. `geos_backend_traits` drop Z for the GEOS call and restore the instance coordinateDimension on the result. 3D predicates use the XY of the GEOS call; they do not grow a 3D GEOS port.

YAGNI: traits collapse 2D/3D dispatch and backend choice. This is not an in-tree Boost.Geometry.

### Merged `SmtGeoCore` (geo agent)

`smt_shared_library("geo")` lists **all** sources that used to live in `algorithm/geo`, `geo3d`, `math`, and `math3d` — now as local files under `algorithm/geo/`. `dll_stem` stays `SmtGeoCore`. `defines = [ "GEO_EXPORT" ]`.

Compatibility: `GEO_EXPORT` also defines `Export_SmtMathLib`, `Export_Smt3DMathLib`, and `GEO_EXPORT` so existing `__declspec` headers stay valid. `#pragma comment(lib, …)` lines name `SmtGeoCore` / `SmtGeoCoreD` only.

`src/algorithm/BUILD.gn`:

- `group("geom")` deps only `//src/algorithm/geo:geo`.
- `group("algorithm")` deps: `geo`, `proj`, `tin`, `baogrid`, `stat`. **Not** `dem`. **Not** `chart`. **Not** math/math3d/geo3d (those labels are deleted).

Callers depend on `//src/algorithm/geo:geo`. Do not keep `//src/algorithm/math:math` / `math3d:math3d` / `geo3d:geo3d`.

`build/BUILD.gn` `smt_legacy` include dirs keep `algorithm/geo` (quoted `#include "geometry.h"`) and `render/math` (`"mathlib_3d.h"`). Drop `algorithm/math`, `math3d`, and `geo3d`. Do not point includes at `algorithm/` as a parent.

### proj (proj agent)

Delete self-written Gauss and Lambert: `gaussprj.cpp`, `gaussprj.h`, `lambertprj.cpp`, `lambertprj.h` (and any `PrjX` subclass that only exists for those). Do not keep a software fallback.

Replace the `USE_PROJ4` / `proj_api.h` path in `projection_api.h` with PROJ 9 `proj.h`. `SmtProjection` stores a `PJ*` (and a `PJ_CONTEXT*` owned by the DLL). `SmtLoadProjectionString` / `SmtLoadProjectionStringEPSG` call `proj_create` / `proj_create_crs_to_crs`. `SmtProjectPoint` / `PrjLB2XY` / `PrjXY2LB` call `proj_trans`.

Delete the `PrjX` / `GaussPrj` / `LambertPrj` / `Projection` wrapper tree. Zone and standard-parallel UI fields become PROJ strings via `make_tmerc_crs` / `make_lcc_crs` in `proj_runtime`. `plugin/proj` and other callers use `SmtInitProjection` / `SmtLoadProjectionString` / `SmtLoadTmercCrs` / `SmtProjectPoint`.

`sdb/crs/crs.h` stays:

```cpp
namespace sdb {
struct CrsId {
  const char* name;
};
}
```

Transforms stay in `algorithm/proj`. Do not store `PJ*` on the layer.

PROJ data files: use the `PROJ_DATA` / `PROJ_LIB` directory already next to the gdal_sdk runtime (same prefix whose DLLs `gdal_runtime_dlls` copies). `SmtSetPROJ_LIB` sets that path. If the SDK layout has no separate PROJ data dir, set it to `gdal_sdk/share/proj` when that folder exists beside the include prefix; the adapter logs and returns `SMT_ERR_INVALID_FILE` when `proj_create_crs_to_crs` fails.

### tin (tin agent)

One backend. `CreateDelaunayTin_Div` and `CreateDelaunayTin_Inc` stay exported and both call `tin::tin_backend_traits`. The two in-tree Delaunay `.cpp` files are removed once the backend test passes.

Selection (compile time, one interpretation):

1. If shipped `geos_c.h` declares `GEOSDelaunayTriangulation_r`, use it for point-set TIN (`SmtTin` / `Smt3DSurface` from `Vector3` arrays).
2. If it also declares `GEOSConstrainedDelaunayTriangulation_r`, use that for `DivPolygenIntoTriMesh`.
3. If either symbol is missing, vendor **header-only** [CDT](https://github.com/artem-ogre/CDT) (MPL-2.0) at `third_party/cdt` and use it for the missing path. Do not add CGAL. Do not add Triangle (license).
4. Do not keep the self-written incremental/divide sources as a third fallback.

`tin::read_xyz_points(path, skip_header_lines, separator, x_col, y_col, z_col)` lives here so XYZ ingest is testable without MFC. Columns are 0-based. Separator is a single `char` (`','`, `' '`, or `'\t'`).

### dem (dem agent)

`algorithm/dem` is **not** a core algorithm package. Remove `//src/algorithm/dem:dem` from `group("algorithm")` and from `src_all` (it disappears when algorithm drops it). Delete `SmtDemCore` as a DLL.

`plugin/dem` stays UI (`SmtAMDemCreater`). It:

- Opens heightmaps with **GDAL** (`GDALOpen` / `GDALRasterBand::RasterIO`). Any raster GDAL can read is in scope; 8-bit BMP is no longer a private parser. Build a `Vector3` grid (pixel → x/y from geotransform or from the dialog’s start/scale; z from the first band) and call `CreateDelaunayTin_*` or fill `Smt3DSurface` the same way the old grid loader did.
- Opens ASCII XYZ through `tin::read_xyz_points`, then the same TIN call.
- Drops `//src/algorithm/dem:dem` from `plugin/dem/BUILD.gn`. Adds `//third_party:gdal`, `//src/algorithm/tin:tin`, `//src/algorithm/geo:geo`.

Delete `src/algorithm/dem/` in the same change once the plugin compiles without it. Remove `$smt_src/algorithm/dem` from `build/BUILD.gn` `smt_legacy` include dirs.

Do not recreate `Smt3DGridLoader` / `Smt3DTinLoader` as a shared library. Dialog `.cpp` files own the GDAL calls.

### chart (chart agent)

Chart is **not** algorithm. Move `src/algorithm/chart/` → `src/ui/chart/` (same sources, `smt_mfc_shared_library("stat_chart")`, `dll_stem = "stat_chart"`). Data stays in `algorithm/stat`.

Update `build/BUILD.gn` include dir `algorithm/chart` → `ui/chart`. Do not add chart to `group("algorithm")` or `src_all` (MFC, already gated).

No Qt Charts. No Web charts. Views + Skia chart widgets are a later UI spec, not this implementation.

### baogrid and stat

Stay. After the geo merge they pick up `SmtGeoCore` through `//src/algorithm/geo:geo`. No API rewrite.

## Data flow

1. **Geometry construct** — callers create `SmtPoint` / `SmtLineString` / … and `SetCoordinateDimension(2|3)`. Z accessors already exist on those types when dim is 3.
2. **Predicate / overlay** — `geo::buffer` / existing `SmtGeometry::Intersects` (implementation becomes GEOS) → `geos_backend_traits` → `GEOSGeom_create*` / `GEOSIntersects_r` / `GEOSBuffer_r` → decode to `SmtGeometry`, `SetCoordinateDimension` from the input instance.
3. **Vector math** — `Vector` / `Vector3` live in `src/render/math` (Eigen). Algorithms that need compile-time dim use `vector_traits<V>::dimension`.
4. **Project** — `SmtLoadProjectionStringEPSG("4326")` + `"3857"` (or full `proj_create_crs_to_crs` strings) → `SmtProjectPoint`. Layer objects keep storing the SRS **name** in `SmtLayerInfo.szSRS` / `sdb::CrsId`.
5. **TIN** — points (`Vector3*` or XYZ file via `tin::read_xyz_points`) → backend → `SmtTin` or `Smt3DSurface`. `CreateDelaunayTin_Div` and `_Inc` are the same path.
6. **Heightmap** — plugin dialog → GDAL band → point grid → step 5. No `SmtDemCore`.
7. **Chart** — `SmtStaCore` computes values; `src/ui/chart` shows an MFC modal. No new data model.

No COM. No second math library.

## Error handling

- Public methods keep existing `long` / `int` / `bool` / `SMT_ERR_*` codes. Do not throw across a DLL boundary.
- Install a GEOS error handler (`initGEOS_r` / `GEOSContext_setNoticeHandler_r`) that writes to `SmtLog` named `SmtGeoCore`.
- PROJ failures: log `proj_context_errno_string`, return `SMT_ERR_INVALID_PARAM` (bad CRS string) or `SMT_ERR_FAILURE` (transform failed). Redact nothing PROJ-specific beyond existing log practice; there are no passwords here.
- GDAL raster open in `plugin/dem`: log `CPLGetLastErrorMsg()`, return `SMT_ERR_INVALID_FILE`. Missing raster driver is a logged failure, not a crash.
- XYZ parse: `SMT_ERR_INVALID_FILE` if the file cannot be opened; `SMT_ERR_INVALID_PARAM` if a data line has too few columns; skip blank lines.
- If `gdal_sdk` headers lack `geos_c.h` / `proj.h`, the geo/proj targets fail at compile time (the SDK is a hard dependency). Do not `#ifdef` a software Gauss fallback.

## Testing

Add `test()` targets and register each on `//:test_all` in the root `BUILD.gn`. Follow `src/sdb/datasource/gdal:sde_gdal_test` (`import("//testing/test.gni")`, `output_name`, include `//src`).

Always run (no extra servers, no MFC):

| Test | Package | Asserts |
| --- | --- | --- |
| `proj_transform_test` | proj | EPSG:4326 lon/lat `(116.3883, 39.9289)` → EPSG:3857; expected Web Mercator within 1 metre of `(12958573.0, 4853957.0)` |
| `proj_rejects_empty_crs` | proj | empty CRS string → `SmtLoadProjectionString` fails (`!= 0` error) |
| `tin_delaunay_test` | tin | four points of a square → at least two triangles; all input vertices appear |
| `tin_xyz_test` | tin | write a three-line temp XYZ, `read_xyz_points` returns three `Vector3`s |
| `chart` | — | no new algorithm test (MFC). Move must keep `stat_chart` generating under `smt_build_app` |

Optional: `plugin/dem` has no `test()` in this spec (MFC). Raster heightmap coverage is the plugin dialog path plus the tin tests above.

`build.bat te` stays green without PROJ grid files beyond what gdal_sdk already ships for EPSG:4326 ↔ 3857.

## Target tree

```
src/algorithm/
  BUILD.gn                 # algorithm = geo, proj, tin, baogrid, stat
  geo/                     # SmtGeoCore DLL (TIN/grid/surface meshes only)
                           # no math/, math3d/, geo3d/; no traits/WKB/GEOS glue
  proj/                    # SmtGisPrj, PROJ 9 only (no gauss/lambert files)
  tin/                     # SmtTinMesh + tin::read_xyz_points
  baogrid/                 # unchanged
  stat/                    # unchanged (data)
                           # no dem/, no chart/

src/ui/chart/              # SmtStaDiagram (MFC)
src/plugin/dem/            # UI + GDAL raster
src/sdb/crs/               # identity only
third_party/cdt/           # only if geos_c.h lacks the needed Delaunay symbol
third_party/gdal_sdk/      # existing GEOS + PROJ 9 (no second copy)
```

New headers (snake_case `.h`; implementation `.cc` for new TUs):

- `src/algorithm/proj/proj_backend.h` / `proj_backend.cc`
- `src/algorithm/tin/tin_backend.h` / `tin_backend.cc`
- `src/algorithm/tin/xyz_points.h` / `xyz_points.cc`

Legacy `.cpp` files keep that extension.

## GN / DLL map

| Old DLL stem | Old label | After | New stem |
| --- | --- | --- | --- |
| `SmtMathLib` | `//src/algorithm/math:math` | **removed** (files in `algorithm/geo/`) | *(absorbed)* `SmtGeoCore` |
| `Smt3DMathLib` | `//src/algorithm/math3d:math3d` | **removed** (files in `algorithm/geo/`) | *(absorbed)* `SmtGeoCore` |
| `SmtGeoCore` | `//src/algorithm/geo:geo` | `smt_shared_library` (one tree) | `SmtGeoCore` |
| `Smt3DGeoCore` | `//src/algorithm/geo3d:geo3d` | **removed** (files in `algorithm/geo/`) | *(absorbed)* `SmtGeoCore` |
| `SmtGisPrj` | `//src/algorithm/proj:proj` | same DLL; PROJ 9 internals | `SmtGisPrj` |
| `SmtTinMesh` | `//src/algorithm/tin:tin` | same DLL; GEOS/CDT internals | `SmtTinMesh` |
| `SmtDemCore` | `//src/algorithm/dem:dem` | **removed** | — |
| `SmtBAOrthGrid` | `//src/algorithm/baogrid:baogrid` | unchanged | `SmtBAOrthGrid` |
| `SmtStaCore` | `//src/algorithm/stat:stat` | unchanged | `SmtStaCore` |
| `SmtStaDiagram` | `//src/algorithm/chart:stat_chart` | `//src/legacy/ui/chart:stat_chart` | `SmtStaDiagram` |

`//:all` / `//src:src_all` lose four on-disk DLLs: `SmtMathLib`, `Smt3DMathLib`, `Smt3DGeoCore`, `SmtDemCore`. Update the “31 DLLs” comment in `src/BUILD.gn` and the root README **in the implementation change** that lands the merge (not in this docs-only change).

`group("algorithm")` after:

```gn
group("algorithm") {
  deps = [
    "//src/algorithm/baogrid:baogrid",
    "//src/algorithm/geo:geo",
    "//src/algorithm/proj:proj",
    "//src/algorithm/stat:stat",
    "//src/algorithm/tin:tin",
  ]
}
```

## Phased implementation order

Geo merge is the gate. Other packages may edit only their own trees in parallel **after** the geo groups exist on `master`, or they depend on `//src/algorithm/geo:geo` directly.

| Phase | Owner | Lands | Blocks |
| --- | --- | --- | --- |
| 1 | **geo** | Union sources into `SmtGeoCore`; math/math3d/geo3d groups; traits; GEOS predicates/buffer; `algorithm/BUILD.gn`; `geo_*_test` | proj, tin, dem (GN) |
| 2a | **proj** | Delete Gauss/Lambert; PROJ 9 adapter; `proj_*_test` | nothing else |
| 2b | **tin** | Backend + XYZ helper; remove in-tree Delaunay `.cpp`; `tin_*_test` | dem |
| 2c | **chart** | Move to `src/ui/chart`; fix include dir | nothing else |
| 3 | **dem** | Retarget `plugin/dem` to GDAL + tin; delete `src/algorithm/dem` | — |

baogrid and stat have no phase. They compile once phase 1 groups exist.

## What sibling agents own

| Agent | Trees (do not edit outside these) |
| --- | --- |
| **geo** | `src/algorithm/geo`, `src/algorithm/geo3d`, `src/algorithm/math`, `src/algorithm/math3d`, `src/algorithm/BUILD.gn`. May add `//third_party` GN configs for `geos_c` import libs (not a second GEOS tree). May add `geo_*_test` and a `test_all` line. May change `#pragma comment(lib)` in those four trees so they name `SmtGeoCore`. |
| **proj** | `src/algorithm/proj` only. Must not add files under `sdb/crs` except if a comment there still says “PROJ.4 optional” — then one-line comment fix is allowed. `sdb/crs` **code** stays identity-only. |
| **tin** | `src/algorithm/tin` only. May add `third_party/cdt` **only** when phase-1/2 geos_c.h lacks the required Delaunay symbol. |
| **dem** | `src/algorithm/dem` (delete from graph + directory) and `src/plugin/dem`. May drop `algorithm/dem` from `build/BUILD.gn` include dirs. |
| **chart** | `src/algorithm/chart` → `src/ui/chart`. May edit `build/BUILD.gn` include dir for the chart path. |

Do not implement C++ in the spec change. Do not run ninja for this document.

## Docs to update in the same implementation change

Phase 1 (geo) updates algorithm rows if this spec’s layout edit is already on `master` (it is). Implementation agents still:

- Refresh `src/BUILD.gn` “31 DLLs” to the post-merge count.
- Refresh root `README.md` `//:all` DLL count and **最后更新** when that sentence is no longer true.
- `docs/build/src-layout.md` and `src/README.md` — already describe the approved target in this docs change; only touch them again if a path name slips.

## Risks

- `gdal_sdk` import lib names for `geos_c` / PROJ 9 differ by debug/release. Match the prefix already used for `gdald.lib`; do not invent `out/third_party` as a second root unless `is_build_third_party=true` already installs there.
- GEOS XY-only: Z is carried on the product instance, not through GEOS.
- EPSG:4326 ↔ 3857 needs the shipped PROJ database; if `proj_create_crs_to_crs` fails in CI, the test fails honestly — do not hard-code a Gauss fallback.
- LoadLibrary of `SmtMathLib.dll` / `Smt3DMathLib.dll` / `Smt3DGeoCore.dll` / `SmtDemCore.dll` will fail after the merge. Product plugins in this repo link via GN, not those names. Do not add forwarding stub DLLs.
- `plugin/proj` must not construct `GaussPrj` / `LambertPrj` / `PrjX`; those types are gone.

## Success

- `build.bat` (`//:all`) is green without `SmtMathLib`, `Smt3DMathLib`, `Smt3DGeoCore`, or `SmtDemCore` as separate outputs.
- `build.bat te` includes `proj_test`, tin tests, and the remaining `test_all` binaries (no geo traits/GEOS wrapper tests).
- One Geometry story: instance 2 or 3 on `SmtGeometry`; compile-time dim only on vectors; no `Geometry2`/`Geometry3`.
- `algorithm/proj` contains no Gauss/Lambert implementation files.
- `algorithm/dem` is gone; `plugin/dem` opens rasters with GDAL.
- Chart sources live under `src/ui/chart`; `algorithm/stat` still owns numbers.
- No second GEOS/PROJ, no glm/Eigen, no Qt.
