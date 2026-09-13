// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/datasource/gdal/ogr_connect.h"
#include "sdb/datasource/gdal/ogr_feature_codec.h"

#include "feature.h"
#include "geometry.h"
#include "layer.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using Smt_GIS::SmtDataSourceInfo;
using Smt_GIS::SmtFeature;
using Smt_GIS::SmtField;
using Smt_GIS::SmtFtAnno;
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

  GDALAllRegister();
  GDALDriver* mem = GetGDALDriverManager()->GetDriverByName("Memory");
  expect(mem != nullptr, "Memory OGR driver");
  if (mem) {
    GDALDataset* ds = mem->Create("codec_mem", 0, 0, 0, GDT_Unknown, nullptr);
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
      GDALClose(ds);
    }
  }

  if (g_fails) {
    std::fprintf(stderr, "%d checks failed\n", g_fails);
    return 1;
  }
  std::printf("sde_gdal_test connect checks ok\n");
  return 0;
}
