// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/datasource/gdal/gdal_driver.h"
#include "sdb/datasource/gdal/ogr_connect.h"
#include "sdb/datasource/gdal/ogr_feature_codec.h"
#include "sdb/datasource/gdal/ogr_raster_layer.h"
#include "sdb/datasource/gdal/sdbd_dataset.h"
#include "sdb/datasource/gdal/sdbd_gdal_driver.h"
#include "sdb/datasource/gdal/sdbd_handler.h"
#include "sdb/datasource/gdal/sdbd_layer.h"

#include "sdb/datasource/mgr/datasourcemgr.h"

#include "sdb/feature/feature.h"
#include "algorithm/geo/geometry.h"
#include "sdb/layer/layer.h"
#include "algorithm/geo/matrix2d.h"
#include "sdb/carto/style.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

using namespace geo;
using base::fRect;
using base::SmtTriangle;
using sdb::SmtDataSourceInfo;
using sdb::SmtFtAnno;
using sdb::SmtFtCurve;
using sdb::SmtFtDot;
using sdb::SmtFtGrid;
using sdb::SmtFtSurface;
using sdb::SmtFtTin;
using sdb::SmtFtUnknown;
using sdb::SmtRasterLayer;
using sdb::DS_DB_ADO;
using sdb::PROVIDER_ACCESS;
using sdb::PROVIDER_GPKG;
using sdb::PROVIDER_POSTGRES;
using sdb::PROVIDER_SPATIALITE;
using sdb::PROVIDER_SQLSERVER;

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
  expect(sdb::datasource::as_sdbd_dataset(ds) != nullptr,
         "SDBD:MEM is SdbdDataset");

  OGRLayer* lyr = ds->CreateLayer("pts", nullptr, wkbPoint, nullptr);
  expect(lyr != nullptr, "SDBD CreateLayer pts");
  expect(sdb::datasource::as_sdbd_layer(lyr) != nullptr,
         "CreateLayer returns SdbdLayer");
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
  expect(ds->GetDriver() != nullptr &&
             std::strcmp(ds->GetDriver()->GetDescription(), "SDBD") == 0,
         "GetDriver is SDBD");
  {
    auto* sdbd_ds = sdb::datasource::as_sdbd_dataset(ds);
    expect(sdbd_ds && sdbd_ds->inner(), "inner stock dataset");
    if (sdbd_ds && sdbd_ds->inner() && sdbd_ds->inner()->GetRasterCount() > 0) {
      double outer_gt[6] = {};
      double inner_gt[6] = {};
      const CPLErr outer_err = ds->GetGeoTransform(outer_gt);
      const CPLErr inner_err = sdbd_ds->inner()->GetGeoTransform(inner_gt);
      expect(outer_err == inner_err, "raster GetGeoTransform forwards");
    }
  }
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
        OGRPoint smt_pt(1.5, 2.5);
        OGRFeature ogr(lyr->GetLayerDefn());
        expect(sdb::datasource::encode_smt_geometry(&smt_pt, &ogr, SmtFtAnno),
               "smt geom->ogr anno");
        ogr.SetField("anno", "hi");
        ogr.SetField("color", 9);
        ogr.SetField("angle", 45.0);
        SmtStyle sty;
        sty.set_style_name("codec_style");
        SmtPenDesc pen = sty.get_pen_desc();
        pen.lPenColor = 0x00aabb;
        sty.set_pen_desc(pen);
        sdb::datasource::copy_smt_style_to_ogr(&sty, &ogr);
        int style_n = 0;
        const int style_i = ogr.GetFieldIndex("style");
        expect(style_i >= 0 &&
                   ogr.GetFieldAsBinary(style_i, &style_n) != nullptr &&
                   style_n == static_cast<int>(sizeof(SmtStyle)),
               "style OFTBinary written");
        OGRGeometry* back_g =
            sdb::datasource::decode_ogr_geometry(&ogr, SmtFtAnno);
        const OGRPoint* pt = dynamic_cast<const OGRPoint*>(back_g);
        expect(pt && pt->getX() == 1.5 && pt->getY() == 2.5, "anno xy");
        expect(ogr.GetFieldIndex("anno") >= 0, "anno field present");
        expect(sdb::datasource::infer_feature_type(&ogr, SmtFtUnknown) ==
                   SmtFtAnno,
               "nonempty anno is SmtFtAnno");
        ogr.SetField("anno", "");
        expect(sdb::datasource::infer_feature_type(&ogr, SmtFtUnknown) ==
                   SmtFtDot,
               "empty anno stays SmtFtDot");
        ogr.SetField("anno", "hi");
        SmtStyle* back_sty = sdb::datasource::copy_ogr_style_from_ogr(&ogr);
        expect(back_sty &&
                   std::strcmp(back_sty->get_style_name(), "codec_style") == 0,
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
        OGRGeometry* mp_back =
            sdb::datasource::decode_ogr_geometry(&mpl, SmtFtDot);
        const OGRPoint* mpt = dynamic_cast<const OGRPoint*>(mp_back);
        expect(mpt && mpt->getX() == 9.0 && mpt->getY() == 8.0,
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
        OGRGeometry* mls_back =
            sdb::datasource::decode_ogr_geometry(&mls, SmtFtCurve);
        const OGRMultiLineString* got_mls =
            dynamic_cast<const OGRMultiLineString*>(mls_back);
        expect(got_mls && got_mls->getNumGeometries() == 2,
               "MultiLineString keeps all parts");
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
        OGRGeometry* mpg_back =
            sdb::datasource::decode_ogr_geometry(&mpgf, SmtFtSurface);
        const OGRMultiPolygon* got_mpg =
            dynamic_cast<const OGRMultiPolygon*>(mpg_back);
        expect(got_mpg && got_mpg->getNumGeometries() == 2,
               "MultiPolygon keeps all parts");
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
  const std::string sdbd_target = sdb::datasource::make_sdbd_open_target(info);
  expect(sdbd_target.rfind("SDBD:GPKG:", 0) == 0, "SDBD:GPKG target");
  DeleteFileA(path.c_str());

  GDALDriver* gpkg_drv = GetGDALDriverManager()->GetDriverByName("GPKG");
  const bool can_file = gpkg_drv != nullptr;
  GDALDataset* gdal_ds = sdb::datasource::open_sdbd_dataset(info);
  if (!can_file) {
    expect(gdal_ds == nullptr, "GPKG Open fails without GPKG driver");
    std::fprintf(stderr,
                 "SKIP: GPKG driver not in this GDAL; file round-trip omitted\n");
  } else {
    expect(gdal_ds != nullptr, "SDBD:GPKG open/create");
    expect(sdb::datasource::as_sdbd_dataset(gdal_ds) != nullptr,
           "GPKG open returns SdbdDataset");
    if (gdal_ds) {
      expect(std::filesystem::exists(path), "created .gpkg file");
    }
  }

  fRect rect;
  rect.lb.x = 0;
  rect.lb.y = 0;
  rect.rt.x = 10;
  rect.rt.y = 10;
  auto* sdbd_ds = sdb::datasource::as_sdbd_dataset(gdal_ds);
  if (!sdbd_ds) {
    // File round-trip requires a real GPKG driver.
  } else {
  OGRLayer* lyr = sdbd_ds->create_sdbd_layer("dots", SmtFtDot);
  expect(lyr != nullptr, "create dots");
  if (lyr) {
    OGRFeature feat(lyr->GetLayerDefn());
    OGRPoint smt_pt(3.0, 4.0);
    expect(sdb::datasource::encode_smt_geometry(&smt_pt, &feat, SmtFtDot),
           "encode point");
    expect(lyr->CreateFeature(&feat) == OGRERR_NONE, "append point");
    GDALClose(gdal_ds);
    gdal_ds = sdb::datasource::open_sdbd_dataset(info);
    sdbd_ds = sdb::datasource::as_sdbd_dataset(gdal_ds);
    OGRLayer* lyr2 = sdbd_ds ? sdbd_ds->GetLayerByName("dots") : nullptr;
    expect(lyr2 != nullptr, "reopen dots");
    if (lyr2) {
      expect(lyr2->GetFeatureCount() >= 1, "count");
      lyr2->ResetReading();
      OGRFeature* got = lyr2->GetNextFeature();
      expect(got != nullptr, "get 0");
      OGRGeometry* g =
          got ? sdb::datasource::decode_ogr_geometry(got, SmtFtDot) : nullptr;
      const OGRPoint* p = dynamic_cast<const OGRPoint*>(g);
      expect(p && p->getX() == 3.0 && p->getY() == 4.0, "xy persist");
      delete g;
      OGRFeature::DestroyFeature(got);
    }
  }

  OGRLayer* lines = sdbd_ds ? sdbd_ds->create_sdbd_layer("lines", SmtFtCurve)
                            : nullptr;
  expect(lines != nullptr, "create lines");
  if (lines) {
    auto* line = new OGRLineString();
    line->setNumPoints(2);
    line->setPoint(0, 0.0, 0.0);
    line->setPoint(1, 1.0, 1.0);
    OGRFeature feat(lines->GetLayerDefn());
    expect(sdb::datasource::encode_smt_geometry(line, &feat, SmtFtCurve),
           "encode line");
    expect(lines->CreateFeature(&feat) == OGRERR_NONE, "append line");
    delete line;
    GDALClose(gdal_ds);
    gdal_ds = sdb::datasource::open_sdbd_dataset(info);
    sdbd_ds = sdb::datasource::as_sdbd_dataset(gdal_ds);
    OGRLayer* back = sdbd_ds ? sdbd_ds->GetLayerByName("lines") : nullptr;
    expect(back != nullptr, "reopen lines");
    if (back) {
      back->ResetReading();
      OGRFeature* got = back->GetNextFeature();
      OGRGeometry* g =
          got ? sdb::datasource::decode_ogr_geometry(got, SmtFtCurve) : nullptr;
      const OGRLineString* ls = dynamic_cast<const OGRLineString*>(g);
      expect(ls && ls->getNumPoints() == 2, "line points");
      delete g;
      OGRFeature::DestroyFeature(got);
    }
  }

  OGRLayer* polys = sdbd_ds ? sdbd_ds->create_sdbd_layer("polys", SmtFtSurface)
                            : nullptr;
  expect(polys != nullptr, "create polys");
  if (polys) {
    auto* ring = new OGRLinearRing();
    ring->setNumPoints(5);
    ring->setPoint(0, 0.0, 0.0);
    ring->setPoint(1, 1.0, 0.0);
    ring->setPoint(2, 1.0, 1.0);
    ring->setPoint(3, 0.0, 1.0);
    ring->setPoint(4, 0.0, 0.0);
    ring->closeRings();
    auto* poly = new OGRPolygon();
    poly->addRingDirectly(ring);
    OGRFeature feat(polys->GetLayerDefn());
    expect(sdb::datasource::encode_smt_geometry(poly, &feat, SmtFtSurface),
           "encode poly");
    expect(polys->CreateFeature(&feat) == OGRERR_NONE, "append poly");
    delete poly;
    GDALClose(gdal_ds);
    gdal_ds = sdb::datasource::open_sdbd_dataset(info);
    sdbd_ds = sdb::datasource::as_sdbd_dataset(gdal_ds);
    OGRLayer* back = sdbd_ds ? sdbd_ds->GetLayerByName("polys") : nullptr;
    expect(back != nullptr, "reopen polys");
    if (back) {
      back->ResetReading();
      OGRFeature* got = back->GetNextFeature();
      OGRGeometry* g =
          got ? sdb::datasource::decode_ogr_geometry(got, SmtFtSurface)
              : nullptr;
      const OGRPolygon* pg = dynamic_cast<const OGRPolygon*>(g);
      expect(pg && pg->getExteriorRing() != nullptr, "poly ring");
      delete g;
      OGRFeature::DestroyFeature(got);
    }
  }

  OGRLayer* annos = sdbd_ds ? sdbd_ds->create_sdbd_layer("annos", SmtFtAnno)
                            : nullptr;
  expect(annos != nullptr, "create annos");
  if (annos) {
    OGRPoint anno_pt(2.0, 3.0);
    OGRFeature feat(annos->GetLayerDefn());
    expect(sdb::datasource::encode_smt_geometry(&anno_pt, &feat, SmtFtAnno),
           "encode anno");
    feat.SetField("anno", "n");
    feat.SetField("color", 3);
    feat.SetField("angle", 12.0);
    expect(annos->CreateFeature(&feat) == OGRERR_NONE, "append anno");
    GDALClose(gdal_ds);
    gdal_ds = sdb::datasource::open_sdbd_dataset(info);
    sdbd_ds = sdb::datasource::as_sdbd_dataset(gdal_ds);
    OGRLayer* back = sdbd_ds ? sdbd_ds->GetLayerByName("annos") : nullptr;
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

  OGRLayer* tins = sdbd_ds ? sdbd_ds->create_sdbd_layer("tins", SmtFtTin)
                           : nullptr;
  expect(tins != nullptr, "create tins");
  if (tins) {
    SmtTin tin;
    OGRPoint a(0, 0);
    OGRPoint b(1, 0);
    OGRPoint c(0, 1);
    tin.add_point(&a);
    tin.add_point(&b);
    tin.add_point(&c);
    SmtTriangle tri;
    tri.a = 0;
    tri.b = 1;
    tri.c = 2;
    tin.add_triangle(&tri);
    OGRFeature feat(tins->GetLayerDefn());
    expect(sdb::datasource::encode_smt_geometry(&tin, &feat, SmtFtTin),
           "encode tin");
    expect(tins->CreateFeature(&feat) == OGRERR_NONE, "append tin");
    GDALClose(gdal_ds);
    gdal_ds = sdb::datasource::open_sdbd_dataset(info);
    sdbd_ds = sdb::datasource::as_sdbd_dataset(gdal_ds);
    OGRLayer* back = sdbd_ds ? sdbd_ds->GetLayerByName("tins") : nullptr;
    expect(back != nullptr, "reopen tins");
    if (back) {
      back->ResetReading();
      OGRFeature* got = back->GetNextFeature();
      SmtTin* gt = got ? sdb::datasource::decode_smt_tin(got) : nullptr;
      expect(gt && (gt->get_triangle_count() >= 1 || gt->get_point_count() >= 3),
             "tin persist");
      delete gt;
      OGRFeature::DestroyFeature(got);
    }
  }

  OGRLayer* grids = sdbd_ds ? sdbd_ds->create_sdbd_layer("grids", SmtFtGrid)
                            : nullptr;
  expect(grids != nullptr, "create grids");
  if (grids) {
    SmtGrid grid(2, 2);
    grid.set_node(0, 0, RawPoint(0, 0));
    grid.set_node(0, 1, RawPoint(1, 0));
    grid.set_node(1, 0, RawPoint(0, 1));
    grid.set_node(1, 1, RawPoint(1, 1));
    OGRFeature feat(grids->GetLayerDefn());
    expect(sdb::datasource::encode_smt_geometry(&grid, &feat, SmtFtGrid),
           "encode grid");
    feat.SetField("grid_row", 2);
    feat.SetField("grid_col", 2);
    expect(grids->CreateFeature(&feat) == OGRERR_NONE, "append grid");
    GDALClose(gdal_ds);
    gdal_ds = sdb::datasource::open_sdbd_dataset(info);
    sdbd_ds = sdb::datasource::as_sdbd_dataset(gdal_ds);
    OGRLayer* back = sdbd_ds ? sdbd_ds->GetLayerByName("grids") : nullptr;
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

  auto* ras = new sdb::datasource::OgrRasterLayer(nullptr);
  expect(ras != nullptr, "raster layer object");
  if (ras) {
    expect(ras->Create(), "raster MEM Create");
    const char payload[] = "ras-bytes";
    const long cr =
        ras->CreaterRaster(payload, static_cast<long>(sizeof(payload)), rect, 7);
    expect(cr == SMT_ERR_NONE, "raster CreaterRaster");
    char* got = nullptr;
    long got_size = 0;
    long got_code = -1;
    fRect got_rect;
    expect(ras->GetRasterNoClone(got, got_size, got_rect, got_code) ==
               SMT_ERR_NONE,
           "GetRasterNoClone");
    expect(got && got_size == static_cast<long>(sizeof(payload)) &&
               std::memcmp(got, payload, sizeof(payload)) == 0,
           "raster blob round-trip");
    expect(got_code == 7, "raster image code");
    expect(got_rect.rt.x == rect.rt.x && got_rect.rt.y == rect.rt.y,
           "raster rect");
    expect(ras->GetDataset() != nullptr &&
               ras->GetDataset()->GetRasterCount() > 0,
           "raster hangs GDALDataset bands");
    SMT_SAFE_DELETE(ras);
  }

  if (gdal_ds) {
    GDALClose(gdal_ds);
    gdal_ds = nullptr;
  }
  }

  SmtDataSourceInfo ainfo;
  ainfo.unProvider = PROVIDER_ACCESS;
  expect(sdb::datasource::make_sdbd_open_target(ainfo).empty(),
         "ACCESS has no SDBD target");
  expect(sdb::datasource::open_sdbd_dataset(ainfo) == nullptr,
         "ACCESS Open false");

  {
    fRect rrect;
    rrect.lb.x = 0;
    rrect.lb.y = 0;
    rrect.rt.x = 1;
    rrect.rt.y = 1;
    GDALDriver* mem_ras = GetGDALDriverManager()->GetDriverByName("MEM");
    if (!mem_ras) {
      std::fprintf(stderr, "SKIP: MEM raster driver missing\n");
    } else {
      auto* ras = new sdb::datasource::OgrRasterLayer(nullptr);
      expect(ras->Create(), "standalone MEM raster Create");
      expect(ras->CreaterRaster(nullptr, 0, rrect, 0) == SMT_ERR_NONE,
             "empty CreaterRaster ok");
      SMT_SAFE_DELETE(ras);
    }
  }

  sdb::SmtDataSourceMgr* mgr =
      sdb::SmtDataSourceMgr::get_singleton_ptr();
  expect(mgr != nullptr, "datasource mgr");
  if (mgr) {
    GDALDataset* tmp = mgr->CreateTmpDataSource(DS_MEM);
    expect(tmp != nullptr && sdb::datasource::as_sdbd_dataset(tmp) != nullptr,
           "CreateTmpDataSource MEM is SdbdDataset");
    mgr->DestoryTmpDataSource(tmp);
    GDALDataset* file_ds = mgr->OpenDataset(info);
    if (can_file) {
      expect(file_ds != nullptr, "mgr GPKG Open");
    } else {
      expect(file_ds == nullptr, "mgr GPKG Open fails without driver");
    }
    mgr->CloseDataset(file_ds);
    sdb::ScratchLayer scratch =
        sdb::SmtDataSourceMgr::CreateMemVecLayer();
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
    sdb::SmtDataSourceMgr::DestoryMemVecLayer(scratch);
    sdb::SmtRasterLayer* mem_ras =
        sdb::SmtDataSourceMgr::CreateMemRasLayer();
    expect(mem_ras != nullptr, "CreateMemRasLayer");
    if (mem_ras) {
      expect(dynamic_cast<sdb::datasource::OgrRasterLayer*>(mem_ras) != nullptr,
             "CreateMemRasLayer is OgrRasterLayer");
      expect(mem_ras->IsOpen() && mem_ras->GetDataset() != nullptr,
             "CreateMemRasLayer open with GDALDataset");
      const char bytes[] = {1, 2, 3, 4};
      fRect rr;
      rr.lb.x = 0;
      rr.lb.y = 0;
      rr.rt.x = 2;
      rr.rt.y = 2;
      expect(mem_ras->CreaterRaster(bytes, 4, rr, 3) == SMT_ERR_NONE,
             "mgr ras CreaterRaster");
      char* out = nullptr;
      long out_n = 0;
      long out_code = 0;
      fRect out_r;
      expect(mem_ras->GetRasterNoClone(out, out_n, out_r, out_code) ==
                 SMT_ERR_NONE &&
             out_n == 4 && out && out[0] == 1 && out_code == 3,
             "mgr ras GetRasterNoClone");
      sdb::SmtDataSourceMgr::DestoryMemRasLayer(mem_ras);
    }

    // Open(file) must backfill /vsimem so GetRasterNoClone works for GDI.
    {
      GDALDriver* gtiff = GetGDALDriverManager()->GetDriverByName("GTiff");
      if (!gtiff) {
        std::fprintf(stderr, "SKIP: GTiff driver missing for Open blob test\n");
      } else {
        namespace fs = std::filesystem;
        const fs::path dir =
            fs::temp_directory_path() / "smartgis_ogr_ras_open_test";
        std::error_code ec;
        fs::remove_all(dir, ec);
        fs::create_directories(dir, ec);
        const fs::path tif = dir / "one.tif";
        GDALDataset* created = gtiff->Create(tif.string().c_str(), 2, 2, 1,
                                             GDT_Byte, nullptr);
        expect(created != nullptr, "GTiff Create for Open test");
        if (created) {
          GDALRasterBand* band = created->GetRasterBand(1);
          unsigned char px[4] = {9, 8, 7, 6};
          band->RasterIO(GF_Write, 0, 0, 2, 2, px, 2, 2, GDT_Byte, 0, 0);
          GDALClose(created);
          auto* file_ras = new sdb::datasource::OgrRasterLayer(nullptr);
          expect(file_ras->Open(tif.string().c_str()), "OgrRasterLayer Open tif");
          char* blob = nullptr;
          long blob_n = 0;
          long blob_code = -1;
          fRect blob_r;
          expect(file_ras->GetRasterNoClone(blob, blob_n, blob_r, blob_code) ==
                     SMT_ERR_NONE &&
                 blob && blob_n > 0,
                 "Open file GetRasterNoClone has bytes");
          SMT_SAFE_DELETE(file_ras);
        }
        fs::remove_all(dir, ec);
      }
    }
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
    GDALDataset* pgds = sdb::datasource::open_sdbd_dataset(pgi);
    expect(pgds != nullptr, "SMT_PG_DSN Open");
    if (pgds) {
      expect(sdb::datasource::as_sdbd_dataset(pgds) != nullptr,
             "PG open returns SdbdDataset");
      GDALClose(pgds);
    }
  }

  if (g_fails) {
    std::fprintf(stderr, "%d checks failed\n", g_fails);
    return 1;
  }
  std::printf("sde_gdal_test connect checks ok\n");
  return 0;
}
