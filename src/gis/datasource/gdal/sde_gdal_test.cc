// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

#include "gis/geo/ops/indexed_tin.h"
#include "gdal_priv.h"
#include "ogrsf_frmts.h"
#include "gis/datasource/gdal/gdal_driver.h"
#include "gis/datasource/session/connection_spec.h"
#include "gis/datasource/session/data_session.h"
#include "gis/datasource/ogr/ogr_connect.h"
#include "gis/datasource/ogr/ogr_feature_codec.h"
#include "gis/datasource/ogr/ogr_raster_layer.h"
#include "gis/datasource/sdbd/sdbd_dataset.h"
#include "gis/datasource/sdbd/sdbd_driver.h"
#include "gis/datasource/sdbd/sdbd_layer.h"
#include "gis/envelope.h"
#include "gis/feature/feature.h"
#include "gis/map/layer_kind.h"
#include "base/process/switches.h"

using namespace geo;
using gis::Envelope;
using gis::DS_DB_ADO;
using gis::PROVIDER_ACCESS;
using gis::PROVIDER_GPKG;
using gis::PROVIDER_POSTGRES;
using gis::PROVIDER_SPATIALITE;
using gis::PROVIDER_SQLSERVER;
using gis::datasource::ConnectionSpec;
using gis::VectorSchema;

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

void run_sdbd_driver_tests() {
  expect(gis::datasource::register_gdal_driver(), "register_gdal_driver");
  expect(gis::datasource::register_sdbd_driver(), "register_sdbd_driver");
  GDALDriver* sdbd =
      GetGDALDriverManager()->GetDriverByName(gis::datasource::kSdbdDriverName);
  expect(sdbd != nullptr, "GetDriverByName(SDBD)");
  if (!sdbd) {
    return;
  }

  GDALDataset* ds = static_cast<GDALDataset*>(
      GDALOpenEx("SDBD:MEM:sdbd_point", GDAL_OF_VECTOR | GDAL_OF_UPDATE,
                 nullptr, nullptr, nullptr));
  if (!ds) {
    ds = sdbd->Create("SDBD:MEM:sdbd_point", 0, 0, 0, GDT_Unknown, nullptr);
  }
  expect(ds != nullptr, "GDALOpenEx/Create SDBD:MEM");
  if (!ds) {
    std::fprintf(stderr, "SKIP: SDBD Memory-backed open failed\n");
    return;
  }
  expect(gis::datasource::as_sdbd_dataset(ds) != nullptr,
         "SDBD:MEM is SdbdDataset");

  OGRLayer* lyr = ds->CreateLayer("pts", nullptr, wkbPoint, nullptr);
  expect(lyr != nullptr, "SDBD CreateLayer pts");
  expect(gis::datasource::as_sdbd_layer(lyr) != nullptr,
         "CreateLayer returns SdbdLayer");
  if (lyr) {
    OGRFieldDefn name("name", OFTString);
    lyr->CreateField(&name);
    OGRFeature feat(lyr->GetLayerDefn());
    OGRPoint pt(10.0, 20.0);
    feat.SetGeometry(&pt);
    feat.SetField("name", "a");
    expect(lyr->CreateFeature(&feat) == OGRERR_NONE,
           "SDBD CreateFeature point");
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
    auto* sdbd_ds = gis::datasource::as_sdbd_dataset(ds);
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

  expect(std::strcmp(gis::datasource::gdal_driver_name(PROVIDER_GPKG),
                     "GPKG") == 0,
         "GPKG driver name");
  expect(std::strcmp(gis::datasource::gdal_driver_name(PROVIDER_POSTGRES),
                     "PostgreSQL") == 0,
         "PG driver name");
  expect(std::strcmp(gis::datasource::gdal_driver_name(PROVIDER_SPATIALITE),
                     "SQLite") == 0,
         "SQLite driver name");
  expect(gis::datasource::gdal_driver_name(PROVIDER_ACCESS) == nullptr,
         "ACCESS has no driver");

  ConnectionSpec gpkg;
  gpkg.provider_id = PROVIDER_GPKG;
  gpkg.service = "C:\\tmp\\ds";
  gpkg.db_name = "sample1.gpkg";
  std::string gpkg_path =
      gis::datasource::make_gdal_open_target(gpkg);
  expect(gpkg_path.find("sample1.gpkg") != std::string::npos, "GPKG path");
  expect(gpkg_path.find("PG:") == std::string::npos, "GPKG is not PG:");

  ConnectionSpec pg;
  pg.provider_id = PROVIDER_POSTGRES;
  pg.service = "127.0.0.1:5432";
  pg.db_name = "gis";
  pg.uid = "u";
  pg.pwd = "secret";
  std::string pg_target =
      gis::datasource::make_gdal_open_target(pg);
  expect(pg_target.find("PG:") == 0, "PG prefix");
  expect(pg_target.find("host=127.0.0.1") != std::string::npos, "PG host");
  expect(pg_target.find("port=5432") != std::string::npos, "PG port");
  expect(pg_target.find("dbname=gis") != std::string::npos, "PG dbname");
  expect(pg_target.find("user=u") != std::string::npos, "PG user");
  expect(pg_target.find("password=secret") != std::string::npos, "PG password");

  ConnectionSpec access;
  access.provider_id = PROVIDER_ACCESS;
  expect(gis::datasource::make_gdal_open_target(access)
             .empty(),
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
        expect(gis::datasource::encode_ogr_geometry(&smt_pt, &ogr, wkbPoint, VectorSchema::kAnno),
               "smt geom->ogr anno");
        ogr.SetField("anno", "hi");
        ogr.SetField("color", 9);
        ogr.SetField("angle", 45.0);
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
            gis::datasource::decode_ogr_geometry(&mpl);
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
            gis::datasource::decode_ogr_geometry(&mls);
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
            gis::datasource::decode_ogr_geometry(&mpgf);
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
  ConnectionSpec info;
  info.ds_type = DS_DB_ADO;
  info.provider_id = PROVIDER_GPKG;
  info.name = "t";
  info.service = tmp;
  info.db_name = "sde_gdal_roundtrip.gpkg";
  std::string path = gis::datasource::make_gdal_open_target(
      info);
  const std::string sdbd_target = gis::datasource::make_sdbd_open_target(
      info);
  expect(sdbd_target.rfind("SDBD:GPKG:", 0) == 0, "SDBD:GPKG target");
  DeleteFileA(path.c_str());

  GDALDriver* gpkg_drv = GetGDALDriverManager()->GetDriverByName("GPKG");
  const bool can_file = gpkg_drv != nullptr;
  GDALDataset* gdal_ds = gis::datasource::open_sdbd_dataset(info);
  if (!can_file) {
    expect(gdal_ds == nullptr, "GPKG Open fails without GPKG driver");
    std::fprintf(
        stderr,
        "SKIP: GPKG driver not in this GDAL; file round-trip omitted\n");
  } else {
    expect(gdal_ds != nullptr, "SDBD:GPKG open/create");
    expect(gis::datasource::as_sdbd_dataset(gdal_ds) != nullptr,
           "GPKG open returns SdbdDataset");
    if (gdal_ds) {
      expect(std::filesystem::exists(path), "created .gpkg file");
    }
  }

  gis::Envelope rect;
  rect.MinX = 0;
  rect.MinY = 0;
  rect.MaxX = 10;
  rect.MaxY = 10;
  auto* sdbd_ds = gis::datasource::as_sdbd_dataset(gdal_ds);
  if (!sdbd_ds) {
    // File round-trip requires a real GPKG driver.
  } else {
    OGRLayer* lyr = sdbd_ds->create_sdbd_layer("dots", wkbPoint);
    expect(lyr != nullptr, "create dots");
    if (lyr) {
      OGRFeature feat(lyr->GetLayerDefn());
      OGRPoint smt_pt(3.0, 4.0);
      expect(gis::datasource::encode_ogr_geometry(&smt_pt, &feat, wkbPoint),
             "encode point");
      expect(lyr->CreateFeature(&feat) == OGRERR_NONE, "append point");
      GDALClose(gdal_ds);
      gdal_ds = gis::datasource::open_sdbd_dataset(info);
      sdbd_ds = gis::datasource::as_sdbd_dataset(gdal_ds);
      OGRLayer* lyr2 = sdbd_ds ? sdbd_ds->GetLayerByName("dots") : nullptr;
      expect(lyr2 != nullptr, "reopen dots");
      if (lyr2) {
        expect(lyr2->GetFeatureCount() >= 1, "count");
        lyr2->ResetReading();
        OGRFeature* got = lyr2->GetNextFeature();
        expect(got != nullptr, "get 0");
        OGRGeometry* g =
            got ? gis::datasource::decode_ogr_geometry(got) : nullptr;
        const OGRPoint* p = dynamic_cast<const OGRPoint*>(g);
        expect(p && p->getX() == 3.0 && p->getY() == 4.0, "xy persist");
        delete g;
        OGRFeature::DestroyFeature(got);
      }
    }

    OGRLayer* lines =
        sdbd_ds ? sdbd_ds->create_sdbd_layer("lines", wkbLineString) : nullptr;
    expect(lines != nullptr, "create lines");
    if (lines) {
      auto* line = new OGRLineString();
      line->setNumPoints(2);
      line->setPoint(0, 0.0, 0.0);
      line->setPoint(1, 1.0, 1.0);
      OGRFeature feat(lines->GetLayerDefn());
      expect(gis::datasource::encode_ogr_geometry(line, &feat, wkbLineString),
             "encode line");
      expect(lines->CreateFeature(&feat) == OGRERR_NONE, "append line");
      delete line;
      GDALClose(gdal_ds);
      gdal_ds = gis::datasource::open_sdbd_dataset(info);
      sdbd_ds = gis::datasource::as_sdbd_dataset(gdal_ds);
      OGRLayer* back = sdbd_ds ? sdbd_ds->GetLayerByName("lines") : nullptr;
      expect(back != nullptr, "reopen lines");
      if (back) {
        back->ResetReading();
        OGRFeature* got = back->GetNextFeature();
        OGRGeometry* g =
            got ? gis::datasource::decode_ogr_geometry(got)
                : nullptr;
        const OGRLineString* ls = dynamic_cast<const OGRLineString*>(g);
        expect(ls && ls->getNumPoints() == 2, "line points");
        delete g;
        OGRFeature::DestroyFeature(got);
      }
    }

    OGRLayer* polys =
        sdbd_ds ? sdbd_ds->create_sdbd_layer("polys", wkbPolygon) : nullptr;
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
      expect(gis::datasource::encode_ogr_geometry(poly, &feat, wkbPolygon),
             "encode poly");
      expect(polys->CreateFeature(&feat) == OGRERR_NONE, "append poly");
      delete poly;
      GDALClose(gdal_ds);
      gdal_ds = gis::datasource::open_sdbd_dataset(info);
      sdbd_ds = gis::datasource::as_sdbd_dataset(gdal_ds);
      OGRLayer* back = sdbd_ds ? sdbd_ds->GetLayerByName("polys") : nullptr;
      expect(back != nullptr, "reopen polys");
      if (back) {
        back->ResetReading();
        OGRFeature* got = back->GetNextFeature();
        OGRGeometry* g =
            got ? gis::datasource::decode_ogr_geometry(got)
                : nullptr;
        const OGRPolygon* pg = dynamic_cast<const OGRPolygon*>(g);
        expect(pg && pg->getExteriorRing() != nullptr, "poly ring");
        delete g;
        OGRFeature::DestroyFeature(got);
      }
    }

    OGRLayer* annos =
        sdbd_ds ? sdbd_ds->create_sdbd_layer("annos", VectorSchema::kAnno) : nullptr;
    expect(annos != nullptr, "create annos");
    if (annos) {
      OGRPoint anno_pt(2.0, 3.0);
      OGRFeature feat(annos->GetLayerDefn());
      expect(gis::datasource::encode_ogr_geometry(&anno_pt, &feat, wkbPoint, VectorSchema::kAnno),
             "encode anno");
      feat.SetField("anno", "n");
      feat.SetField("color", 3);
      feat.SetField("angle", 12.0);
      expect(annos->CreateFeature(&feat) == OGRERR_NONE, "append anno");
      GDALClose(gdal_ds);
      gdal_ds = gis::datasource::open_sdbd_dataset(info);
      sdbd_ds = gis::datasource::as_sdbd_dataset(gdal_ds);
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
          expect(ci >= 0 && got->GetFieldAsInteger(ci) == 3,
                 "anno color persist");
          const int gi = got->GetFieldIndex("angle");
          expect(gi >= 0 && got->GetFieldAsDouble(gi) == 12.0,
                 "anno angle persist");
        }
        OGRFeature::DestroyFeature(got);
      }
    }

    OGRLayer* tins =
        sdbd_ds ? sdbd_ds->create_sdbd_layer("tins", VectorSchema::kTin) : nullptr;
    expect(tins != nullptr, "create tins");
    if (tins) {
      OGRTriangulatedSurface tin;
      OGRPoint a(0, 0, 0);
      OGRPoint b(1, 0, 0);
      OGRPoint c(0, 1, 0);
      expect(geo::add_patch(&tin, a, b, c), "add tin patch");
      OGRFeature feat(tins->GetLayerDefn());
      expect(gis::datasource::encode_ogr_geometry(&tin, &feat, wkbMultiPolygon, VectorSchema::kTin),
             "encode tin");
      expect(tins->CreateFeature(&feat) == OGRERR_NONE, "append tin");
      GDALClose(gdal_ds);
      gdal_ds = gis::datasource::open_sdbd_dataset(info);
      sdbd_ds = gis::datasource::as_sdbd_dataset(gdal_ds);
      OGRLayer* back = sdbd_ds ? sdbd_ds->GetLayerByName("tins") : nullptr;
      expect(back != nullptr, "reopen tins");
      if (back) {
        back->ResetReading();
        OGRFeature* got = back->GetNextFeature();
        OGRGeometry* geom = got ? got->GetGeometryRef() : nullptr;
        expect(geom != nullptr, "tin persist");
        OGRFeature::DestroyFeature(got);
      }
    }

    OGRLayer* grids =
        sdbd_ds ? sdbd_ds->create_sdbd_layer("grids", VectorSchema::kGrid) : nullptr;
    expect(grids != nullptr, "create grids");
    if (grids) {
      OGRMultiPoint mp;
      OGRPoint a(0, 0);
      OGRPoint b(1, 0);
      OGRPoint c(0, 1);
      OGRPoint d(1, 1);
      mp.addGeometry(&a);
      mp.addGeometry(&b);
      mp.addGeometry(&c);
      mp.addGeometry(&d);
      OGRFeature feat(grids->GetLayerDefn());
      expect(gis::datasource::encode_ogr_geometry(&mp, &feat, wkbMultiPoint, VectorSchema::kGrid),
             "encode grid");
      feat.SetField("grid_row", 2);
      feat.SetField("grid_col", 2);
      expect(grids->CreateFeature(&feat) == OGRERR_NONE, "append grid");
      GDALClose(gdal_ds);
      gdal_ds = gis::datasource::open_sdbd_dataset(info);
      sdbd_ds = gis::datasource::as_sdbd_dataset(gdal_ds);
      OGRLayer* back = sdbd_ds ? sdbd_ds->GetLayerByName("grids") : nullptr;
      expect(back != nullptr, "reopen grids");
      if (back) {
        back->ResetReading();
        OGRFeature* got = back->GetNextFeature();
        expect(got && gis::datasource::infer_vector_schema(got, VectorSchema::kGrid) == VectorSchema::kGrid,
               "grid type");
        OGRFeature::DestroyFeature(got);
      }
    }

    auto* ras = new gis::datasource::OgrRasterLayer(nullptr);
    expect(ras != nullptr, "raster layer object");
    if (ras) {
      expect(ras->Create(), "raster MEM Create");
      const char payload[] = "ras-bytes";
      const long cr = ras->CreaterRaster(
          payload, static_cast<long>(sizeof(payload)), rect, 7);
      expect(cr == gis::datasource::k_raster_ok, "raster CreaterRaster");
      char* got = nullptr;
      long got_size = 0;
      long got_code = -1;
      gis::Envelope got_rect;
      expect(ras->GetRasterNoClone(got, got_size, got_rect, got_code) ==
                 gis::datasource::k_raster_ok,
             "GetRasterNoClone");
      expect(got && got_size == static_cast<long>(sizeof(payload)) &&
                 std::memcmp(got, payload, sizeof(payload)) == 0,
             "raster blob round-trip");
      expect(got_code == 7, "raster image code");
      expect(got_rect.MaxX == rect.MaxX && got_rect.MaxY == rect.MaxY,
             "raster rect");
      expect(ras->dataset() != nullptr &&
                 ras->dataset()->GetRasterCount() > 0,
             "raster hangs GDALDataset bands");
      delete ras;
    }

    if (gdal_ds) {
      GDALClose(gdal_ds);
      gdal_ds = nullptr;
    }
  }

  ConnectionSpec ainfo;
  ainfo.provider_id = PROVIDER_ACCESS;
  expect(gis::datasource::make_sdbd_open_target(ainfo)
             .empty(),
         "ACCESS has no SDBD target");
  expect(gis::datasource::open_sdbd_dataset(ainfo) ==
             nullptr,
         "ACCESS Open false");

  {
    gis::Envelope rrect;
    rrect.MinX = 0;
    rrect.MinY = 0;
    rrect.MaxX = 1;
    rrect.MaxY = 1;
    GDALDriver* mem_ras = GetGDALDriverManager()->GetDriverByName("MEM");
    if (!mem_ras) {
      std::fprintf(stderr, "SKIP: MEM raster driver missing\n");
    } else {
      auto* ras = new gis::datasource::OgrRasterLayer(nullptr);
      expect(ras->Create(), "standalone MEM raster Create");
      expect(ras->CreaterRaster(nullptr, 0, rrect, 0) == gis::datasource::k_raster_ok,
             "empty CreaterRaster ok");
      delete ras;
    }
  }

  {
    gis::datasource::register_gdal_driver();
    gis::datasource::DataSession session;
    gis::MapLayer scratch = session.create_mem_vector_layer("scratch");
    expect(scratch.ogr() != nullptr, "CreateMemVecLayer Memory");
    if (scratch.ogr()) {
      OGRFeature feat(scratch.ogr()->GetLayerDefn());
      OGRPoint pt(1.0, 2.0);
      feat.SetGeometry(&pt);
      expect(scratch.ogr()->CreateFeature(&feat) == OGRERR_NONE,
             "scratch CreateFeature");
      expect(scratch.ogr()->GetFeatureCount() >= 1, "scratch count");
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
        GDALDataset* created =
            gtiff->Create(tif.string().c_str(), 2, 2, 1, GDT_Byte, nullptr);
        expect(created != nullptr, "GTiff Create for Open test");
        if (created) {
          GDALRasterBand* band = created->GetRasterBand(1);
          unsigned char px[4] = {9, 8, 7, 6};
          band->RasterIO(GF_Write, 0, 0, 2, 2, px, 2, 2, GDT_Byte, 0, 0);
          GDALClose(created);
          auto* file_ras = new gis::datasource::OgrRasterLayer(nullptr);
          expect(file_ras->Open(tif.string().c_str()),
                 "OgrRasterLayer Open tif");
          char* blob = nullptr;
          long blob_n = 0;
          long blob_code = -1;
          gis::Envelope blob_r;
          expect(file_ras->GetRasterNoClone(blob, blob_n, blob_r, blob_code) ==
                         gis::datasource::k_raster_ok &&
                     blob && blob_n > 0,
                 "Open file GetRasterNoClone has bytes");
          delete file_ras;
        }
        fs::remove_all(dir, ec);
      }
    }
  }

  const char* pg_dsn = base::switch_cstr("pg-dsn");
  if (pg_dsn && pg_dsn[0]) {
    ConnectionSpec pgi;
    pgi.ds_type = DS_DB_ADO;
    pgi.provider_id = PROVIDER_POSTGRES;
    pgi.name = "pg";
    std::string dsn = pg_dsn;
    if (dsn.rfind("PG:", 0) == 0) {
      dsn = dsn.substr(3);
    }
    auto take = [&](const char* key) {
      const std::string token = std::string(key) + "=";
      const auto pos = dsn.find(token);
      if (pos == std::string::npos) {
        return std::string();
      }
      auto end = dsn.find(' ', pos);
      if (end == std::string::npos) {
        end = dsn.size();
      }
      return dsn.substr(pos + token.size(), end - pos - token.size());
    };
    std::string host = take("host");
    if (host.empty()) {
      host = "127.0.0.1";
    }
    std::string port = take("port");
    if (port.empty()) {
      port = "5432";
    }
    pgi.db_name = take("dbname");
    pgi.uid = take("user");
    pgi.pwd = take("password");
    pgi.service = host + ":" + port;
    GDALDataset* pgds = gis::datasource::open_sdbd_dataset(
        pgi);
    expect(pgds != nullptr, "SMT_PG_DSN Open");
    if (pgds) {
      expect(gis::datasource::as_sdbd_dataset(pgds) != nullptr,
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
