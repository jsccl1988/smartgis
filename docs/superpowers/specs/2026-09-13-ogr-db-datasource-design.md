<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# OGR database datasource (replace ADO)

**Date:** 2026-09-13  
**Status:** accepted  
**Scope:** one implementation plan. Replace the self-written Microsoft ADO connection stack with the already-vendored GDAL/OGR library as the GIS database provider.

## Goal

Stop owning a database client. `src/ado` (`SmtAdoCore`) is a COM wrapper around `msado15.dll`. `sdb/datasource/ado` (`SmtSDEAdoDevice`) stores GIS feature-class tables in Access / SQL Server through that wrapper.

The replacement talks **PostgreSQL/PostGIS** and **SQLite spatial files** (new files are **GeoPackage**; existing **SpatiaLite** databases still open). Connection, SQL, WKB, spatial filters, transactions, layer create/drop, and raster I/O come from GDAL/OGR. This repo only adapts `SmtDataSource` / `SmtVectorLayer` / `SmtRasterLayer`.

## Non-goals

- Do not vendor SOCI, nanodbc, libpqxx, SQLiteCpp, or a second GDAL tree.
- Do not keep Microsoft ADO / `msado15.dll` on the default product path.
- Do not recreate the ADO catalog table `DS_TB` or per-type `*_Fcls` SQL schemas.
- Do not change `content/public`.
- Do not merge DLLs that are unrelated to this provider.
- Qt is banned.

## Architecture

```
SmtDataSourceMgr
        |
        |  DS_DB_ADO + PROVIDER_POSTGRES | PROVIDER_GPKG | PROVIDER_SPATIALITE
        v
sdb/datasource/gdal  (new DLL SmtSDEGdalDevice)
        |
        |  GDALOpenEx / GDALDataset / OGRLayer / GDALRasterBand
        v
third_party/gdal_sdk   (existing, no second copy)
        |
        +-- PostGIS  (PG: connection string)
        +-- GeoPackage / SpatiaLite  (file path)
```

- Implementation lives in `src/sdb/datasource/gdal/`, which today is a register stub (`register_gdal_driver()` returns false).
- Device manager constructs the OGR datasource instead of `SmtAdoDataSource`.
- SMF keeps using GDAL for **files**. The new driver is the **database** provider. Shared feature codec is extracted so SMF and the DB driver do not duplicate OGR ↔ `SmtFeature` mapping.
- `src/ado` and `sdb/datasource/ado` leave the default `src_all` graph once the manager no longer instantiates them.

## Components

| Unit | Role | Depends on |
| --- | --- | --- |
| `OgrDataSource` (`SmtDataSource`) | Open/close a GDAL dataset; list/create/delete layers | `//third_party:gdal` |
| `OgrVectorLayer` (`SmtVectorLayer`) | One class for all vector feature types; branches on OGR geometry + product fields | codec + `OGRLayer` |
| `OgrRasterLayer` (`SmtRasterLayer`) | Raster layers and child-image overlays in the same dataset | `GDALDataset` raster API |
| `ogr_feature_codec` | Bidirectional `SmtFeature` ↔ `OGRFeature` (geometry, attributes, style blob) | `algorithm/geo`, `sdb/feature` |
| `SmtDataSourceMgr` | Register providers; assemble GDAL connection strings from `SmtDataSourceInfo` | `sde_gdal` instead of `sde_ado` |

Public C++ for new files stays at two namespace levels: `sdb::datasource`. Helpers go in `sdb::datasource::detail` or an anonymous namespace. The types the manager already names (`SmtDataSource`, `SmtVectorLayer`) stay in `Smt_GIS`.

### Traits (C++17, MSVC-safe TMP)

Windows `cc_std` is **c++17**. Use template **traits + `if constexpr` + `std::index_sequence`** so feature kinds and DB providers are data, not seven copied layer classes or a 200-line switch in every function.

**Do use (MSVC v145 / C++17):** class template specialization, `constexpr` members, `using` type aliases, `std::tuple` field lists, `std::index_sequence` / pack-in-initializer-list `int unused[] = {0, (expr, 0)...}`, `if constexpr`, `static_assert` on the primary traits template.

**Do not use (likely fail or unused here):** C++20 concepts / requires, NTTP `template<char...>`, C++23 `if consteval`, ranges, `__if_exists`, expression-SFINAE that only gcc/clang accept. Do not bump `cc_std` in this change.

#### Provider traits (`ogr_connect`)

```cpp
template <uint Provider>
struct db_provider_traits;  // primary: static constexpr bool supported = false;

template <>
struct db_provider_traits<Smt_GIS::PROVIDER_GPKG> {
  static constexpr bool supported = true;
  static constexpr const char* driver_name = "GPKG";
  static std::string open_target(const Smt_GIS::SmtDataSourceInfo& info);
};
```

Free functions `is_db_provider_supported` / `gdal_driver_name` / `make_gdal_open_target` are thin runtime switches that dispatch to these specializations. All path/PG-string logic lives once in the specialization.

#### Feature-kind traits (`ogr_feature_kind.h`)

One specialization per `SmtFeatureType`. Generic create-layer / codec / extra-field setup is a function template on `Traits`. Runtime `SmtFeatureType` enters through a **single** `visit_feature_kind(ft, fn)` switch that calls `fn(feature_kind_traits<Ft>{})`.

```cpp
template <Smt_GIS::SmtFeatureType Ft>
struct feature_kind_traits;  // primary undefined

struct field_anno {
  static constexpr char name[] = "anno";
  static constexpr OGRFieldType ogr_type = OFTString;
  static constexpr Smt_Core::varType smt_type = Smt_Core::SmtString;
};

template <>
struct feature_kind_traits<Smt_GIS::SmtFtAnno> {
  static constexpr Smt_GIS::SmtFeatureType feature_type = Smt_GIS::SmtFtAnno;
  static constexpr OGRwkbGeometryType wkb = wkbPoint;
  static constexpr bool is_raster = false;
  using extra_fields = std::tuple<field_anno, field_color, field_angle>;
};
```

- `OgrVectorLayer::Create` does not switch on geometry to copy-paste `CreateLayer`. It `visit_feature_kind` → `dataset->CreateLayer(name, nullptr, Traits::wkb, nullptr)` then `create_fields_from_tuple<typename Traits::extra_fields>(layer)`.
- Codec encode/decode of extra attributes is a tuple walk, not per-type `GetCollect` copies.
- Geometry convertors are still small per-WKB helpers (Point/Line/Polygon/TIN/Grid); they are registered as `Traits` members (`encode_geom` / `decode_geom` function pointers or static functions) so the public codec stays two functions.

`OgrRasterLayer` is the `is_raster == true` kinds (`SmtFtChildImage` and raster layers), not a second copy of the vector class.

### New files (snake_case `.cc` / `.h`)

Under `src/sdb/datasource/gdal/`:

- `ogr_dataset.h` / `ogr_dataset.cc`
- `ogr_vec_layer.h` / `ogr_vec_layer.cc`
- `ogr_raster_layer.h` / `ogr_raster_layer.cc`
- `ogr_feature_kind.h` — `feature_kind_traits` specializations + `visit_feature_kind` (header-only)
- `ogr_feature_codec.h` / `ogr_feature_codec.cc` — generic encode/decode over traits
- `ogr_connect.h` / `ogr_connect.cc` — `db_provider_traits` + thin free functions
- `gdal_driver.h` / `gdal_driver.cc` — `register_gdal_driver()` returns true after `GDALAllRegister()`

GN: replace `source_set("gdal_seam")` with `smt_shared_library("sde_gdal")`, `dll_stem = "sde_gdal"`, `SDE_GDAL_EXPORT`.

### Device manager and enums

Keep `eDSType::DS_DB_ADO` as the database datasource kind (existing URLs and `unType` integers stay meaningful as “DB”, not as Microsoft ADO).

Append to `eSmtDBProvider` (do not reorder existing values):

```
PROVIDER_POSTGRES,     // PostGIS
PROVIDER_GPKG,         // GeoPackage file
PROVIDER_SPATIALITE,   // existing SpatiaLite file
```

`PROVIDER_ACCESS` and `PROVIDER_SQLSERVER` remain in the enum. `Open()` on those providers logs that they are unsupported and returns false.

Connection string mapping:

| Provider | `szService` | `szDBName` | GDAL open |
| --- | --- | --- | --- |
| `PROVIDER_POSTGRES` | host (optional `:port`) | database name | `PG:host=… port=… dbname=… user=… password=…` |
| `PROVIDER_GPKG` | directory | `name.gpkg` | `GDALOpenEx` on that file; create with GPKG driver if missing |
| `PROVIDER_SPATIALITE` | directory | `name.sqlite` / `.db` | SQLite driver, `SPATIALITE=YES` |

URL format stays `sdb:name\service,dbname,uid,pwd,type,provider`.

Default product sample in `src/app/app_core` that currently sets `PROVIDER_ACCESS` switches to `PROVIDER_GPKG`.

### Feature type mapping (no custom SQL schema)

OGR/GDAL owns storage. Product types map as follows:

| `SmtFeatureType` | Storage |
| --- | --- |
| `SmtFtDot` | OGR Point / MultiPoint |
| `SmtFtCurve` | OGR LineString / MultiLineString |
| `SmtFtSurface` | OGR Polygon / MultiPolygon |
| `SmtFtAnno` | OGR Point plus fields `anno` (string), `color` (int), `angle` (real) |
| `SmtFtTin` | OGR TIN when the driver accepts `wkbTIN`; otherwise MultiPolygon of triangles |
| `SmtFtGrid` | Regular row/col + values → GDAL raster in the same dataset; otherwise MultiPoint plus `grid_row` / `grid_col` |
| `SmtFtChildImage` | GDAL raster in the same dataset (GPKG tiles or PostGIS raster). Not a private blob schema. |
| Raster layer | `GDALDataset` raster (same open as vectors for GPKG; PostGIS raster when the SDK driver is present) |

Style remains a product binary field named `style` (`OFTBinary`). Envelope / FID come from OGR; do not persist a parallel `mbr_*` column set.

Layer identity: OGR layer name is the product layer name. No `DS_TB`. `GetLayerTableInfos()` walks `GDALDataset::GetLayerCount()` and raster bands/subdatasets.

TIN / Grid / annotation / child-image remain first-class product layer types. They are not dropped; they are not stored as ADO `geom_points` blobs.

## Data flow

1. **Create** — `SmtDataSourceMgr::CreateDataSource` with `DS_DB_ADO` + one of the three new providers constructs `OgrDataSource`, fills `szUrl`.
2. **Open** — `GDALAllRegister()` once; `GDALOpenEx` with update access. Missing file GPKG: `GetGDALDriverManager()->GetDriverByName("GPKG")->Create`. Failure: `m_bOpen = false`, log `CPLGetLastErrorMsg()` and the list of vector drivers if `PostgreSQL` / `GPKG` / `SQLite` is absent.
3. **List / create layer** — `CreateLayer` / `CopyLayer` / `DeleteLayer` on the GDAL dataset. Geometry type from the requested `SmtFeatureType`.
4. **Fetch** — `OGRLayer::ResetReading` + `GetNextFeature`; codec fills `SmtFeature`. Optional in-memory cache stays an implementation detail of the layer (SMF/ADO already clone into `SmtMem*` for editing); the source of truth is OGR.
5. **Append / update / delete** — codec to `OGRFeature`; `CreateFeature` / `SetFeature` / `DeleteFeature`.
6. **Query** — `SetSpatialFilter` from `SmtGQueryDesc`; attribute filter via `SetAttributeFilter` when `SmtPQueryDesc` is a simple expression the layer can pass through. Unsupported filters fall back to scan + existing in-memory query on the mem clone.
7. **Transactions** — `GDALDataset::StartTransaction` / `CommitTransaction` / `RollbackTransaction` when the driver reports support; otherwise each `CreateFeature` is auto-commit.
8. **Close** — `GDALClose`; clear layer list.

No COM, no `_ConnectionPtr`, no `_variant_t`.

## Error handling

- Install a GDAL error callback that writes to the existing `SmtLog` named `SmtSDEGdalDevice` (same pattern as `C_STR_SDE_ADODEVICE_LOG`).
- Public methods return the existing `bool` / `SMT_ERR_*` codes. Do not throw across the DLL boundary.
- Open/create failure is never silent: log driver name, connection target (password redacted), and CPL message.
- If the SDK build lacks PostgreSQL, GPKG still works. PostGIS `Open` fails with an explicit “PostgreSQL driver not in this GDAL” message, not a crash.

## Testing

Add `test("sde_gdal_test")` under `src/sdb/datasource/gdal/` and register it on `//:test_all`.

Always run (no server):

1. Create a temp `.gpkg`.
2. Create a point layer, append one feature, close, reopen, fetch; IDs and coordinates match.
3. Line and polygon round-trip.
4. Annotation: `anno` / `color` / `angle` round-trip.
5. Reject `PROVIDER_ACCESS` open (returns false).

Optional: if environment `SMT_PG_DSN` is set (libpq-style or `PG:` string), run the same point round-trip against PostGIS. Skip when unset so `build.bat te` stays green without a database.

Do not require a live SQL Server or Access `.mdb`.

## Build / runtime

- Link `//third_party:gdal` only (same as SMF). Copy GDAL runtime DLLs already handled by `gdal_runtime_dlls`.
- PostgreSQL at runtime needs `libpq` if/when the SDK driver is built with it. If `gdal_sdk` from mgis is missing that driver, document it in `src/sdb/datasource/gdal` comments; do not vendor libpq separately unless cmake `is_build_third_party=true` is later used to enable `GDAL_USE_POSTGRESQL`.
- SQLite is already in the GDAL SDK graph.

`sde_mgr` deps: drop `//src/ado:ado` and `//src/sdb/datasource/ado:sde_ado`; add `//src/sdb/datasource/gdal:sde_gdal`. Remove `comsuppw.lib` from mgr if nothing else needs COM.

`//src/sdb:datasource` and `//src:src_all`: swap `sde_ado` / `ado` for `sde_gdal`. Leave `src/ado` and `sdb/datasource/ado` sources on disk until a follow-up delete; they must not be in `src_all`.

## Docs to update in the same implementation change

- `docs/build/src-layout.md` — `src/ado` no longer in `src_all`; `datasource/gdal` is the OGR DB device; DLL stem `SmtSDEGdalDevice`.
- `src/README.md` — same.
- Root `README.md` — only if the module table still names ADO as the DB path; then refresh **最后更新**.
- `docs/README.md` — link this spec.

## Risks

- GDAL SDK without PostgreSQL: PostGIS waits; GPKG ships.
- GPKG TIN: use MultiPolygon fallback; product type remains `SmtFtTin`.
- PostGIS raster / GPKG raster support varies by SDK; child-image and rasters fail `Create` with a logged `SMT_ERR_UNSUPPORTED` if the driver cannot create a raster, rather than inventing a blob table.
- SMF codec extraction must keep SMF file open paths compiling and behaving as today.

## Success

- `build.bat` (`//:all`) is green without `SmtAdoCore` / `SmtSDEAdoDevice`.
- `build.bat te` includes `sde_gdal_test` GPKG round-trip.
- Creating a database datasource from the app uses GeoPackage or PostGIS, not Access.
- No new hand-written SQL dialect or connection pool.
