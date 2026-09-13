// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/datasource/gdal/ogr_connect.h"
#include "sdb/datasource/gdal/ogr_dataset.h"
#include "sdb/datasource/gdal/ogr_feature_codec.h"

#include "feature.h"
#include "geometry.h"
#include "layer.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

using namespace Smt_Geo;
using Smt_Core::fRect;
using Smt_Core::SmtTriangle;
using Smt_GIS::SmtDataSourceInfo;
using Smt_GIS::SmtFeature;
using Smt_GIS::SmtField;
using Smt_GIS::SmtFtAnno;
using Smt_GIS::SmtFtCurve;
using Smt_GIS::SmtFtDot;
using Smt_GIS::SmtFtGrid;
using Smt_GIS::SmtFtSurface;
using Smt_GIS::SmtFtTin;
using Smt_GIS::SmtRasterLayer;
using Smt_GIS::SmtVectorLayer;
using Smt_GIS::DS_DB_ADO;
using Smt_GIS::FETCH_ALL;
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
  {
    std::string dir = path;
    const auto slash = dir.find_last_of("\\/");
    const auto dot = dir.find_last_of('.');
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
      dir.resize(dot);
    }
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
  }

  sdb::datasource::OgrDataSource ds;
  ds.SetInfo(info);
  expect(ds.Create(), "GPKG Create");
  expect(ds.Open(), "GPKG Open");
  expect(ds.IsOpen(), "GPKG IsOpen");

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

  SmtVectorLayer* lines = ds.CreateVectorLayer("lines", rect, SmtFtCurve);
  expect(lines != nullptr, "create lines");
  if (lines) {
    SmtFeature feat;
    feat.SetID(2);
    feat.SetFeatureType(SmtFtCurve);
    auto* line = new SmtLineString();
    line->SetNumPoints(2);
    line->SetPoint(0, 0.0, 0.0);
    line->SetPoint(1, 1.0, 1.0);
    feat.SetGeometryDirectly(line);
    expect(lines->AppendFeature(&feat, true) == SMT_ERR_NONE, "append line");
    lines->Close();
    SMT_SAFE_DELETE(lines);
    ds.Close();
    expect(ds.Open(), "reopen for line");
    SmtVectorLayer* back = ds.OpenVectorLayer("lines");
    expect(back != nullptr, "reopen lines");
    if (back) {
      back->Fetch(FETCH_ALL);
      SmtFeature* got = back->GetFeature(0);
      const SmtLineString* ls =
          got ? dynamic_cast<const SmtLineString*>(got->GetGeometryRef())
              : nullptr;
      expect(ls && ls->GetNumPoints() == 2, "line points");
      SMT_SAFE_DELETE(back);
    }
  }

  SmtVectorLayer* polys = ds.CreateVectorLayer("polys", rect, SmtFtSurface);
  expect(polys != nullptr, "create polys");
  if (polys) {
    SmtFeature feat;
    feat.SetID(3);
    feat.SetFeatureType(SmtFtSurface);
    auto* ring = new SmtLinearRing();
    ring->SetNumPoints(5);
    ring->SetPoint(0, 0.0, 0.0);
    ring->SetPoint(1, 1.0, 0.0);
    ring->SetPoint(2, 1.0, 1.0);
    ring->SetPoint(3, 0.0, 1.0);
    ring->SetPoint(4, 0.0, 0.0);
    ring->CloseRings();
    auto* poly = new SmtPolygon();
    poly->AddRingDirectly(ring);
    feat.SetGeometryDirectly(poly);
    expect(polys->AppendFeature(&feat, true) == SMT_ERR_NONE, "append poly");
    polys->Close();
    SMT_SAFE_DELETE(polys);
    ds.Close();
    expect(ds.Open(), "reopen for poly");
    SmtVectorLayer* back = ds.OpenVectorLayer("polys");
    expect(back != nullptr, "reopen polys");
    if (back) {
      back->Fetch(FETCH_ALL);
      SmtFeature* got = back->GetFeature(0);
      const SmtPolygon* pg =
          got ? dynamic_cast<const SmtPolygon*>(got->GetGeometryRef()) : nullptr;
      expect(pg && pg->GetExteriorRing() != nullptr, "poly ring");
      SMT_SAFE_DELETE(back);
    }
  }

  SmtVectorLayer* annos = ds.CreateVectorLayer("annos", rect, SmtFtAnno);
  expect(annos != nullptr, "create annos");
  if (annos) {
    SmtFeature feat;
    feat.SetID(4);
    feat.SetFeatureType(SmtFtAnno);
    feat.SetGeometryDirectly(new SmtPoint(2.0, 3.0));
    feat.SetFieldValue(feat.GetFieldIndexByName("anno"), "n");
    feat.SetFieldValue(feat.GetFieldIndexByName("color"), 3);
    feat.SetFieldValue(feat.GetFieldIndexByName("angle"), 12.0);
    expect(annos->AppendFeature(&feat, true) == SMT_ERR_NONE, "append anno");
    annos->Close();
    SMT_SAFE_DELETE(annos);
    ds.Close();
    expect(ds.Open(), "reopen for anno");
    SmtVectorLayer* back = ds.OpenVectorLayer("annos");
    expect(back != nullptr, "reopen annos");
    if (back) {
      back->Fetch(FETCH_ALL);
      SmtFeature* got = back->GetFeature(0);
      expect(got != nullptr, "anno feat");
      if (got) {
        const int ai = got->GetFieldIndexByName("anno");
        expect(ai >= 0 && std::strcmp(got->GetAttributeRef()->GetFieldPtr(ai)
                                          ->GetValueAsString(),
                                      "n") == 0,
               "anno text persist");
        const int ci = got->GetFieldIndexByName("color");
        expect(ci >= 0 &&
                   got->GetAttributeRef()->GetFieldPtr(ci)->GetValueAsInteger() ==
                       3,
               "anno color persist");
        const int gi = got->GetFieldIndexByName("angle");
        expect(gi >= 0 &&
                   got->GetAttributeRef()->GetFieldPtr(gi)->GetValueAsDouble() ==
                       12.0,
               "anno angle persist");
      }
      SMT_SAFE_DELETE(back);
    }
  }

  SmtVectorLayer* tins = ds.CreateVectorLayer("tins", rect, SmtFtTin);
  expect(tins != nullptr, "create tins");
  if (tins) {
    SmtFeature feat;
    feat.SetID(5);
    feat.SetFeatureType(SmtFtTin);
    auto* tin = new SmtTin();
    SmtPoint a(0, 0);
    SmtPoint b(1, 0);
    SmtPoint c(0, 1);
    tin->AddPoint(&a);
    tin->AddPoint(&b);
    tin->AddPoint(&c);
    SmtTriangle tri;
    tri.a = 0;
    tri.b = 1;
    tri.c = 2;
    tin->AddTriangle(&tri);
    feat.SetGeometryDirectly(tin);
    expect(tins->AppendFeature(&feat, true) == SMT_ERR_NONE, "append tin");
    tins->Close();
    SMT_SAFE_DELETE(tins);
    ds.Close();
    expect(ds.Open(), "reopen for tin");
    SmtVectorLayer* back = ds.OpenVectorLayer("tins");
    expect(back != nullptr, "reopen tins");
    if (back) {
      back->Fetch(FETCH_ALL);
      SmtFeature* got = back->GetFeature(0);
      expect(got && got->GetFeatureType() == SmtFtTin, "tin type");
      const SmtTin* gt =
          got ? dynamic_cast<const SmtTin*>(got->GetGeometryRef()) : nullptr;
      expect(gt && (gt->GetTriangleCount() >= 1 || gt->GetPointCount() >= 3),
             "tin persist");
      SMT_SAFE_DELETE(back);
    }
  }

  SmtVectorLayer* grids = ds.CreateVectorLayer("grids", rect, SmtFtGrid);
  expect(grids != nullptr, "create grids");
  if (grids) {
    SmtFeature feat;
    feat.SetID(6);
    feat.SetFeatureType(SmtFtGrid);
    auto* grid = new SmtGrid(2, 2);
    Matrix2D<RawPoint>* buf = grid->GetGridNodeBuf();
    RawPoint p00(0, 0);
    RawPoint p01(1, 0);
    RawPoint p10(0, 1);
    RawPoint p11(1, 1);
    buf->SetElement(p00, 0, 0);
    buf->SetElement(p01, 0, 1);
    buf->SetElement(p10, 1, 0);
    buf->SetElement(p11, 1, 1);
    feat.SetGeometryDirectly(grid);
    SmtField fr;
    fr.SetName("grid_row");
    fr.SetType(SmtInteger);
    feat.AddField(fr);
    SmtField fc;
    fc.SetName("grid_col");
    fc.SetType(SmtInteger);
    feat.AddField(fc);
    feat.SetFieldValue(feat.GetFieldIndexByName("grid_row"), 2);
    feat.SetFieldValue(feat.GetFieldIndexByName("grid_col"), 2);
    expect(grids->AppendFeature(&feat, true) == SMT_ERR_NONE, "append grid");
    grids->Close();
    SMT_SAFE_DELETE(grids);
    ds.Close();
    expect(ds.Open(), "reopen for grid");
    SmtVectorLayer* back = ds.OpenVectorLayer("grids");
    expect(back != nullptr, "reopen grids");
    if (back) {
      back->Fetch(FETCH_ALL);
      SmtFeature* got = back->GetFeature(0);
      expect(got && got->GetFeatureType() == SmtFtGrid, "grid type");
      SMT_SAFE_DELETE(back);
    }
  }

  SmtRasterLayer* ras = ds.CreateRasterLayer("ras", rect, 0);
  expect(ras != nullptr, "raster layer object");
  if (ras) {
    const bool created = ras->Create();
    const long cr = ras->CreaterRaster(nullptr, 0, rect, 0);
    expect(!created || cr == SMT_ERR_UNSUPPORTED || cr == SMT_ERR_NONE,
           "raster not ado blob");
    expect(cr == SMT_ERR_UNSUPPORTED || created, "raster unsupported or ok");
    SMT_SAFE_DELETE(ras);
  }

  ds.Close();
  expect(!ds.IsOpen(), "GPKG closed");

  sdb::datasource::OgrDataSource acc;
  SmtDataSourceInfo ainfo;
  ainfo.unProvider = PROVIDER_ACCESS;
  acc.SetInfo(ainfo);
  expect(!acc.Open(), "ACCESS Open false");

  if (g_fails) {
    std::fprintf(stderr, "%d checks failed\n", g_fails);
    return 1;
  }
  std::printf("sde_gdal_test connect checks ok\n");
  return 0;
}
