// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/layer/layer_ops.h"
#include "gis/analysis/ops/runner/ops_runner.h"

#include <cstdio>
#include <filesystem>
#include <string>

#include "gdal_priv.h"
#include "ogr_geometry.h"
#include "ogrsf_frmts.h"

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

std::string temp_path(const char* name) {
  return (std::filesystem::temp_directory_path() / name).generic_string();
}

bool write_poly_layer(const char* path,
                      const char* layer_name,
                      const char* field_name,
                      const char* const* field_values,
                      const double (*rings)[8],
                      int ring_count,
                      int points_per_ring) {
  GDALAllRegister();
  GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GeoJSON");
  if (!driver) {
    return false;
  }
  VSIUnlink(path);
  GDALDatasetUniquePtr ds(
      driver->Create(path, 0, 0, 0, GDT_Unknown, nullptr));
  if (!ds) {
    return false;
  }
  OGRLayer* layer = ds->CreateLayer(layer_name, nullptr, wkbPolygon, nullptr);
  if (!layer) {
    return false;
  }
  if (field_name && *field_name) {
    OGRFieldDefn defn(field_name, OFTString);
    layer->CreateField(&defn);
  }
  OGRFeatureDefn* fdefn = layer->GetLayerDefn();
  for (int i = 0; i < ring_count; ++i) {
    OGRFeatureUniquePtr feat(OGRFeature::CreateFeature(fdefn));
    OGRPolygon poly;
    OGRLinearRing ring;
    for (int p = 0; p < points_per_ring; ++p) {
      ring.addPoint(rings[i][2 * p], rings[i][2 * p + 1]);
    }
    ring.closeRings();
    poly.addRing(&ring);
    feat->SetGeometry(&poly);
    if (field_values && field_values[i]) {
      feat->SetField(field_name, field_values[i]);
    }
    if (layer->CreateFeature(feat.get()) != OGRERR_NONE) {
      return false;
    }
  }
  return true;
}

int feature_count(const std::string& path) {
  GDALAllRegister();
  GDALDatasetUniquePtr ds(static_cast<GDALDataset*>(GDALOpenEx(
      path.c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY, nullptr, nullptr, nullptr)));
  if (!ds || ds->GetLayerCount() < 1) {
    return -1;
  }
  OGRLayer* layer = ds->GetLayer(0);
  int count = 0;
  layer->ResetReading();
  while (layer->GetNextFeature()) {
    ++count;
  }
  return count;
}

bool write_line_layer(const char* path, const double* xy, int point_count) {
  GDALAllRegister();
  GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GeoJSON");
  if (!driver) {
    return false;
  }
  VSIUnlink(path);
  GDALDatasetUniquePtr ds(driver->Create(path, 0, 0, 0, GDT_Unknown, nullptr));
  if (!ds) {
    return false;
  }
  OGRLayer* layer = ds->CreateLayer("lines", nullptr, wkbLineString, nullptr);
  if (!layer) {
    return false;
  }
  OGRFeatureUniquePtr feat(OGRFeature::CreateFeature(layer->GetLayerDefn()));
  OGRLineString line;
  for (int i = 0; i < point_count; ++i) {
    line.addPoint(xy[2 * i], xy[2 * i + 1]);
  }
  feat->SetGeometry(&line);
  return layer->CreateFeature(feat.get()) == OGRERR_NONE;
}

bool write_point_layer(const char* path, const double* xy, int point_count) {
  GDALAllRegister();
  GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GeoJSON");
  if (!driver) {
    return false;
  }
  VSIUnlink(path);
  GDALDatasetUniquePtr ds(driver->Create(path, 0, 0, 0, GDT_Unknown, nullptr));
  if (!ds) {
    return false;
  }
  OGRLayer* layer = ds->CreateLayer("pts", nullptr, wkbPoint, nullptr);
  if (!layer) {
    return false;
  }
  for (int i = 0; i < point_count; ++i) {
    OGRFeatureUniquePtr feat(OGRFeature::CreateFeature(layer->GetLayerDefn()));
    OGRPoint pt(xy[2 * i], xy[2 * i + 1]);
    feat->SetGeometry(&pt);
    if (layer->CreateFeature(feat.get()) != OGRERR_NONE) {
      return false;
    }
  }
  return true;
}

}  // namespace

int main() {
  const std::string dissolve_in = temp_path("sg_dissolve_in.geojson");
  const std::string dissolve_out = temp_path("sg_dissolve_out.geojson");
  const char* groups[] = {"A", "A", "B"};
  const double dissolve_rings[][8] = {
      {0, 0, 1, 0, 1, 1, 0, 1},
      {2, 0, 3, 0, 3, 1, 2, 1},
      {10, 10, 11, 10, 11, 11, 10, 11},
  };
  expect(write_poly_layer(dissolve_in.c_str(), "src", "grp", groups, dissolve_rings, 3, 4),
         "dissolve input");
  const gis::detail::LayerOpResult dissolve_r =
      gis::detail::dissolve_by_field(dissolve_in, "grp", dissolve_out);
  expect(dissolve_r.ok && dissolve_r.features_out == 2, "dissolve by field");
  expect(feature_count(dissolve_out) == 2, "dissolve feature count");

  const std::string clip_in = temp_path("sg_clip_in.geojson");
  const std::string clip_mask = temp_path("sg_clip_mask.geojson");
  const std::string clip_out = temp_path("sg_clip_out.geojson");
  const double big_ring[8] = {0, 0, 4, 0, 4, 4, 0, 4};
  const double small_ring[8] = {1, 1, 2, 1, 2, 2, 1, 2};
  expect(write_poly_layer(clip_in.c_str(), "src", nullptr, nullptr, &big_ring, 1, 4),
         "clip input");
  expect(write_poly_layer(clip_mask.c_str(), "mask", nullptr, nullptr, &small_ring, 1, 4),
         "clip mask");
  const gis::detail::LayerOpResult clip_r =
      gis::detail::clip_layer_by_polygons(clip_in, clip_mask, clip_out);
  expect(clip_r.ok && clip_r.features_out == 1, "clip layer");
  expect(feature_count(clip_out) == 1, "clip count");

  const std::string target = temp_path("sg_join_target.geojson");
  const std::string join = temp_path("sg_join_join.geojson");
  const std::string join_out = temp_path("sg_join_out.geojson");
  const double target_ring[8] = {0, 0, 2, 0, 2, 2, 0, 2};
  const double join_ring[8] = {1, 1, 3, 1, 3, 3, 1, 3};
  expect(write_poly_layer(target.c_str(), "target", "id", nullptr, &target_ring, 1, 4),
         "join target write");
  {
    GDALAllRegister();
    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GeoJSON");
    VSIUnlink(join.c_str());
    GDALDatasetUniquePtr ds(driver->Create(join.c_str(), 0, 0, 0, GDT_Unknown, nullptr));
    OGRLayer* layer = ds->CreateLayer("join", nullptr, wkbPolygon, nullptr);
    OGRFieldDefn name("name", OFTString);
    layer->CreateField(&name);
    OGRFeatureDefn* defn = layer->GetLayerDefn();
    OGRFeatureUniquePtr feat(OGRFeature::CreateFeature(defn));
    OGRPolygon poly;
    OGRLinearRing ring;
    for (int p = 0; p < 4; ++p) {
      ring.addPoint(join_ring[2 * p], join_ring[2 * p + 1]);
    }
    ring.closeRings();
    poly.addRing(&ring);
    feat->SetGeometry(&poly);
    feat->SetField("name", "overlap");
    expect(layer->CreateFeature(feat.get()) == OGRERR_NONE, "join feature");
  }
  const gis::detail::LayerOpResult join_r = gis::detail::spatial_join_layer(
      target, join, "intersects", "name", join_out);
  expect(join_r.ok && join_r.features_out == 1, "spatial join");

  const std::string erase_in = temp_path("sg_erase_in.geojson");
  const std::string erase_mask = temp_path("sg_erase_mask.geojson");
  const std::string erase_out = temp_path("sg_erase_out.geojson");
  expect(write_poly_layer(erase_in.c_str(), "src", nullptr, nullptr, &big_ring, 1, 4),
         "erase input");
  expect(write_poly_layer(erase_mask.c_str(), "mask", nullptr, nullptr, &small_ring, 1, 4),
         "erase mask");
  const gis::detail::LayerOpResult erase_r =
      gis::detail::erase_layer(erase_in, erase_mask, erase_out);
  expect(erase_r.ok && erase_r.features_out == 1, "erase layer");

  const std::string id_in = temp_path("sg_id_in.geojson");
  const std::string id_mask = temp_path("sg_id_mask.geojson");
  const std::string id_out = temp_path("sg_id_out.geojson");
  expect(write_poly_layer(id_in.c_str(), "src", "aid", nullptr, &big_ring, 1, 4),
         "identity input");
  {
    GDALAllRegister();
    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GeoJSON");
    VSIUnlink(id_mask.c_str());
    GDALDatasetUniquePtr ds(
        driver->Create(id_mask.c_str(), 0, 0, 0, GDT_Unknown, nullptr));
    OGRLayer* layer = ds->CreateLayer("id", nullptr, wkbPolygon, nullptr);
    OGRFieldDefn name("zone", OFTString);
    layer->CreateField(&name);
    OGRFeatureUniquePtr feat(OGRFeature::CreateFeature(layer->GetLayerDefn()));
    OGRPolygon poly;
    OGRLinearRing ring;
    for (int p = 0; p < 4; ++p) {
      ring.addPoint(small_ring[2 * p], small_ring[2 * p + 1]);
    }
    ring.closeRings();
    poly.addRing(&ring);
    feat->SetGeometry(&poly);
    feat->SetField("zone", "Z1");
    expect(layer->CreateFeature(feat.get()) == OGRERR_NONE, "identity mask");
  }
  const gis::detail::LayerOpResult id_r =
      gis::detail::identity_layer(id_in, id_mask, "zone", id_out);
  expect(id_r.ok && id_r.features_out >= 2, "identity overlay");

  const std::string line_in = temp_path("sg_split_line.geojson");
  const std::string pts_in = temp_path("sg_split_pts.geojson");
  const std::string split_out = temp_path("sg_split_out.geojson");
  const double line_xy[] = {0, 0, 4, 0};
  const double pts_xy[] = {2, 0};
  expect(write_line_layer(line_in.c_str(), line_xy, 2), "split line write");
  expect(write_point_layer(pts_in.c_str(), pts_xy, 1), "split pts write");
  const gis::detail::LayerOpResult split_r =
      gis::detail::split_line_by_points(line_in, pts_in, 0.01, split_out);
  expect(split_r.ok && split_r.features_out == 2, "split line");

  const std::string cut_poly = temp_path("sg_cut_poly.geojson");
  const std::string cut_line = temp_path("sg_cut_line.geojson");
  const std::string cut_out = temp_path("sg_cut_out.geojson");
  expect(write_poly_layer(cut_poly.c_str(), "src", nullptr, nullptr, &big_ring, 1, 4),
         "cut poly write");
  const double cutter_xy[] = {2, -1, 2, 5};
  expect(write_line_layer(cut_line.c_str(), cutter_xy, 2), "cut line write");
  const gis::detail::LayerOpResult cut_r =
      gis::detail::cut_polygon_by_lines(cut_poly, cut_line, cut_out);
  expect(cut_r.ok && cut_r.features_out >= 2, "cut polygon");

  const std::string dissolve_args =
      std::string(R"({"input":")") + dissolve_in + R"(","output":")" +
      temp_path("sg_dissolve_runner.geojson") + R"(","field":"grp"})";
  expect(gis::detail::run_builtin_op("native.dissolve", dissolve_args), "runner dissolve");
  const std::string identity_args =
      std::string(R"({"input":")") + id_in + R"(","identity":")" + id_mask +
      R"(","output":")" + temp_path("sg_id_runner.geojson") +
      R"(","copy_fields":"zone"})";
  expect(gis::detail::run_builtin_op("native.identity", identity_args),
         "runner identity");

  if (g_fails != 0) {
    std::fprintf(stderr, "%d test(s) failed\n", g_fails);
    return 1;
  }
  std::fprintf(stderr, "analysis_layer_ops_test OK\n");
  return 0;
}
