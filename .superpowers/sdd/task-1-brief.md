### Task 1: Providers and GDAL open targets

**Files:**
- Modify: `src/gis/layer/layer.h` (`eSmtDBProvider`)
- Create: `src/gis/datasource/gdal/ogr_connect.h`
- Create: `src/gis/datasource/gdal/ogr_connect.cc`
- Create: `src/gis/datasource/gdal/sde_gdal_test.cc`
- Modify: `src/gis/datasource/gdal/BUILD.gn`
- Modify: `BUILD.gn` (`test_all`)

**Interfaces:**
- Consumes: `Smt_GIS::SmtDataSourceInfo`, existing `PROVIDER_ACCESS` / `PROVIDER_SQLSERVER`
- Produces:
  - `gis::datasource::is_db_provider_supported(uint provider) -> bool`
  - `gis::datasource::gdal_driver_name(uint provider) -> const char*` (`"PostgreSQL"`, `"GPKG"`, `"SQLite"`, or `nullptr`)
  - `gis::datasource::make_gdal_open_target(const SmtDataSourceInfo& info) -> std::string`

- [ ] **Step 1: Write the failing test**

Create `src/gis/datasource/gdal/sde_gdal_test.cc`:

```cpp
// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/datasource/gdal/ogr_connect.h"

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
  expect(!gis::datasource::is_db_provider_supported(PROVIDER_ACCESS),
         "ACCESS unsupported");
  expect(!gis::datasource::is_db_provider_supported(PROVIDER_SQLSERVER),
         "SQLSERVER unsupported");
  expect(gis::datasource::is_db_provider_supported(PROVIDER_GPKG),
         "GPKG supported");
  expect(gis::datasource::is_db_provider_supported(PROVIDER_POSTGRES),
         "POSTGRES supported");
  expect(gis::datasource::is_db_provider_supported(PROVIDER_SPATIALITE),
         "SPATIALITE supported");

  expect(std::strcmp(gis::datasource::gdal_driver_name(PROVIDER_GPKG), "GPKG") ==
             0,
         "GPKG driver name");
  expect(std::strcmp(gis::datasource::gdal_driver_name(PROVIDER_POSTGRES),
                     "PostgreSQL") == 0,
         "PG driver name");
  expect(std::strcmp(gis::datasource::gdal_driver_name(PROVIDER_SPATIALITE),
                     "SQLite") == 0,
         "SQLite driver name");
  expect(gis::datasource::gdal_driver_name(PROVIDER_ACCESS) == nullptr,
         "ACCESS has no driver");

  SmtDataSourceInfo gpkg;
  gpkg.unProvider = PROVIDER_GPKG;
  std::strcpy(gpkg.db.szService, "C:\\tmp\\ds");
  std::strcpy(gpkg.db.szDBName, "sample1.gpkg");
  std::string gpkg_path = gis::datasource::make_gdal_open_target(gpkg);
  expect(gpkg_path.find("sample1.gpkg") != std::string::npos, "GPKG path");
  expect(gpkg_path.find("PG:") == std::string::npos, "GPKG is not PG:");

  SmtDataSourceInfo pg;
  pg.unProvider = PROVIDER_POSTGRES;
  std::strcpy(pg.db.szService, "127.0.0.1:5432");
  std::strcpy(pg.db.szDBName, "gis");
  std::strcpy(pg.szUID, "u");
  std::strcpy(pg.szPWD, "secret");
  std::string pg_target = gis::datasource::make_gdal_open_target(pg);
  expect(pg_target.find("PG:") == 0, "PG prefix");
  expect(pg_target.find("host=127.0.0.1") != std::string::npos, "PG host");
  expect(pg_target.find("port=5432") != std::string::npos, "PG port");
  expect(pg_target.find("dbname=gis") != std::string::npos, "PG dbname");
  expect(pg_target.find("user=u") != std::string::npos, "PG user");
  expect(pg_target.find("password=secret") != std::string::npos, "PG password");

  SmtDataSourceInfo access;
  access.unProvider = PROVIDER_ACCESS;
  expect(gis::datasource::make_gdal_open_target(access).empty(),
         "ACCESS target empty");

  if (g_fails) {
    std::fprintf(stderr, "%d checks failed\n", g_fails);
    return 1;
  }
  std::printf("sde_gdal_test connect checks ok\n");
  return 0;
}
```

Append to `eSmtDBProvider` in `src/gis/layer/layer.h` **after** `PROVIDER_MYSQL` (do not reorder):

```cpp
		PROVIDER_MYSQL,				//MySQL 数据库
		PROVIDER_POSTGRES,			// PostgreSQL / PostGIS
		PROVIDER_GPKG,				// GeoPackage file
		PROVIDER_SPATIALITE,			// SpatiaLite file
```

Replace `src/gis/datasource/gdal/BUILD.gn` with:

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
    "//src/gis/map:gis",
  ]
}

smt_shared_library("sde_gdal") {
  dll_stem = "SmtSDEGdalDevice"
  defines = [ "Export_SmtSDEGdalDevice" ]
  sources = [
    "gdal_driver.cc",
    "gdal_driver.h",
  ]
  include_dirs += [ "//src" ]
  deps = [
    ":ogr_codec",
    "//src/base:core",
    "//src/gis/map:gis",
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
    "//src/gis/map:gis",
  ]
}
```

Keep `gdal_driver.cc` compiling (still returns false). Add to root `BUILD.gn` `group("test_all")`:

```gn
group("test_all") {
  testonly = true
  deps = [
    "//testing/e2e:exe_smoke",
    "//src/gis/datasource/gdal:sde_gdal_test",
  ]
}
```

- [ ] **Step 2: Run test to verify it fails**

```bat
build.bat te
```

Expected: link or compile fails (`ogr_connect.h` missing) **or** if you stub empty files first, `sde_gdal_test` exits 1. Do not implement `make_gdal_open_target` before this run.

- [ ] **Step 3: Write minimal implementation**

`src/gis/datasource/gdal/ogr_connect.h`:

```cpp
// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_DATASOURCE_GDAL_OGR_CONNECT_H_
#define SDB_DATASOURCE_GDAL_OGR_CONNECT_H_

#include "layer.h"

#include <string>

namespace gis {
namespace datasource {

bool is_db_provider_supported(uint provider);
const char* gdal_driver_name(uint provider);
std::string make_gdal_open_target(const Smt_GIS::SmtDataSourceInfo& info);

}  // namespace datasource
}  // namespace gis

#endif  // SDB_DATASOURCE_GDAL_OGR_CONNECT_H_
```

`src/gis/datasource/gdal/ogr_connect.cc`:

```cpp
// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/datasource/gdal/ogr_connect.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace gis {
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
}  // namespace gis
```

- [ ] **Step 4: Run the tests and make sure they pass**

```bat
build.bat te
```

Expected: `out\sde_gdal_test.exe` prints `sde_gdal_test connect checks ok` and exit 0. `exe_smoke` may skip missing product exes.

- [ ] **Step 5: Commit**

```bat
git add src/gis/layer/layer.h src/gis/datasource/gdal/ogr_connect.h src/gis/datasource/gdal/ogr_connect.cc src/gis/datasource/gdal/sde_gdal_test.cc src/gis/datasource/gdal/BUILD.gn BUILD.gn
git commit -m "Add OGR DB provider ids and GDAL open-target helper."
```

---

