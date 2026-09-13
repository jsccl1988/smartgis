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
