// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/datasource/gdal/gdal_driver.h"
#include "sdb/datasource/gdal/ogr_connect.h"
#include "sdb/datasource/gdal/ogr_dataset.h"
#include "sdb/datasource/gdal/ogr_feature_codec.h"
#include "sdb/datasource/gdal/sdbd_gdal_driver.h"
#include "sdb/datasource/gdal/sdbd_handler.h"

#include "datasourcemgr.h"

#include "feature.h"
#include "geometry.h"
#include "layer.h"
#include "matrix2d.h"
#include "style.h"

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
using Smt_GIS::SmtFtAnno;
using Smt_GIS::SmtFtCurve;
using Smt_GIS::SmtFtDot;
using Smt_GIS::SmtFtGrid;
using Smt_GIS::SmtFtSurface;
using Smt_GIS::SmtFtTin;
using Smt_GIS::SmtRasterLayer;
using Smt_GIS::DS_DB_ADO;
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

void run_sdbd_driver_tests() {
  expect(sdb::datasource::register_gdal_driver(), "register_gdal_driver");
  expect(sdb::datasource::register_sdbd_driver(), "register_sdbd_driver");
  GDALDriver* sdbd =
      GetGDALDriverManager()->GetDriverByName(sdb::datasource::kSdbdDriverName);
  expect(sdbd != nullptr, "GetDriverByName(SDBD)");
  if (!sdbd) {
    return;
  }

  GDALDataset* ds = static_cast<GDALDataset*>(GDALOpenEx(
      "SDBD:MEM:sdbd_point", GDAL_OF_VECTOR | GDAL_OF_UPDATE, nullptr, nullptr,
      nullptr));
  if (!ds) {
    ds = sdbd->Create("SDBD:MEM:sdbd_point", 0, 0, 0, GDT_Unknown, nullptr);
  }
  expect(ds != nullptr, "GDALOpenEx/Create SDBD:MEM");
  if (!ds) {
    std::fprintf(stderr, "SKIP: SDBD Memory-backed open failed\n");
    return;
  }

  OGRLayer* lyr = ds->CreateLayer("pts", nullptr, wkbPoint, nullptr);
  expect(lyr != nullptr, "SDBD CreateLayer pts");
  if (lyr) {
    OGRFieldDefn name("name", OFTString);
    lyr->CreateField(&name);
    OGRFeature feat(lyr->GetLayerDefn());
    OGRPoint pt(10.0, 20.0);
    feat.SetGeometry(&pt);
    feat.SetField("name", "a");
    expect(lyr->CreateFeature(&feat) == OGRERR_NONE, "SDBD CreateFeature point");
    expect(lyr->GetFeatureCount() >= 1, "SDBD point count");
    lyr->ResetReading();
    OGRFeature* got = lyr->GetNextFeature();
    expect(got != nullptr, "SDBD GetNextFeature");
    if (got) {
      const OGRGeometry* geom = got->GetGeometryRef();
      const bool is_pt =
          geom && wkbFlatten(geom->getGeometryType()) == wkbPoint;
      const auto* back = is_pt ? geom->toPoint() : nullptr;
      expect(back && back->getX() == 10.0 && back->getY() == 20.0,
             "SDBD point coords");
      OGRFeature::DestroyFeature(got);
    }
  }
  expect(ds->GetLayerCount() >= 1, "SDBD GetLayerCount");
  GDALClose(ds);

  sdb::datasource::SdbdHandler handler;
  int status = 0;
  std::string body;
  handler.handle("GET", "/sdbd/api/v1/health", "", &status, &body);
  expect(status == 200 && body.find("\"service\":\"sdbd\"") != std::string::npos,
         "sdbd health");
  handler.handle("POST", "/sdbd/api/v1/datasource/open",
                 "{\"driver\":\"Memory\",\"target\":\"sdbd_ipc\"}", &status,
                 &body);
  expect(status == 200, "sdbd datasource/open Memory");
  handler.handle("POST", "/sdbd/api/v1/layers",
                 "{\"name\":\"pts\",\"crs\":\"EPSG:4326\",\"geom_type\":\"point\","
                 "\"geom_column\":\"geom\",\"fields\":[{\"name\":\"name\","
                 "\"type\":\"string\"}]}",
                 &status, &body);
  expect(status == 200 && body.find("\"name\":\"pts\"") != std::string::npos,
         "sdbd create layer");
  handler.handle("GET", "/sdbd/api/v1/catalog", "", &status, &body);
  expect(status == 200 && body.find("pts") != std::string::npos,
         "sdbd list catalog");
  handler.handle("GET", "/sdbd/api/v1/layers/pts", "", &status, &body);
  expect(status == 200 && body.find("point") != std::string::npos,
         "sdbd open layer");
  handler.handle("POST", "/sdbd/api/v1/recordset/append",
                 "{\"layer\":\"pts\",\"features\":[{\"geom_wkt\":\"POINT (1 2)\","
                 "\"attrs\":{\"name\":\"a\"}}]}",
                 &status, &body);
  expect(status == 200 && body.find("\"count\":1") != std::string::npos,
         "sdbd append point");
  handler.handle("POST", "/sdbd/api/v1/recordset/query",
                 "{\"layer\":\"pts\",\"bbox\":{\"min_x\":0,\"min_y\":0,"
                 "\"max_x\":10,\"max_y\":10},\"predicate\":\"intersects\"}",
                 &status, &body);
  expect(status == 200 && body.find("POINT") != std::string::npos,
         "sdbd query point");
  handler.handle("POST", "/sdbd/api/v1/recordset/open",
                 "{\"layer\":\"pts\"}", &status, &body);
  expect(status == 200 && body.find("\"handle\"") != std::string::npos,
         "sdbd recordset/open");
  std::string handle;
  {
    const std::string key = "\"handle\":\"";
    const auto pos = body.find(key);
    if (pos != std::string::npos) {
      const auto start = pos + key.size();
      const auto end = body.find('"', start);
      if (end != std::string::npos) {
        handle = body.substr(start, end - start);
      }
    }
  }
  if (!handle.empty()) {
    const std::string fetch =
        std::string("{\"handle\":\"") + handle + "\",\"offset\":0,\"limit\":10}";
    handler.handle("POST", "/sdbd/api/v1/recordset/fetch", fetch, &status,
                   &body);
    expect(status == 200 && body.find("POINT") != std::string::npos,
           "sdbd recordset/fetch");
    const std::string close =
        std::string("{\"handle\":\"") + handle + "\"}";
    handler.handle("POST", "/sdbd/api/v1/recordset/close", close, &status,
                   &body);
    expect(status == 200, "sdbd recordset/close");
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
  run_sdbd_driver_tests();
  GDALDriver* mem = GetGDALDriverManager()->GetDriverByName("Memory");
  expect(mem != nullptr, "Memory OGR driver");
  if (mem) {
    GDALDataset* ds = mem->Create("codec_mem", 0, 0, 0, GDT_Unknown, nullptr);
    expect(ds != nullptr, "Memory datasource");
    if (ds) {
      OGRFieldDefn anno("anno", OFTString);
      OGRFieldDefn color("color", OFTInteger);
      OGRFieldDefn angle("angle", OFTReal);
      OGRFieldDefn style("style", OFTBinary);
      OGRLayer* lyr = ds->CreateLayer("pts", nullptr, wkbPoint, nullptr);
      expect(lyr != nullptr, "Memory point layer");
      if (lyr) {
        lyr->CreateField(&anno);
        lyr->CreateField(&color);
        lyr->CreateField(&angle);
        lyr->CreateField(&style);
        SmtPoint smt_pt(1.5, 2.5);
        OGRFeature ogr(lyr->GetLayerDefn());
        expect(sdb::datasource::encode_smt_geometry(&smt_pt, &ogr, SmtFtAnno),
               "smt geom->ogr anno");
        ogr.SetField("anno", "hi");
        ogr.SetField("color", 9);
        ogr.SetField("angle", 45.0);
        SmtStyle sty;
        sty.SetStyleName("codec_style");
        SmtPenDesc pen = sty.GetPenDesc();
        pen.lPenColor = 0x00aabb;
        sty.SetPenDesc(pen);
        sdb::datasource::copy_smt_style_to_ogr(&sty, &ogr);
        int style_n = 0;
        const int style_i = ogr.GetFieldIndex("style");
        expect(style_i >= 0 &&
                   ogr.GetFieldAsBinary(style_i, &style_n) != nullptr &&
                   style_n == static_cast<int>(sizeof(SmtStyle)),
               "style OFTBinary written");
        SmtGeometry* back_g =
            sdb::datasource::decode_ogr_geometry(&ogr, SmtFtAnno);
        const SmtPoint* pt = dynamic_cast<const SmtPoint*>(back_g);
        expect(pt && pt->GetX() == 1.5 && pt->GetY() == 2.5, "anno xy");
        expect(ogr.GetFieldIndex("anno") >= 0, "anno field present");
        SmtStyle* back_sty = sdb::datasource::copy_ogr_style_from_ogr(&ogr);
        expect(back_sty &&
                   std::strcmp(back_sty->GetStyleName(), "codec_style") == 0,
               "style blob decode");
        delete back_g;
        delete back_sty;
      }

      OGRLayer* multi_lyr =
          ds->CreateLayer("multi", nullptr, wkbUnknown, nullptr);
      expect(multi_lyr != nullptr, "Memory multi layer");
      if (multi_lyr) {
        OGRFeature mpl(multi_lyr->GetLayerDefn());
        OGRMultiPoint mp;
        OGRPoint p0(9.0, 8.0);
        OGRPoint p1(1.0, 2.0);
        mp.addGeometry(&p0);
        mp.addGeometry(&p1);
        mpl.SetGeometry(&mp);
        SmtGeometry* mp_back =
            sdb::datasource::decode_ogr_geometry(&mpl, SmtFtDot);
        const SmtPoint* mpt = dynamic_cast<const SmtPoint*>(mp_back);
        expect(mpt && mpt->GetX() == 9.0 && mpt->GetY() == 8.0,
               "MultiPoint first part");
        delete mp_back;

        OGRFeature mls(multi_lyr->GetLayerDefn());
        OGRLineString ls0;
        ls0.addPoint(0.0, 0.0);
        ls0.addPoint(2.0, 3.0);
        OGRLineString ls1;
        ls1.addPoint(4.0, 5.0);
        ls1.addPoint(6.0, 7.0);
        OGRMultiLineString mls_g;
        mls_g.addGeometry(&ls0);
        mls_g.addGeometry(&ls1);
        mls.SetGeometry(&mls_g);
        SmtGeometry* mls_back =
            sdb::datasource::decode_ogr_geometry(&mls, SmtFtCurve);
        const SmtLineString* got_ls =
            dynamic_cast<const SmtLineString*>(mls_back);
        expect(got_ls && got_ls->GetNumPoints() == 2 && got_ls->GetX(1) == 2.0,
               "MultiLineString first part");
        delete mls_back;

        OGRLinearRing ring;
        ring.addPoint(0.0, 0.0);
        ring.addPoint(1.0, 0.0);
        ring.addPoint(1.0, 1.0);
        ring.addPoint(0.0, 0.0);
        ring.closeRings();
        OGRPolygon poly0;
        poly0.addRing(&ring);
        OGRPolygon poly1;
        poly1.addRing(&ring);
        OGRMultiPolygon mpg;
        mpg.addGeometry(&poly0);
        mpg.addGeometry(&poly1);
        OGRFeature mpgf(multi_lyr->GetLayerDefn());
        mpgf.SetGeometry(&mpg);
        SmtGeometry* mpg_back =
            sdb::datasource::decode_ogr_geometry(&mpgf, SmtFtSurface);
        const SmtPolygon* got_pg = dynamic_cast<const SmtPolygon*>(mpg_back);
        expect(got_pg && got_pg->GetExteriorRing() != nullptr,
               "MultiPolygon first part");
        delete mpg_back;
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

  GDALDriver* gpkg_drv = GetGDALDriverManager()->GetDriverByName("GPKG");
  const bool can_file = gpkg_drv != nullptr;

  sdb::datasource::OgrDataSource ds;
  ds.SetInfo(info);
  if (!can_file) {
    expect(!ds.Create(), "GPKG Create fails without GPKG driver");
    expect(!ds.Open(), "GPKG Open fails without GPKG driver");
    std::fprintf(stderr,
                 "SKIP: GPKG driver not in this GDAL; file round-trip omitted\n");
  } else {
    expect(ds.Create(), "GPKG Create");
    expect(ds.Open(), "GPKG Open");
    expect(ds.IsOpen(), "GPKG IsOpen");
    if (ds.IsOpen() && ds.dataset()) {
      expect(std::filesystem::exists(path), "created .gpkg file");
      GDALDriver* opened = ds.dataset()->GetDriver();
      expect(opened && std::strcmp(opened->GetDescription(), "GPKG") == 0,
             "opened driver is GPKG");
    }
  }

  fRect rect;
  rect.lb.x = 0;
  rect.lb.y = 0;
  rect.rt.x = 10;
  rect.rt.y = 10;
  if (!ds.IsOpen()) {
    // File round-trip requires a real GPKG driver.
  } else {
  OGRLayer* lyr = ds.CreateVectorLayer("dots", rect, SmtFtDot);
  expect(lyr != nullptr, "create dots");
  if (lyr) {
    OGRFeature feat(lyr->GetLayerDefn());
    SmtPoint smt_pt(3.0, 4.0);
    expect(sdb::datasource::encode_smt_geometry(&smt_pt, &feat, SmtFtDot),
           "encode point");
    expect(lyr->CreateFeature(&feat) == OGRERR_NONE, "append point");
    ds.Close();
    expect(ds.Open(), "reopen gpkg");
    OGRLayer* lyr2 = ds.OpenVectorLayer("dots");
    expect(lyr2 != nullptr, "reopen dots");
    if (lyr2) {
      expect(lyr2->GetFeatureCount() >= 1, "count");
      lyr2->ResetReading();
      OGRFeature* got = lyr2->GetNextFeature();
      expect(got != nullptr, "get 0");
      SmtGeometry* g =
          got ? sdb::datasource::decode_ogr_geometry(got, SmtFtDot) : nullptr;
      const SmtPoint* p = dynamic_cast<const SmtPoint*>(g);
      expect(p && p->GetX() == 3.0 && p->GetY() == 4.0, "xy persist");
      delete g;
      OGRFeature::DestroyFeature(got);
    }
  }

  OGRLayer* lines = ds.CreateVectorLayer("lines", rect, SmtFtCurve);
  expect(lines != nullptr, "create lines");
  if (lines) {
    auto* line = new SmtLineString();
    line->SetNumPoints(2);
    line->SetPoint(0, 0.0, 0.0);
    line->SetPoint(1, 1.0, 1.0);
    OGRFeature feat(lines->GetLayerDefn());
    expect(sdb::datasource::encode_smt_geometry(line, &feat, SmtFtCurve),
           "encode line");
    expect(lines->CreateFeature(&feat) == OGRERR_NONE, "append line");
    delete line;
    ds.Close();
    expect(ds.Open(), "reopen for line");
    OGRLayer* back = ds.OpenVectorLayer("lines");
    expect(back != nullptr, "reopen lines");
    if (back) {
      back->ResetReading();
      OGRFeature* got = back->GetNextFeature();
      SmtGeometry* g =
          got ? sdb::datasource::decode_ogr_geometry(got, SmtFtCurve) : nullptr;
      const SmtLineString* ls = dynamic_cast<const SmtLineString*>(g);
      expect(ls && ls->GetNumPoints() == 2, "line points");
      delete g;
      OGRFeature::DestroyFeature(got);
    }
  }

  OGRLayer* polys = ds.CreateVectorLayer("polys", rect, SmtFtSurface);
  expect(polys != nullptr, "create polys");
  if (polys) {
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
    OGRFeature feat(polys->GetLayerDefn());
    expect(sdb::datasource::encode_smt_geometry(poly, &feat, SmtFtSurface),
           "encode poly");
    expect(polys->CreateFeature(&feat) == OGRERR_NONE, "append poly");
    delete poly;
    ds.Close();
    expect(ds.Open(), "reopen for poly");
    OGRLayer* back = ds.OpenVectorLayer("polys");
    expect(back != nullptr, "reopen polys");
    if (back) {
      back->ResetReading();
      OGRFeature* got = back->GetNextFeature();
      SmtGeometry* g =
          got ? sdb::datasource::decode_ogr_geometry(got, SmtFtSurface)
              : nullptr;
      const SmtPolygon* pg = dynamic_cast<const SmtPolygon*>(g);
      expect(pg && pg->GetExteriorRing() != nullptr, "poly ring");
      delete g;
      OGRFeature::DestroyFeature(got);
    }
  }

  OGRLayer* annos = ds.CreateVectorLayer("annos", rect, SmtFtAnno);
  expect(annos != nullptr, "create annos");
  if (annos) {
    SmtPoint anno_pt(2.0, 3.0);
    OGRFeature feat(annos->GetLayerDefn());
    expect(sdb::datasource::encode_smt_geometry(&anno_pt, &feat, SmtFtAnno),
           "encode anno");
    feat.SetField("anno", "n");
    feat.SetField("color", 3);
    feat.SetField("angle", 12.0);
    expect(annos->CreateFeature(&feat) == OGRERR_NONE, "append anno");
    ds.Close();
    expect(ds.Open(), "reopen for anno");
    OGRLayer* back = ds.OpenVectorLayer("annos");
    expect(back != nullptr, "reopen annos");
    if (back) {
      back->ResetReading();
      OGRFeature* got = back->GetNextFeature();
      expect(got != nullptr, "anno feat");
      if (got) {
        const int ai = got->GetFieldIndex("anno");
        expect(ai >= 0 && std::strcmp(got->GetFieldAsString(ai), "n") == 0,
               "anno text persist");
        const int ci = got->GetFieldIndex("color");
        expect(ci >= 0 && got->GetFieldAsInteger(ci) == 3, "anno color persist");
        const int gi = got->GetFieldIndex("angle");
        expect(gi >= 0 && got->GetFieldAsDouble(gi) == 12.0,
               "anno angle persist");
      }
      OGRFeature::DestroyFeature(got);
    }
  }

  OGRLayer* tins = ds.CreateVectorLayer("tins", rect, SmtFtTin);
  expect(tins != nullptr, "create tins");
  if (tins) {
    SmtTin tin;
    SmtPoint a(0, 0);
    SmtPoint b(1, 0);
    SmtPoint c(0, 1);
    tin.AddPoint(&a);
    tin.AddPoint(&b);
    tin.AddPoint(&c);
    SmtTriangle tri;
    tri.a = 0;
    tri.b = 1;
    tri.c = 2;
    tin.AddTriangle(&tri);
    OGRFeature feat(tins->GetLayerDefn());
    expect(sdb::datasource::encode_smt_geometry(&tin, &feat, SmtFtTin),
           "encode tin");
    expect(tins->CreateFeature(&feat) == OGRERR_NONE, "append tin");
    ds.Close();
    expect(ds.Open(), "reopen for tin");
    OGRLayer* back = ds.OpenVectorLayer("tins");
    expect(back != nullptr, "reopen tins");
    if (back) {
      back->ResetReading();
      OGRFeature* got = back->GetNextFeature();
      SmtGeometry* g =
          got ? sdb::datasource::decode_ogr_geometry(got, SmtFtTin) : nullptr;
      const SmtTin* gt = dynamic_cast<const SmtTin*>(g);
      expect(gt && (gt->GetTriangleCount() >= 1 || gt->GetPointCount() >= 3),
             "tin persist");
      delete g;
      OGRFeature::DestroyFeature(got);
    }
  }

  OGRLayer* grids = ds.CreateVectorLayer("grids", rect, SmtFtGrid);
  expect(grids != nullptr, "create grids");
  if (grids) {
    SmtGrid grid(2, 2);
    Matrix2D<RawPoint>* buf = grid.GetGridNodeBuf();
    RawPoint p00(0, 0);
    RawPoint p01(1, 0);
    RawPoint p10(0, 1);
    RawPoint p11(1, 1);
    buf->SetElement(p00, 0, 0);
    buf->SetElement(p01, 0, 1);
    buf->SetElement(p10, 1, 0);
    buf->SetElement(p11, 1, 1);
    OGRFeature feat(grids->GetLayerDefn());
    expect(sdb::datasource::encode_smt_geometry(&grid, &feat, SmtFtGrid),
           "encode grid");
    feat.SetField("grid_row", 2);
    feat.SetField("grid_col", 2);
    expect(grids->CreateFeature(&feat) == OGRERR_NONE, "append grid");
    ds.Close();
    expect(ds.Open(), "reopen for grid");
    OGRLayer* back = ds.OpenVectorLayer("grids");
    expect(back != nullptr, "reopen grids");
    if (back) {
      back->ResetReading();
      OGRFeature* got = back->GetNextFeature();
      expect(got && sdb::datasource::infer_feature_type(got, SmtFtGrid) ==
                        SmtFtGrid,
             "grid type");
      OGRFeature::DestroyFeature(got);
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
  }

  sdb::datasource::OgrDataSource acc;
  SmtDataSourceInfo ainfo;
  ainfo.unProvider = PROVIDER_ACCESS;
  acc.SetInfo(ainfo);
  expect(!acc.Open(), "ACCESS Open false");

  {
    fRect rrect;
    rrect.lb.x = 0;
    rrect.lb.y = 0;
    rrect.rt.x = 1;
    rrect.rt.y = 1;
    SmtRasterLayer* ras = acc.CreateRasterLayer("ras", rrect, 0);
    expect(ras != nullptr, "raster layer object");
    if (ras) {
      expect(!ras->Create(), "raster Create false when bands cannot be written");
      expect(ras->CreaterRaster(nullptr, 0, rrect, 0) == SMT_ERR_UNSUPPORTED,
             "raster create unsupported (no blob table)");
      SMT_SAFE_DELETE(ras);
    }
  }

  Smt_SDEDevMgr::SmtDataSourceMgr* mgr =
      Smt_SDEDevMgr::SmtDataSourceMgr::GetSingletonPtr();
  expect(mgr != nullptr, "datasource mgr");
  if (mgr) {
    sdb::datasource::OgrDataSource* tmp = mgr->CreateTmpDataSource(DS_DB_ADO);
    expect(tmp != nullptr, "CreateTmpDataSource OGR");
    if (tmp) {
      tmp->SetInfo(info);
      if (can_file) {
        expect(tmp->Open(), "mgr GPKG Open");
        tmp->Close();
      } else {
        expect(!tmp->Open(), "mgr GPKG Open fails without driver");
      }
      mgr->DestoryTmpDataSource(tmp);
    }
    Smt_SDEDevMgr::ScratchLayer scratch =
        Smt_SDEDevMgr::SmtDataSourceMgr::CreateMemVecLayer();
    expect(scratch.dataset != nullptr && scratch.layer != nullptr,
           "CreateMemVecLayer Memory");
    if (scratch.layer) {
      OGRFeature feat(scratch.layer->GetLayerDefn());
      OGRPoint pt(1.0, 2.0);
      feat.SetGeometry(&pt);
      expect(scratch.layer->CreateFeature(&feat) == OGRERR_NONE,
             "scratch CreateFeature");
      expect(scratch.layer->GetFeatureCount() >= 1, "scratch count");
    }
    Smt_SDEDevMgr::SmtDataSourceMgr::DestoryMemVecLayer(scratch);
  }

  const char* pg_dsn = std::getenv("SMT_PG_DSN");
  if (pg_dsn && pg_dsn[0]) {
    SmtDataSourceInfo pgi;
    pgi.unType = DS_DB_ADO;
    pgi.unProvider = PROVIDER_POSTGRES;
    std::strcpy(pgi.szName, "pg");
    std::string dsn = pg_dsn;
    if (dsn.rfind("PG:", 0) == 0) {
      dsn = dsn.substr(3);
    }
    auto take = [&](const char* key, char* dest, size_t dest_len) {
      const std::string token = std::string(key) + "=";
      const auto pos = dsn.find(token);
      if (pos == std::string::npos) {
        return;
      }
      auto end = dsn.find(' ', pos);
      if (end == std::string::npos) {
        end = dsn.size();
      }
      const std::string val = dsn.substr(pos + token.size(), end - pos - token.size());
      std::strncpy(dest, val.c_str(), dest_len - 1);
    };
    char host[128] = "127.0.0.1";
    char port[16] = "5432";
    take("host", host, sizeof(host));
    take("port", port, sizeof(port));
    take("dbname", pgi.db.szDBName, sizeof(pgi.db.szDBName));
    take("user", pgi.szUID, sizeof(pgi.szUID));
    take("password", pgi.szPWD, sizeof(pgi.szPWD));
    std::snprintf(pgi.db.szService, sizeof(pgi.db.szService), "%s:%s", host,
                  port);
    sdb::datasource::OgrDataSource pgds;
    pgds.SetInfo(pgi);
    expect(pgds.Open(), "SMT_PG_DSN Open");
    if (pgds.IsOpen()) {
      pgds.Close();
    }
  }

  if (g_fails) {
    std::fprintf(stderr, "%d checks failed\n", g_fails);
    return 1;
  }
  std::printf("sde_gdal_test connect checks ok\n");
  return 0;
}
