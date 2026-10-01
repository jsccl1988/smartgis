// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <cstdio>
#include <filesystem>
#include <string>

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "gis/analysis/network/cost_path.h"
#include "ogr_api.h"
#include "ogr_geometry.h"
#include "ogrsf_frmts.h"

namespace {

int g_fails = 0;

void expect(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

std::string temp_file(const char* name) {
  return (std::filesystem::temp_directory_path() / name).generic_string();
}

std::string write_toy_network() {
  const std::string path = temp_file("traffic_path_test_net.geojson");
  GDALAllRegister();
  GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GeoJSON");
  expect(driver != nullptr, "geojson driver");
  if (!driver) {
    return {};
  }
  VSIUnlink(path.c_str());
  GDALDatasetUniquePtr ds(
      driver->Create(path.c_str(), 0, 0, 0, GDT_Unknown, nullptr));
  expect(!!ds, "create network ds");
  if (!ds) {
    return {};
  }
  OGRLayer* layer = ds->CreateLayer("roads", nullptr, wkbLineString, nullptr);
  expect(layer != nullptr, "create layer");
  if (!layer) {
    return {};
  }
  OGRFeatureDefn* defn = layer->GetLayerDefn();
  auto add_seg = [&](double x0, double y0, double x1, double y1) {
    OGRFeatureUniquePtr feat(OGRFeature::CreateFeature(defn));
    OGRLineString line;
    line.addPoint(x0, y0);
    line.addPoint(x1, y1);
    feat->SetGeometry(&line);
    expect(layer->CreateFeature(feat.get()) == OGRERR_NONE, "add segment");
  };
  // L shape: (0,0)-(1,0)-(1,1)
  add_seg(0, 0, 1, 0);
  add_seg(1, 0, 1, 1);
  return path;
}

}  // namespace

int main() {
  const std::string network = write_toy_network();
  expect(!network.empty(), "network path");
  const gis::detail::CostPathResult path =
      gis::detail::run_cost_path(network, 0.0, 0.0, 1.0, 1.0, "");
  expect(path.ok, "path ok");
  expect(path.xy.size() >= 4, "path has vertices");
  expect(path.total_cost > 0.0, "positive cost");
  const std::string out = temp_file("traffic_path_test_out.geojson");
  expect(gis::detail::write_path_geojson(out, path), "write geojson");
  expect(gis::detail::run_cost_path_op(
             ("{\"network\":\"" + network + "\",\"output\":\"" + out +
              "\",\"start_x\":0,\"start_y\":0,\"end_x\":1,\"end_y\":1}")
                 .c_str()),
         "op runner");
  std::error_code ec;
  std::filesystem::remove(network, ec);
  std::filesystem::remove(out, ec);
  if (g_fails == 0) {
    std::printf("traffic_path_test OK\n");
  }
  return g_fails == 0 ? 0 : 1;
}
