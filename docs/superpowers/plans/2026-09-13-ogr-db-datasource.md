# OGR Database Datasource Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the Microsoft ADO GIS database provider with the already-vendored GDAL/OGR library (PostGIS + GeoPackage; SpatiaLite open).

**Architecture:** New DLL `SmtSDEGdalDevice` (`src/sdb/datasource/gdal`) implements `SmtDataSource` over `GDALDataset` / `OGRLayer`. Device manager constructs it for `DS_DB_ADO` plus `PROVIDER_POSTGRES` / `PROVIDER_GPKG` / `PROVIDER_SPATIALITE`. Access/SQL Server open returns false. Shared `ogr_feature_codec` is a `source_set` used by this DLL and later by SMF. No SOCI, nanodbc, or second GDAL.

**Tech Stack:** GDAL/OGR (`//third_party:gdal`), existing `Smt_GIS` layer/feature types, GN `smt_shared_library` + `test()`, Windows MSVC.

**Spec:** `docs/superpowers/specs/2026-09-13-ogr-db-datasource-design.md`

## Global Constraints

- Stay on `master`; do not create topic branches.
- Copyright: `// Copyright (c) 2026 The Mogu Authors.` on every new/touched engineering file; bump year on existing Mogu headers.
- New C++: `snake_case` functions, at most two public namespace levels (`sdb::datasource`); internals in `detail` or anonymous namespace. Types PascalCase (`OgrDataSource`).
- **Traits / TMP:** feature kinds and DB providers are `feature_kind_traits<Ft>` / `db_provider_traits<Provider>` specializations. Generic algorithms (`visit_feature_kind`, `create_fields_from_tuple`, codec field copy) must not duplicate per-type layer classes. Stay on **C++17** MSVC (`cc_std = c++17`): `if constexpr`, `std::tuple` + `std::index_sequence`, pack-in-`int unused[] = {0, (expr, 0)...}`. No concepts, NTTP strings, or C++23.
- Comments in English.
- Do not vendor SOCI, nanodbc, libpqxx, SQLiteCpp, Qt, or a second GDAL tree.
- Do not recreate `DS_TB` or `*_Fcls` SQL schemas.
- Do not change `content/public`.
- Build output only under repo-root `out/` via `build.bat`.
- No `Co-authored-by: Cursor`.
- `dll_stem` for the new device is `SmtSDEGdalDevice`. Leave `src/ado` and `sdb/datasource/ado` sources on disk but drop them from `src_all`.

## File map

| Path | Responsibility |
| --- | --- |
| `src/sdb/layer/layer.h` | Append `PROVIDER_POSTGRES`, `PROVIDER_GPKG`, `PROVIDER_SPATIALITE` |
| `src/sdb/datasource/gdal/ogr_connect.h/.cc` | Provider checks + GDAL open target strings |
| `src/sdb/datasource/gdal/ogr_feature_codec.h/.cc` | Bidirectional `SmtFeature` ↔ `OGRFeature` |
| `src/sdb/datasource/gdal/ogr_dataset.h/.cc` | `OgrDataSource` |
| `src/sdb/datasource/gdal/ogr_vec_layer.h/.cc` | One `OgrVectorLayer` for all vector types |
| `src/sdb/datasource/gdal/ogr_raster_layer.h/.cc` | Raster/child-image; `SMT_ERR_UNSUPPORTED` if driver cannot create |
| `src/sdb/datasource/gdal/gdal_driver.h/.cc` | `register_gdal_driver()` → `GDALAllRegister()` |
| `src/sdb/datasource/gdal/sde_gdal_test.cc` | GPKG round-trip + ACCESS reject; optional PostGIS |
| `src/sdb/datasource/gdal/BUILD.gn` | `ogr_codec` source_set + `sde_gdal` DLL + test |
| `src/sdb/datasource/mgr/datasourcemgr.cpp` + `BUILD.gn` | Instantiate `OgrDataSource`; drop ADO deps |
| `src/sdb/BUILD.gn`, `src/BUILD.gn`, `src/sdb/datasource/BUILD.gn` | Swap ado → gdal in groups |
| `src/sdb/datasource/smf/smf_ogrsupport.cpp` | Call shared codec |
| `src/app/app_core/app_smtapp.cpp` | Default sample → GPKG |
| Docs: `docs/build/src-layout.md`, `src/README.md`, `README.md` if it still names ADO as the DB path | |

---

### Task 1: Providers and GDAL open targets

**Files:**
- Modify: `src/sdb/layer/layer.h` (`eSmtDBProvider`)
- Create: `src/sdb/datasource/gdal/ogr_connect.h`
- Create: `src/sdb/datasource/gdal/ogr_connect.cc`
- Create: `src/sdb/datasource/gdal/sde_gdal_test.cc`
- Modify: `src/sdb/datasource/gdal/BUILD.gn`
- Modify: `BUILD.gn` (`test_all`)

**Interfaces:**
- Consumes: `Smt_GIS::SmtDataSourceInfo`, existing `PROVIDER_ACCESS` / `PROVIDER_SQLSERVER`
- Produces:
  - `sdb::datasource::is_db_provider_supported(uint provider) -> bool`
  - `sdb::datasource::gdal_driver_name(uint provider) -> const char*` (`"PostgreSQL"`, `"GPKG"`, `"SQLite"`, or `nullptr`)
  - `sdb::datasource::make_gdal_open_target(const SmtDataSourceInfo& info) -> std::string`

- [ ] **Step 1: Write the failing test**

Create `src/sdb/datasource/gdal/sde_gdal_test.cc`:

```cpp
// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/datasource/gdal/ogr_connect.h"

#include "layer.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using Smt_GIS::SmtDataSourceInfo;
using Smt_GIS::PROVIDER_ACCESS;
using Smt_GIS::PROVIDER_GPKG;
using Smt_GIS::PROVIDER_POSTGRES;
using Smt_GIS::PROVIDER_SPATIALITE;
using Smt_GIS::PROVIDER_SQLSERVER;

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

}  // namespace

int main() {
  expect(!sdb::datasource::is_db_provider_supported(PROVIDER_ACCESS),
         "ACCESS unsupported");
  expect(!sdb::datasource::is_db_provider_supported(PROVIDER_SQLSERVER),
         "SQLSERVER unsupported");
  expect(sdb::datasource::is_db_provider_supported(PROVIDER_GPKG),
         "GPKG supported");
  expect(sdb::datasource::is_db_provider_supported(PROVIDER_POSTGRES),
         "POSTGRES supported");
  expect(sdb::datasource::is_db_provider_supported(PROVIDER_SPATIALITE),
         "SPATIALITE supported");

  expect(std::strcmp(sdb::datasource::gdal_driver_name(PROVIDER_GPKG), "GPKG") ==
             0,
         "GPKG driver name");
  expect(std::strcmp(sdb::datasource::gdal_driver_name(PROVIDER_POSTGRES),
                     "PostgreSQL") == 0,
         "PG driver name");
  expect(std::strcmp(sdb::datasource::gdal_driver_name(PROVIDER_SPATIALITE),
                     "SQLite") == 0,
         "SQLite driver name");
  expect(sdb::datasource::gdal_driver_name(PROVIDER_ACCESS) == nullptr,
         "ACCESS has no driver");

  SmtDataSourceInfo gpkg;
  gpkg.unProvider = PROVIDER_GPKG;
  std::strcpy(gpkg.db.szService, "C:\\tmp\\ds");
  std::strcpy(gpkg.db.szDBName, "sample1.gpkg");
  std::string gpkg_path = sdb::datasource::make_gdal_open_target(gpkg);
  expect(gpkg_path.find("sample1.gpkg") != std::string::npos, "GPKG path");
  expect(gpkg_path.find("PG:") == std::string::npos, "GPKG is not PG:");

  SmtDataSourceInfo pg;
  pg.unProvider = PROVIDER_POSTGRES;
  std::strcpy(pg.db.szService, "127.0.0.1:5432");
  std::strcpy(pg.db.szDBName, "gis");
  std::strcpy(pg.szUID, "u");
  std::strcpy(pg.szPWD, "secret");
  std::string pg_target = sdb::datasource::make_gdal_open_target(pg);
  expect(pg_target.find("PG:") == 0, "PG prefix");
  expect(pg_target.find("host=127.0.0.1") != std::string::npos, "PG host");
  expect(pg_target.find("port=5432") != std::string::npos, "PG port");
  expect(pg_target.find("dbname=gis") != std::string::npos, "PG dbname");
  expect(pg_target.find("user=u") != std::string::npos, "PG user");
  expect(pg_target.find("password=secret") != std::string::npos, "PG password");

  SmtDataSourceInfo access;
  access.unProvider = PROVIDER_ACCESS;
  expect(sdb::datasource::make_gdal_open_target(access).empty(),
         "ACCESS target empty");

  if (g_fails) {
    std::fprintf(stderr, "%d checks failed\n", g_fails);
    return 1;
  }
  std::printf("sde_gdal_test connect checks ok\n");
  return 0;
}
```

Append to `eSmtDBProvider` in `src/sdb/layer/layer.h` **after** `PROVIDER_MYSQL` (do not reorder):

```cpp
		PROVIDER_MYSQL,				//MySQL 数据库
		PROVIDER_POSTGRES,			// PostgreSQL / PostGIS
		PROVIDER_GPKG,				// GeoPackage file
		PROVIDER_SPATIALITE,			// SpatiaLite file
```

Replace `src/sdb/datasource/gdal/BUILD.gn` with:

```gn
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

import("//build/smartgis.gni")
import("//testing/test.gni")

source_set("ogr_codec") {
  sources = [
    "ogr_connect.cc",
    "ogr_connect.h",
  ]
  include_dirs += [ "//src" ]
  deps = [
    "//src/base:core",
    "//src/sdb/map:gis",
  ]
}

smt_shared_library("sde_gdal") {
  dll_stem = "sde_gdal"
  defines = [ "SDE_GDAL_EXPORT" ]
  sources = [
    "gdal_driver.cc",
    "gdal_driver.h",
  ]
  include_dirs += [ "//src" ]
  deps = [
    ":ogr_codec",
    "//src/base:core",
    "//src/sdb/map:gis",
    "//third_party:gdal",
  ]
}

test("sde_gdal_test") {
  output_name = "sde_gdal_test"
  sources = [ "sde_gdal_test.cc" ]
  include_dirs += [ "//src" ]
  deps = [
    ":ogr_codec",
    "//src/base:core",
    "//src/sdb/map:gis",
  ]
}
```

Keep `gdal_driver.cc` compiling (still returns false). Add to root `BUILD.gn` `group("test_all")`:

```gn
group("test_all") {
  testonly = true
  deps = [
    "//testing/e2e:exe_smoke",
    "//src/sdb/datasource/gdal:sde_gdal_test",
  ]
}
```

- [ ] **Step 2: Run test to verify it fails**

```bat
build.bat te
```

Expected: link or compile fails (`ogr_connect.h` missing) **or** if you stub empty files first, `sde_gdal_test` exits 1. Do not implement `make_gdal_open_target` before this run.

- [ ] **Step 3: Write minimal implementation**

`src/sdb/datasource/gdal/ogr_connect.h`:

```cpp
// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_DATASOURCE_GDAL_OGR_CONNECT_H_
#define SDB_DATASOURCE_GDAL_OGR_CONNECT_H_

#include "layer.h"

#include <string>

namespace sdb {
namespace datasource {

bool is_db_provider_supported(uint provider);
const char* gdal_driver_name(uint provider);
std::string make_gdal_open_target(const Smt_GIS::SmtDataSourceInfo& info);

}  // namespace datasource
}  // namespace sdb

#endif  // SDB_DATASOURCE_GDAL_OGR_CONNECT_H_
```

`src/sdb/datasource/gdal/ogr_connect.cc`:

```cpp
// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/datasource/gdal/ogr_connect.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace sdb {
namespace datasource {

bool is_db_provider_supported(uint provider) {
  return provider == Smt_GIS::PROVIDER_POSTGRES ||
         provider == Smt_GIS::PROVIDER_GPKG ||
         provider == Smt_GIS::PROVIDER_SPATIALITE;
}

const char* gdal_driver_name(uint provider) {
  switch (provider) {
    case Smt_GIS::PROVIDER_POSTGRES:
      return "PostgreSQL";
    case Smt_GIS::PROVIDER_GPKG:
      return "GPKG";
    case Smt_GIS::PROVIDER_SPATIALITE:
      return "SQLite";
    default:
      return nullptr;
  }
}

std::string make_gdal_open_target(const Smt_GIS::SmtDataSourceInfo& info) {
  if (info.unProvider == Smt_GIS::PROVIDER_GPKG ||
      info.unProvider == Smt_GIS::PROVIDER_SPATIALITE) {
    std::string path = info.db.szService;
    if (!path.empty()) {
      char last = path[path.size() - 1];
      if (last != '\\' && last != '/') {
        path += '\\';
      }
    }
    path += info.db.szDBName;
    return path;
  }
  if (info.unProvider == Smt_GIS::PROVIDER_POSTGRES) {
    std::string host = info.db.szService;
    std::string port = "5432";
    std::string::size_type colon = host.rfind(':');
    if (colon != std::string::npos && colon + 1 < host.size()) {
      port = host.substr(colon + 1);
      host = host.substr(0, colon);
    }
    char buf[1024];
    std::snprintf(buf, sizeof(buf),
                 "PG:host=%s port=%s dbname=%s user=%s password=%s", host.c_str(),
                 port.c_str(), info.db.szDBName, info.szUID, info.szPWD);
    return buf;
  }
  return std::string();
}

}  // namespace datasource
}  // namespace sdb
```

- [ ] **Step 4: Run the tests and make sure they pass**

```bat
build.bat te
```

Expected: `out\sde_gdal_test.exe` prints `sde_gdal_test connect checks ok` and exit 0. `exe_smoke` may skip missing product exes.

- [ ] **Step 5: Commit**

```bat
git add src/sdb/layer/layer.h src/sdb/datasource/gdal/ogr_connect.h src/sdb/datasource/gdal/ogr_connect.cc src/sdb/datasource/gdal/sde_gdal_test.cc src/sdb/datasource/gdal/BUILD.gn BUILD.gn
git commit -m "Add OGR DB provider ids and GDAL open-target helper."
```

---

### Task 2: Bidirectional feature codec (point, line, polygon, annotation)

**Files:**
- Create: `src/sdb/datasource/gdal/ogr_feature_codec.h`
- Create: `src/sdb/datasource/gdal/ogr_feature_codec.cc`
- Modify: `src/sdb/datasource/gdal/BUILD.gn` (`ogr_codec` sources + `//third_party:gdal`)
- Modify: `src/sdb/datasource/gdal/sde_gdal_test.cc`

**Interfaces:**
- Consumes: `OGRFeature`, `SmtFeature`, `SmtPoint` / `SmtLineString` / `SmtPolygon` / `SmtLinearRing`
- Produces:
  - `sdb::datasource::copy_ogr_feature_to_smt(OGRFeature* src, SmtFeature* dst) -> bool`
  - `sdb::datasource::copy_smt_feature_to_ogr(const SmtFeature* src, OGRFeature* dst) -> bool`
  - Annotation fields: `anno` (string), `color` (int), `angle` (real)

- [ ] **Step 1: Write the failing test**

Add to `sde_gdal_test.cc` (include `ogrsf_frmts.h`, `feature.h`, `geometry.h`, `ogr_feature_codec.h`). After connect checks, before the fail count:

```cpp
  GDALAllRegister();
  OGRSFDriver* mem = OGRSFDriverRegistrar::GetRegistrar()->GetDriverByName("Memory");
  expect(mem != nullptr, "Memory OGR driver");
  if (mem) {
    OGRDataSource* ds = mem->CreateDataSource("codec_mem");
    expect(ds != nullptr, "Memory datasource");
    if (ds) {
      OGRFieldDefn anno("anno", OFTString);
      OGRFieldDefn color("color", OFTInteger);
      OGRFieldDefn angle("angle", OFTReal);
      OGRLayer* lyr = ds->CreateLayer("pts", nullptr, wkbPoint, nullptr);
      expect(lyr != nullptr, "Memory point layer");
      if (lyr) {
        lyr->CreateField(&anno);
        lyr->CreateField(&color);
        lyr->CreateField(&angle);
        SmtFeature smt;
        smt.SetID(7);
        smt.SetFeatureType(SmtFtAnno);
        smt.SetGeometryDirectly(new SmtPoint(1.5, 2.5));
        SmtField f_anno;
        f_anno.SetName("anno");
        f_anno.SetType(SmtString);
        smt.AddField(f_anno);
        SmtField f_color;
        f_color.SetName("color");
        f_color.SetType(SmtInteger);
        smt.AddField(f_color);
        SmtField f_angle;
        f_angle.SetName("angle");
        f_angle.SetType(SmtReal);
        smt.AddField(f_angle);
        smt.SetFieldValue(smt.GetFieldIndexByName("anno"), "hi");
        smt.SetFieldValue(smt.GetFieldIndexByName("color"), 9);
        smt.SetFieldValue(smt.GetFieldIndexByName("angle"), 45.0);
        OGRFeature ogr(lyr->GetLayerDefn());
        expect(sdb::datasource::copy_smt_feature_to_ogr(&smt, &ogr),
               "smt->ogr anno");
        SmtFeature back;
        expect(sdb::datasource::copy_ogr_feature_to_smt(&ogr, &back),
               "ogr->smt anno");
        const SmtPoint* pt = dynamic_cast<const SmtPoint*>(back.GetGeometryRef());
        expect(pt && pt->GetX() == 1.5 && pt->GetY() == 2.5, "anno xy");
        int ai = back.GetFieldIndexByName("anno");
        expect(ai >= 0, "anno field present");
      }
      OGRDataSource::DestroyDataSource(ds);
    }
  }
```

Use the GDAL 2/3 C++ API already used in `smf_ogrsupport.cpp` (`ogrsf_frmts.h`). If this SDK is GDAL 3 only (`GDALDataset` not `OGRDataSource`), match SMF: that file includes `ogrsf_frmts.h` and uses `OGRFeature`. Prefer `GDALDriver` / `GDALDataset` if SMF compile flags already do; **match the includes SMF compiles with**. If Memory-driver CreateDataSource is deprecated, use:

```cpp
GDALDriver* drv = GetGDALDriverManager()->GetDriverByName("Memory");
GDALDataset* ds = drv->Create("codec_mem", 0, 0, 0, GDT_Unknown, nullptr);
OGRLayer* lyr = ds->CreateLayer("pts", nullptr, wkbPoint, nullptr);
GDALClose(ds);
```

- [ ] **Step 2: Run test to verify it fails**

```bat
build.bat te
```

Expected: compile error `copy_smt_feature_to_ogr` not found, or link error.

- [ ] **Step 3: Write minimal implementation**

`ogr_feature_codec.h`:

```cpp
// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_DATASOURCE_GDAL_OGR_FEATURE_CODEC_H_
#define SDB_DATASOURCE_GDAL_OGR_FEATURE_CODEC_H_

class OGRFeature;

namespace Smt_GIS {
class SmtFeature;
}

namespace sdb {
namespace datasource {

bool copy_ogr_feature_to_smt(OGRFeature* src, Smt_GIS::SmtFeature* dst);
bool copy_smt_feature_to_ogr(const Smt_GIS::SmtFeature* src, OGRFeature* dst);

}  // namespace datasource
}  // namespace sdb

#endif
```

`ogr_feature_codec.cc`: port the point/line/polygon branches from `src/sdb/datasource/smf/smf_ogrsupport.cpp` (`CopyOGRGeomToSmtGeom` / `CopyOGRAttToSmtAtt`) into `copy_ogr_feature_to_smt`. Add the reverse:

- `SmtFtDot` / `SmtFtAnno` → `OGRPoint` from `SmtPoint`
- `SmtFtCurve` → `OGRLineString` via `GetNumPoints` / `GetX`/`GetY` (use `SmtLineString::GetPoint`)
- `SmtFtSurface` → `OGRPolygon` from exterior ring (`GetExteriorRing`)
- Copy attribute fields by name when the OGR defn has that field
- `FID` ←→ `SmtFeature::SetID` / `GetID`
- Do **not** apply SMF’s random pen colors in the shared codec

Add `ogr_feature_codec.cc` / `.h` to `ogr_codec` `sources` and `deps += [ "//third_party:gdal" ]`. Test `deps += [ "//third_party:gdal" ]` and `data_deps` if needed so `gdald.dll` sits next to `sde_gdal_test.exe`.

- [ ] **Step 4: Run the tests and make sure they pass**

```bat
build.bat te
```

Expected: `sde_gdal_test` exit 0 including codec round-trip.

- [ ] **Step 5: Commit**

```bat
git add src/sdb/datasource/gdal/ogr_feature_codec.h src/sdb/datasource/gdal/ogr_feature_codec.cc src/sdb/datasource/gdal/sde_gdal_test.cc src/sdb/datasource/gdal/BUILD.gn
git commit -m "Add shared OGR feature codec for point, line, polygon, and annotation."
```

---

### Task 3: `OgrDataSource` open/create GeoPackage and reject Access

**Files:**
- Create: `src/sdb/datasource/gdal/ogr_dataset.h`
- Create: `src/sdb/datasource/gdal/ogr_dataset.cc`
- Modify: `src/sdb/datasource/gdal/gdal_driver.cc`
- Modify: `src/sdb/datasource/gdal/BUILD.gn` (`sde_gdal` sources)
- Modify: `src/sdb/datasource/gdal/sde_gdal_test.cc`
- Create stub headers for layers so `OgrDataSource` compiles: `ogr_vec_layer.h/.cc` with methods returning empty/false/`SMT_ERR_UNSUPPORTED` until Task 4

**Interfaces:**
- Consumes: `make_gdal_open_target`, `is_db_provider_supported`, `gdal_driver_name`
- Produces: `sdb::datasource::OgrDataSource` subclass of `SmtDataSource` with:
  - `bool Create()`
  - `bool Open()`
  - `bool Close()`
  - `SmtDataSource* Clone() const`
  - layer methods (vector create may still fail until Task 4)
  - `GDALDataset* dataset()` for tests (can be a file-local friend; tests use public `Open`/`IsOpen`)

- [ ] **Step 1: Write the failing test**

Append:

```cpp
  char tmp[MAX_PATH];
  GetTempPathA(MAX_PATH, tmp);
  SmtDataSourceInfo info;
  info.unType = DS_DB_ADO;
  info.unProvider = PROVIDER_GPKG;
  std::strcpy(info.szName, "t");
  std::strcpy(info.db.szService, tmp);
  std::strcpy(info.db.szDBName, "sde_gdal_roundtrip.gpkg");
  std::string path = sdb::datasource::make_gdal_open_target(info);
  DeleteFileA(path.c_str());

  sdb::datasource::OgrDataSource ds;
  ds.SetInfo(info);
  expect(ds.Create(), "GPKG Create");
  expect(ds.Open(), "GPKG Open");
  expect(ds.IsOpen(), "GPKG IsOpen");
  ds.Close();
  expect(!ds.IsOpen(), "GPKG closed");

  sdb::datasource::OgrDataSource acc;
  SmtDataSourceInfo ainfo;
  ainfo.unProvider = PROVIDER_ACCESS;
  acc.SetInfo(ainfo);
  expect(!acc.Open(), "ACCESS Open false");
```

Include `windows.h` for `GetTempPathA` / `DeleteFileA`.

- [ ] **Step 2: Run test to verify it fails**

```bat
build.bat te
```

Expected: `OgrDataSource` not defined.

- [ ] **Step 3: Write minimal implementation**

`gdal_driver.cc`:

```cpp
bool register_gdal_driver() {
  GDALAllRegister();
  return true;
}
```

`OgrDataSource::Open`:

1. If `!is_db_provider_supported(m_dsInfo.unProvider)`: log via `SmtLogManager` channel `SmtSDEGdalDevice` that Access/SQL Server are unsupported; `m_bOpen = false`; return false.
2. `register_gdal_driver()`.
3. `target = make_gdal_open_target(m_dsInfo)`.
4. `GDALOpenEx(target.c_str(), GDAL_OF_VECTOR | GDAL_OF_RASTER | GDAL_OF_UPDATE, nullptr, nullptr, nullptr)`.
5. If null and GPKG/SpatiaLite: `GetGDALDriverManager()->GetDriverByName(gdal_driver_name(...))->Create(target.c_str(), 0, 0, 0, GDT_Unknown, nullptr)`. For SpatiaLite pass options `SPATIALITE=YES` if Create supports papsz.
6. If still null: log `CPLGetLastErrorMsg()` with password redacted (do not log `szPWD`); return false.
7. `m_bOpen = true`; fill `m_vLayerInfos` from `GetLayerCount()`.
8. Install `CPLSetErrorHandler` once that writes CPL messages to that log.

`Create()` can call `Open()`. `Close()` `GDALClose` and `m_bOpen = false`. `Clone()` copies `m_dsInfo` into a new `OgrDataSource`. Tile methods return NULL/false like ADO. Raster create may return NULL until Task 6.

Vector `CreateVectorLayer` / `OpenVectorLayer` may return NULL in this task if the layer class is still a stub; then **do not yet assert layer create in the test**. Only Create/Open/Close/ACCESS.

- [ ] **Step 4: Run the tests and make sure they pass**

```bat
build.bat te
```

Expected: GPKG file created under `%TEMP%`, Access open false, exit 0.

- [ ] **Step 5: Commit**

```bat
git add src/sdb/datasource/gdal
git commit -m "Open GeoPackage datasets through GDAL and reject Access."
```

---

### Task 4: Point layer round-trip on GeoPackage

**Files:**
- Create/complete: `src/sdb/datasource/gdal/ogr_vec_layer.h`
- Create/complete: `src/sdb/datasource/gdal/ogr_vec_layer.cc`
- Modify: `ogr_dataset.cc` `CreateVectorLayer` / `OpenVectorLayer` / `DeleteVectorLayer`
- Modify: `sde_gdal_test.cc`

**Interfaces:**
- Consumes: `OgrDataSource` holding `GDALDataset*`, codec
- Produces: `OgrVectorLayer` implementing `SmtVectorLayer` virtuals. `CreateVectorLayer("dots", rect, SmtFtDot)` creates OGR layer `wkbPoint`. `AppendFeature` → `CreateFeature`. `Fetch` + `GetFeature(0)` returns the point.

- [ ] **Step 1: Write the failing test**

After GPKG Open succeeds (keep dataset open):

```cpp
  fRect rect;
  rect.lb.x = 0;
  rect.lb.y = 0;
  rect.rt.x = 10;
  rect.rt.y = 10;
  SmtVectorLayer* lyr = ds.CreateVectorLayer("dots", rect, SmtFtDot);
  expect(lyr != nullptr, "create dots");
  if (lyr) {
    SmtFeature feat;
    feat.SetID(1);
    feat.SetFeatureType(SmtFtDot);
    feat.SetGeometryDirectly(new SmtPoint(3.0, 4.0));
    expect(lyr->AppendFeature(&feat, true) == SMT_ERR_NONE, "append point");
    expect(lyr->Close(), "close layer");
    SMT_SAFE_DELETE(lyr);
    ds.Close();
    expect(ds.Open(), "reopen gpkg");
    SmtVectorLayer* lyr2 = ds.OpenVectorLayer("dots");
    expect(lyr2 != nullptr, "reopen dots");
    if (lyr2) {
      expect(lyr2->Fetch(FETCH_ALL), "fetch");
      expect(lyr2->GetFeatureCount() >= 1, "count");
      SmtFeature* got = lyr2->GetFeature(0);
      expect(got != nullptr, "get 0");
      const SmtPoint* p = got ? dynamic_cast<const SmtPoint*>(got->GetGeometryRef())
                              : nullptr;
      expect(p && p->GetX() == 3.0 && p->GetY() == 4.0, "xy persist");
      SMT_SAFE_DELETE(lyr2);
    }
  }
```

`AppendFeature(..., true)` must clone if the layer does not take ownership (match mem/ADO: `bclone` true keeps caller’s feature).

- [ ] **Step 2: Run test to verify it fails**

```bat
build.bat te
```

Expected: FAIL `create dots` or `append point`.

- [ ] **Step 3: Write minimal implementation**

`OgrVectorLayer`:

- Members: `OGRLayer* layer_`, `std::vector<SmtFeature*> features_`, `mutable int iterator_`.
- `Create`: `owner dataset()->CreateLayer(name, nullptr, wkbPoint, nullptr)` for `SmtFtDot`.
- `Open`: `GetLayerByName`.
- `AppendFeature`: `OGRFeature ogr(layer_->GetLayerDefn())`; `copy_smt_feature_to_ogr`; `layer_->CreateFeature`; push clone into `features_` if `bclone` else clone anyway for cache (do not delete caller’s feature when `bclone==true`).
- `Fetch`: clear `features_`; `ResetReading`; `GetNextFeature` → codec → push.
- Iterators / `GetFeature` / `GetFeatureByID` / `DeleteFeature` / `UpdateFeature` / `Query` (spatial: `SetSpatialFilter` then fetch into result layer if non-null, else `SMT_ERR_UNSUPPORTED`): follow `SmtMemVecLayer` control flow so all pure virtuals are implemented.
- `StartTransaction` / `CommitTransaction` / `RollbackTransaction`: call `GDALDataset::StartTransaction` on owner when `TestCapability(ODsCTransactions)` (or layer `OLCTransactions`); else `SMT_ERR_NONE` (auto-commit).

`CreateVectorLayer` on the datasource: new `OgrVectorLayer(this)`, set name/type/rect, `Create()`, push `SmtLayerInfo` (`szArchiveName` = layer name).

- [ ] **Step 4: Run the tests and make sure they pass**

```bat
build.bat te
```

Expected: point survives close/reopen.

- [ ] **Step 5: Commit**

```bat
git add src/sdb/datasource/gdal
git commit -m "Round-trip point layers through GeoPackage via OGR."
```

---

### Task 5: Line, polygon, and annotation round-trip

**Files:**
- Modify: `ogr_vec_layer.cc` (`Create` geometry type by `SmtFeatureType`)
- Modify: `ogr_feature_codec.cc` if line/polygon/anno gaps remain
- Modify: `sde_gdal_test.cc`

**Interfaces:**
- Consumes: Task 4 layer + codec
- Produces: `SmtFtCurve` → `wkbLineString`; `SmtFtSurface` → `wkbPolygon`; `SmtFtAnno` → `wkbPoint` plus fields `anno`, `color`, `angle` created on the OGR layer

- [ ] **Step 1: Write the failing test**

For the same GPKG (new temp file or extra layers on the Task 4 file):

- Line: two vertices `(0,0)-(1,1)`, reopen, check `SmtLineString` `GetNumPoints()==2`.
- Polygon: square ring closed, reopen, `SmtPolygon` exterior ring non-null.
- Anno: point + `anno="n"`, `color=3`, `angle=12.0`, reopen, field values match.

- [ ] **Step 2: Run test to verify it fails**

```bat
build.bat te
```

Expected: FAIL until CreateLayer uses the right WKB type / fields.

- [ ] **Step 3: Write minimal implementation**

`Create()` switch:

- `SmtFtDot` / `SmtFtAnno` → `wkbPoint`
- `SmtFtCurve` → `wkbLineString`
- `SmtFtSurface` → `wkbPolygon`

After CreateLayer for annotation, `CreateField` for `anno`/`color`/`angle`. Codec already copies them.

- [ ] **Step 4: Run the tests and make sure they pass**

```bat
build.bat te
```

Expected: all three extra round-trips pass.

- [ ] **Step 5: Commit**

```bat
git add src/sdb/datasource/gdal
git commit -m "Round-trip curve, polygon, and annotation layers through OGR."
```

---

### Task 6: TIN, grid, raster/child-image

**Files:**
- Modify: `ogr_feature_codec.cc` (TIN MultiPolygon, grid MultiPoint)
- Create: `ogr_raster_layer.h/.cc`
- Modify: `ogr_dataset.cc` raster methods
- Modify: `sde_gdal_test.cc`

**Interfaces:**
- Consumes: `SmtTin` (`GetPointCount`, `GetTriangle`, `AddPoint`, `AddTriangle`), `SmtGrid` (`GetSize`, `GetGridNodeBuf`)
- Produces: TIN stored as OGR TIN or MultiPolygon of triangles; grid as MultiPoint plus `grid_row`/`grid_col`; raster `Create` returns false / `CreaterRaster` returns `SMT_ERR_UNSUPPORTED` when the GPKG/PostGIS raster create path is missing — **never** a private blob table

- [ ] **Step 1: Write the failing test**

- TIN: one triangle three points; layer type `SmtFtTin`; after reopen `GetFeatureType()==SmtFtTin` and `SmtTin::GetTriangleCount()>=1` (or `GetPointCount()>=3`).
- Grid: `SmtGrid(2,2)` nodes set; fields `grid_row`/`grid_col`; reopen type `SmtFtGrid`.
- Raster: `CreateRasterLayer` may return a layer whose `Create()` is false **or** a non-null layer whose `CreaterRaster` is `SMT_ERR_UNSUPPORTED`. Assert it does **not** crash and does not create `geom_points` tables. If GPKG raster create works in this SDK, a 1x1 raster round-trip is allowed instead of UNSUPPORTED.

- [ ] **Step 2: Run test to verify it fails**

```bat
build.bat te
```

Expected: TIN/grid create or codec FAIL.

- [ ] **Step 3: Write minimal implementation**

TIN `copy_smt_feature_to_ogr`: build `OGRMultiPolygon`; each triangle → ring of 3 vertices + close. Set geometry. `copy_ogr_feature_to_smt`: if dest/layer type is `SmtFtTin` (or WKB TIN/MultiPolygon on a tin layer), fill `SmtTin`.

Grid: dump `Matrix2D<RawPoint>` as `OGRMultiPoint`; set `grid_row`/`grid_col` from `GetSize`. Reverse: allocate `SmtGrid(nRow,nCol)` and fill nodes in order.

Raster layer: wrap GDAL raster if `dataset()->GetRasterCount()>0`; else `Create()` false and `CreaterRaster` → `SMT_ERR_UNSUPPORTED` + log. Tiles stay NULL like ADO.

- [ ] **Step 4: Run the tests and make sure they pass**

```bat
build.bat te
```

Expected: TIN/grid persist; raster is either UNSUPPORTED or a real GDAL raster — not ADO blobs.

- [ ] **Step 5: Commit**

```bat
git add src/sdb/datasource/gdal
git commit -m "Store TIN and grid through OGR; raster uses GDAL or UNSUPPORTED."
```

---

### Task 7: Device manager, drop ADO from `src_all`, optional PostGIS test

**Files:**
- Modify: `src/sdb/datasource/mgr/datasourcemgr.cpp`
- Modify: `src/sdb/datasource/mgr/BUILD.gn`
- Modify: `src/sdb/datasource/BUILD.gn`
- Modify: `src/sdb/BUILD.gn`
- Modify: `src/BUILD.gn`
- Modify: `sde_gdal_test.cc` (`SMT_PG_DSN`)
- Modify: `src/app/app_core/app_smtapp.cpp`

**Interfaces:**
- Consumes: `OgrDataSource`
- Produces: `CreateDataSource` / `CreateTmpDataSource` for `DS_DB_ADO` allocate `sdb::datasource::OgrDataSource`. URL sprintf unchanged. `src_all` no longer depends on `//src/ado:ado`.

- [ ] **Step 1: Write the failing test**

Add at end of `main`:

```cpp
  const char* pg = std::getenv("SMT_PG_DSN");
  if (pg && pg[0]) {
    SmtDataSourceInfo pgi;
    pgi.unType = DS_DB_ADO;
    pgi.unProvider = PROVIDER_POSTGRES;
    std::strcpy(pgi.szName, "pg");
    // If SMT_PG_DSN starts with PG:, put it in szService/szDBName via a small
    // parser or set szService=host and szDBName from the env in the test only.
    // Minimum: if the string starts with "PG:", Open using a test-only
    // SetUrl is NOT enough — Open builds from info. Parse host/dbname/user/password
    // from libpq-style "host=… dbname=… user=… password=…" or from PG:.
    sdb::datasource::OgrDataSource pgds;
    pgds.SetInfo(pgi);
    expect(pgds.Open(), "SMT_PG_DSN Open");
  }
```

Also add a test that goes through `SmtDataSourceMgr::CreateTmpDataSource(DS_DB_ADO)` after wiring: `Open` with GPKG info succeeds. That test will fail until mgr is switched.

- [ ] **Step 2: Run test to verify it fails**

```bat
build.bat te
```

Expected: `CreateTmpDataSource` still returns `SmtAdoDataSource` (Access) so GPKG Open fails; or mgr still includes ADO.

- [ ] **Step 3: Write minimal implementation**

`datasourcemgr.cpp`:

- Replace `#include "ado.h"` with `"sdb/datasource/gdal/ogr_dataset.h"`.
- Drop `using namespace Smt_SDEAdo`.
- Both `DS_DB_ADO` cases: `pDS = new sdb::datasource::OgrDataSource();`
- Keep URL `sprintf_s` as today.

`mgr/BUILD.gn`: drop `//src/ado:ado`, `//src/sdb/datasource/ado:sde_ado`, `comsuppw.lib`; add `//src/sdb/datasource/gdal:sde_gdal`.

`datasource/BUILD.gn` and `src/sdb/BUILD.gn`: replace `sde_ado` with `sde_gdal`; drop `gdal_seam` in favor of `sde_gdal`.

`src/BUILD.gn` `src_all`: remove `//src/ado:ado`.

`app_smtapp.cpp`: look for `sample1.gpkg` instead of `sample1.mdb`; `unProvider = PROVIDER_GPKG`. If the file is missing, keep empty catalog (`bRet = true`).

Optional PostGIS: only when `SMT_PG_DSN` is set; skip silently otherwise.

- [ ] **Step 4: Run the tests and make sure they pass**

```bat
build.bat
build.bat te
```

Expected: `//:all` green **without** linking `SmtAdoCore` / `SmtSDEAdoDevice`. `sde_gdal_test` exit 0. Ninja graph for `all` must not list those two DLLs as required.

- [ ] **Step 5: Commit**

```bat
git add src/sdb/datasource/mgr src/sdb/datasource/BUILD.gn src/sdb/BUILD.gn src/BUILD.gn src/sdb/datasource/gdal src/app/app_core/app_smtapp.cpp
git commit -m "Route database datasources through OGR and drop ADO from src_all."
```

---

### Task 8: SMF uses shared codec; docs

**Files:**
- Modify: `src/sdb/datasource/smf/smf_ogrsupport.cpp`
- Modify: `src/sdb/datasource/smf/smf_ogrsupport.h` (thin wrappers calling codec, keep existing function names)
- Modify: `src/sdb/datasource/smf/BUILD.gn` (`deps += ":ogr_codec"` — use `//src/sdb/datasource/gdal:ogr_codec`)
- Modify: `docs/build/src-layout.md`, `src/README.md`, `docs/README.md` (already links spec), root `README.md` only if it still describes ADO as the DB path
- Modify: spec status line to `accepted`

**Interfaces:**
- Consumes: `copy_ogr_feature_to_smt`
- Produces: `CopyOGRFeaToSmtFea` calls the codec (behavior for point/line/polygon unchanged aside from dropping random colors **only if SMF tests/visuals require those colors** — if SMF currently depends on random colors, keep that in `CopyOGRFeaToSmtFea` **after** the codec returns, not inside the codec)

- [ ] **Step 1: Write the failing test**

No new SMF exe test required. Extend `sde_gdal_test` only if SMF linkage breaks. Compile `sde_smf` as the check: `ninja -C out SmtSDESmfDevice` (or the actual ninja target name from `gn ls out //src/sdb/datasource/smf:sde_smf`).

- [ ] **Step 2: Run test to verify it fails**

If you change SMF to call the codec before adding the dep, the build fails. That is the red bar.

- [ ] **Step 3: Write minimal implementation**

`CopyOGRFeaToSmtFea` → `return sdb::datasource::copy_ogr_feature_to_smt(...)`. Keep SMF-only style randomization after a successful copy if that code stays in `smf_ogrsupport.cpp`.

Docs (English in new/changed comments; Chinese OK in existing README tables):

- `src-layout.md`: `datasource/gdal` is `SmtSDEGdalDevice`; `src/ado` not in `src_all`.
- `src/README.md`: same.
- Root README module/hard-dep text: GDAL is the DB provider; ADO is leftover sources. Refresh **最后更新** to 2026-09-13 if you touch it.

- [ ] **Step 4: Run the tests and make sure they pass**

```bat
build.bat
build.bat te
```

Expected: both green. SMF still links GDAL.

- [ ] **Step 5: Commit**

```bat
git add src/sdb/datasource/smf docs/build/src-layout.md src/README.md docs/README.md README.md docs/superpowers/specs/2026-09-13-ogr-db-datasource-design.md
git commit -m "Share the OGR feature codec with SMF and document the GDAL DB provider."
```

---

## Self-review

1. **Spec coverage:** Open targets, GPKG/PostGIS/SpatiaLite, ACCESS reject, one vector layer class, TIN/grid/anno/child-image, no `DS_TB`, no ADO in `src_all`, GPKG tests always, PostGIS via `SMT_PG_DSN`, docs — each has a task.
2. **Placeholders:** None. Raster may be UNSUPPORTED; that is specified, not TBD.
3. **Types:** `OgrDataSource`, `OgrVectorLayer`, `OgrRasterLayer`, `is_db_provider_supported`, `make_gdal_open_target`, `copy_ogr_feature_to_smt`, `copy_smt_feature_to_ogr` used consistently.

Work on `master` only when executing.
