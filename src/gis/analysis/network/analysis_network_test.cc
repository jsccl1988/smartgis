// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "gis/analysis/network/apsp.h"
#include "gis/analysis/network/cost_path.h"
#include "gis/analysis/network/line_network.h"
#include "gis/analysis/network/max_flow.h"
#include "gis/analysis/network/service_area.h"
#include "gis/analysis/network/shortest_path.h"
#include "ogr_api.h"
#include "ogr_geometry.h"
#include "ogrsf_frmts.h"

#include "gtest/gtest.h"

namespace gis {
namespace detail {
namespace {

std::string temp_path(const char* name) {
  return (std::filesystem::temp_directory_path() / name).generic_string();
}

bool write_lines_with_fields(const char* name,
                             const double* segs,
                             int seg_count,
                             const char* oneway_field,
                             const int* oneway_values,
                             const char* capacity_field,
                             const double* capacity_values,
                             std::string* out_path) {
  const std::string path = temp_path(name);
  GDALAllRegister();
  GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GeoJSON");
  if (!driver) {
    return false;
  }
  VSIUnlink(path.c_str());
  GDALDatasetUniquePtr ds(
      driver->Create(path.c_str(), 0, 0, 0, GDT_Unknown, nullptr));
  if (!ds) {
    return false;
  }
  OGRLayer* layer = ds->CreateLayer("roads", nullptr, wkbLineString, nullptr);
  if (!layer) {
    return false;
  }
  if (oneway_field && *oneway_field) {
    OGRFieldDefn f(oneway_field, OFTInteger);
    layer->CreateField(&f);
  }
  if (capacity_field && *capacity_field) {
    OGRFieldDefn f(capacity_field, OFTReal);
    layer->CreateField(&f);
  }
  OGRFeatureDefn* defn = layer->GetLayerDefn();
  for (int i = 0; i < seg_count; ++i) {
    const double* s = segs + i * 4;
    OGRFeatureUniquePtr feat(OGRFeature::CreateFeature(defn));
    OGRLineString line;
    line.addPoint(s[0], s[1]);
    line.addPoint(s[2], s[3]);
    feat->SetGeometry(&line);
    if (oneway_field && oneway_values) {
      feat->SetField(oneway_field, oneway_values[i]);
    }
    if (capacity_field && capacity_values) {
      feat->SetField(capacity_field, capacity_values[i]);
    }
    if (layer->CreateFeature(feat.get()) != OGRERR_NONE) {
      return false;
    }
  }
  ds.reset();
  *out_path = path;
  return true;
}

bool write_lines(const char* name, const double* segs, int seg_count,
                 std::string* out_path) {
  return write_lines_with_fields(name, segs, seg_count, nullptr, nullptr,
                                 nullptr, nullptr, out_path);
}

bool path_visits(const PathResult& path, double x, double y) {
  constexpr double kEps = 1e-9;
  for (size_t i = 0; i + 1 < path.xy.size(); i += 2) {
    if (std::abs(path.xy[i] - x) <= kEps &&
        std::abs(path.xy[i + 1] - y) <= kEps) {
      return true;
    }
  }
  return false;
}

TEST(NetworkLineGraph, LShapeDijkstra) {
  const double segs[] = {0, 0, 1, 0, 1, 0, 1, 1};
  std::string path;
  ASSERT_TRUE(write_lines("net_l.geojson", segs, 2, &path));
  const CostPathResult r = run_cost_path(path, 0, 0, 1, 1, "");
  ASSERT_TRUE(r.ok) << r.error;
  EXPECT_GT(r.total_cost, 0.0);
  EXPECT_TRUE(path_visits(r, 1, 0));
  std::filesystem::remove(path);
}

TEST(NetworkLineGraph, RightmostTieBreak) {
  const double grid[] = {0, 0, 0, 0.999, 0, 0.999, 2, 1, 0, 0, 2, 0, 2, 0, 2, 1};
  std::string path;
  ASSERT_TRUE(write_lines("net_right.geojson", grid, 4, &path));
  const CostPathResult r = run_cost_path(path, 0, 0, 2, 1, "");
  ASSERT_TRUE(r.ok) << r.error;
  EXPECT_TRUE(path_visits(r, 2, 0));
  EXPECT_FALSE(path_visits(r, 0, 1));
  std::filesystem::remove(path);
}

TEST(NetworkLineGraph, OnewayBlocksReverse) {
  const double segs[] = {0, 0, 1, 0};
  const int oneway[] = {1};
  std::string path;
  ASSERT_TRUE(write_lines_with_fields("net_oneway.geojson", segs, 1, "oneway",
                                      oneway, nullptr, nullptr, &path));
  NetworkBuildOptions opt;
  opt.oneway_field = "oneway";
  const CostPathResult forward =
      run_cost_path_ex(path, 0, 0, 1, 0, opt, PathAlgorithm::kDijkstra);
  ASSERT_TRUE(forward.ok) << forward.error;
  const CostPathResult reverse =
      run_cost_path_ex(path, 1, 0, 0, 0, opt, PathAlgorithm::kDijkstra);
  EXPECT_FALSE(reverse.ok);
  std::filesystem::remove(path);
}

TEST(NetworkLineGraph, TurnBanBlocksCorner) {
  const double segs[] = {0, 0, 1, 0, 1, 0, 1, 1};
  std::string net;
  ASSERT_TRUE(write_lines("net_turn.geojson", segs, 2, &net));
  const double turn[] = {0, 0, 1, 0, 1, 0, 1, 1};
  // Two-point segments can't encode turn; write a 3-point line manually.
  const std::string turns = temp_path("net_turns.geojson");
  GDALAllRegister();
  GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GeoJSON");
  ASSERT_NE(driver, nullptr);
  VSIUnlink(turns.c_str());
  GDALDatasetUniquePtr ds(
      driver->Create(turns.c_str(), 0, 0, 0, GDT_Unknown, nullptr));
  ASSERT_TRUE(ds);
  OGRLayer* layer = ds->CreateLayer("turns", nullptr, wkbLineString, nullptr);
  ASSERT_NE(layer, nullptr);
  OGRFeatureUniquePtr feat(OGRFeature::CreateFeature(layer->GetLayerDefn()));
  OGRLineString line;
  line.addPoint(0, 0);
  line.addPoint(1, 0);
  line.addPoint(1, 1);
  feat->SetGeometry(&line);
  ASSERT_EQ(layer->CreateFeature(feat.get()), OGRERR_NONE);
  ds.reset();

  NetworkBuildOptions opt;
  opt.turns_path = turns;
  const CostPathResult r =
      run_cost_path_ex(net, 0, 0, 1, 1, opt, PathAlgorithm::kDijkstra);
  EXPECT_FALSE(r.ok);
  std::filesystem::remove(net);
  std::filesystem::remove(turns);
  (void)turn;
}

TEST(NetworkLineGraph, BarrierBlocksNode) {
  const double segs[] = {0, 0, 1, 0, 1, 0, 2, 0};
  std::string net;
  ASSERT_TRUE(write_lines("net_bar.geojson", segs, 2, &net));
  const std::string barriers = temp_path("net_barrier_pt.geojson");
  GDALAllRegister();
  GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GeoJSON");
  ASSERT_NE(driver, nullptr);
  VSIUnlink(barriers.c_str());
  GDALDatasetUniquePtr ds(
      driver->Create(barriers.c_str(), 0, 0, 0, GDT_Unknown, nullptr));
  ASSERT_TRUE(ds);
  OGRLayer* layer = ds->CreateLayer("b", nullptr, wkbPoint, nullptr);
  ASSERT_NE(layer, nullptr);
  OGRFeatureUniquePtr feat(OGRFeature::CreateFeature(layer->GetLayerDefn()));
  OGRPoint pt(1, 0);
  feat->SetGeometry(&pt);
  ASSERT_EQ(layer->CreateFeature(feat.get()), OGRERR_NONE);
  ds.reset();

  NetworkBuildOptions opt;
  opt.barriers_path = barriers;
  const CostPathResult r =
      run_cost_path_ex(net, 0, 0, 2, 0, opt, PathAlgorithm::kDijkstra);
  EXPECT_FALSE(r.ok);
  std::filesystem::remove(net);
  std::filesystem::remove(barriers);
}

TEST(NetworkLineGraph, BidirectionalMatchesDijkstra) {
  const double segs[] = {0, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1};
  std::string path;
  ASSERT_TRUE(write_lines("net_bi.geojson", segs, 4, &path));
  NetworkBuildOptions opt;
  const CostPathResult d =
      run_cost_path_ex(path, 0, 0, 1, 1, opt, PathAlgorithm::kDijkstra);
  const CostPathResult b =
      run_cost_path_ex(path, 0, 0, 1, 1, opt, PathAlgorithm::kBidirectional);
  ASSERT_TRUE(d.ok) << d.error;
  ASSERT_TRUE(b.ok) << b.error;
  EXPECT_NEAR(d.total_cost, b.total_cost, 1e-9);
  std::filesystem::remove(path);
}

TEST(NetworkLineGraph, ServiceAreaCoversSpokes) {
  const double segs[] = {0, 0, 1, 0, 0, 0, 0, 1};
  std::string path;
  ASSERT_TRUE(write_lines("net_sa.geojson", segs, 2, &path));
  NetworkBuildOptions opt;
  const LineNetwork net = build_line_network(path, opt);
  ASSERT_TRUE(net.ok()) << net.error;
  const int src = net.nearest_node(0, 0);
  const ServiceAreaResult area = run_service_area(net, src, 1.5);
  ASSERT_TRUE(area.ok) << area.error;
  EXPECT_GE(area.edge_count, 2);
  std::filesystem::remove(path);
}

TEST(NetworkLineGraph, ApspThreeSources) {
  const double segs[] = {0, 0, 1, 0, 1, 0, 1, 1};
  std::string path;
  ASSERT_TRUE(write_lines("net_apsp.geojson", segs, 2, &path));
  NetworkBuildOptions opt;
  const LineNetwork net = build_line_network(path, opt);
  ASSERT_TRUE(net.ok()) << net.error;
  const std::vector<int> sources = {net.nearest_node(0, 0),
                                     net.nearest_node(1, 0),
                                     net.nearest_node(1, 1)};
  const ApspResult r = run_apsp(net, sources);
  ASSERT_TRUE(r.ok) << r.error;
  EXPECT_EQ(r.pairs.size(), 9u);
  bool found = false;
  for (const ApspPair& p : r.pairs) {
    if (p.from == sources[0] && p.to == sources[2] && p.reachable) {
      found = true;
      EXPECT_NEAR(p.cost, 2.0, 1e-6);
    }
  }
  EXPECT_TRUE(found);
  std::filesystem::remove(path);
}

TEST(NetworkLineGraph, MaxFlowCapacity) {
  const double segs[] = {0, 0, 1, 0, 0, 0, 1, 0};
  const double caps[] = {2.0, 3.0};
  std::string path;
  ASSERT_TRUE(write_lines_with_fields("net_flow.geojson", segs, 2, nullptr,
                                      nullptr, "cap", caps, &path));
  NetworkBuildOptions opt;
  opt.capacity_field = "cap";
  opt.oneway_field = "";
  // Force forward-only by building with oneway all 1 via two identical segs —
  // bidirectional would double capacity. Set oneway on both.
  std::filesystem::remove(path);
  const int oneway[] = {1, 1};
  ASSERT_TRUE(write_lines_with_fields("net_flow2.geojson", segs, 2, "oneway",
                                      oneway, "cap", caps, &path));
  opt.oneway_field = "oneway";
  const LineNetwork net = build_line_network(path, opt);
  ASSERT_TRUE(net.ok()) << net.error;
  const MaxFlowResult r =
      run_max_flow(net, net.nearest_node(0, 0), net.nearest_node(1, 0));
  ASSERT_TRUE(r.ok) << r.error;
  EXPECT_NEAR(r.flow, 5.0, 1e-6);
  std::filesystem::remove(path);
}

}  // namespace
}  // namespace detail
}  // namespace gis
