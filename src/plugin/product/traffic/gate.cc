// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/traffic/gate.h"

#include <cmath>
#include <filesystem>
#include <string>

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "gis/analysis/network/cost_path.h"
#include "ogr_api.h"
#include "ogr_geometry.h"
#include "ogrsf_frmts.h"

namespace plugin {
namespace {

struct TrafficGateFix {
  std::string network;
  std::string grid;
  std::string out;
  gis::detail::CostPathResult path;
};

TrafficGateFix& fix() {
  static TrafficGateFix state;
  return state;
}

std::string temp_file(const char* name) {
  return (std::filesystem::temp_directory_path() / name).generic_string();
}

void clear_fix() {
  std::error_code ec;
  if (!fix().network.empty()) {
    std::filesystem::remove(fix().network, ec);
  }
  if (!fix().grid.empty()) {
    std::filesystem::remove(fix().grid, ec);
  }
  if (!fix().out.empty()) {
    std::filesystem::remove(fix().out, ec);
  }
  fix() = {};
}

bool write_lines(const char* name, const double* segs, int seg_count,
                 std::string* out_path) {
  const std::string path = temp_file(name);
  GDALAllRegister();
  GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GeoJSON");
  if (driver == nullptr) {
    return false;
  }
  VSIUnlink(path.c_str());
  GDALDatasetUniquePtr ds(
      driver->Create(path.c_str(), 0, 0, 0, GDT_Unknown, nullptr));
  if (!ds) {
    return false;
  }
  OGRLayer* layer = ds->CreateLayer("roads", nullptr, wkbLineString, nullptr);
  if (layer == nullptr) {
    return false;
  }
  OGRFeatureDefn* defn = layer->GetLayerDefn();
  for (int i = 0; i < seg_count; ++i) {
    const double* s = segs + i * 4;
    OGRFeatureUniquePtr feat(OGRFeature::CreateFeature(defn));
    OGRLineString line;
    line.addPoint(s[0], s[1]);
    line.addPoint(s[2], s[3]);
    feat->SetGeometry(&line);
    if (layer->CreateFeature(feat.get()) != OGRERR_NONE) {
      return false;
    }
  }
  ds.reset();
  *out_path = path;
  return true;
}

bool path_visits_xy(const gis::detail::CostPathResult& path, double x,
                    double y) {
  constexpr double kEps = 1e-9;
  for (size_t i = 0; i + 1 < path.xy.size(); i += 2) {
    if (std::abs(path.xy[i] - x) <= kEps &&
        std::abs(path.xy[i + 1] - y) <= kEps) {
      return true;
    }
  }
  return false;
}

bool traffic_contract(void*) {
  const double segs[] = {0, 0, 1, 0, 1, 0, 1, 1};
  return write_lines("traffic_gate_net.geojson", segs, 2, &fix().network);
}

bool traffic_compute(void*) {
  fix().path = gis::detail::run_cost_path(fix().network, 0.0, 0.0, 1.0, 1.0, "");
  if (!fix().path.ok || fix().path.xy.size() < 4 || !(fix().path.total_cost > 0.0)) {
    return false;
  }
  const double grid[] = {0, 0, 0, 0.999, 0, 0.999, 2, 1, 0, 0, 2, 0, 2, 0, 2, 1};
  if (!write_lines("traffic_gate_rightmost.geojson", grid, 4, &fix().grid)) {
    return false;
  }
  const gis::detail::CostPathResult rightmost =
      gis::detail::run_cost_path(fix().grid, 0.0, 0.0, 2.0, 1.0, "");
  if (!rightmost.ok || !path_visits_xy(rightmost, 2.0, 0.0) ||
      path_visits_xy(rightmost, 0.0, 1.0)) {
    return false;
  }
  const char* sample_candidates[] = {
      "testing/data/plugin/traffic_network_sample.geojson",
      "../data/plugin/traffic_network_sample.geojson",
  };
  for (const char* sample : sample_candidates) {
    if (!std::filesystem::exists(sample)) {
      continue;
    }
    const gis::detail::CostPathResult beijing = gis::detail::run_cost_path(
        sample, 116.335, 39.870, 116.452, 39.9285, "cost");
    if (!beijing.ok || !path_visits_xy(beijing, 116.452, 39.870) ||
        path_visits_xy(beijing, 116.335, 39.9285)) {
      return false;
    }
    break;
  }
  return true;
}

bool traffic_command(void*) {
  fix().out = temp_file("traffic_gate_out.geojson");
  const std::string args = "{\"network\":\"" + fix().network +
                           "\",\"output\":\"" + fix().out +
                           "\",\"start_x\":0,\"start_y\":0,\"end_x\":1,"
                           "\"end_y\":1}";
  return gis::detail::run_cost_path_op(args.c_str());
}

bool traffic_visual(void*) {
  std::error_code ec;
  if (fix().out.empty() || !std::filesystem::is_regular_file(fix().out, ec) ||
      ec || std::filesystem::file_size(fix().out, ec) == 0 || ec) {
    return false;
  }
  return gis::detail::write_path_geojson(fix().out, fix().path);
}

}  // namespace

ProductGateResult run_traffic_product_gate() {
  clear_fix();
  static const ProductGateStep kSteps[] = {
      {"contract", traffic_contract},
      {"compute", traffic_compute},
      {"command", traffic_command},
      {"visual", traffic_visual},
  };
  const ProductGateResult result =
      run_product_gate("traffic", kSteps, 4, nullptr);
  clear_fix();
  return result;
}

}  // namespace plugin
