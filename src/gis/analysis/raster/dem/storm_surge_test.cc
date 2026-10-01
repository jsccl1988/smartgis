// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/raster/dem/storm_surge.h"
#include "gis/analysis/raster/dem/storm_surge_stats.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "ogrsf_frmts.h"

#include "gis/analysis/ops/ops_runner.h"

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

std::string temp_file(const char* name) {
  return (std::filesystem::temp_directory_path() / name).generic_string();
}

// Coastal toy DEM: left column is ocean (elev=0), land rises to the right.
// An embayment at row 3–4, cols 1–2 sits at elev=1 so surge=2 floods it from ocean.
std::string write_toy_dem() {
  const std::string path = temp_file("storm_surge_test_dem.tif");
  GDALAllRegister();
  GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
  expect(driver != nullptr, "gtiff driver");
  if (!driver) {
    return {};
  }
  VSIUnlink(path.c_str());
  constexpr int kW = 8;
  constexpr int kH = 8;
  GDALDatasetUniquePtr ds(
      driver->Create(path.c_str(), kW, kH, 1, GDT_Float32, nullptr));
  expect(!!ds, "create dem");
  if (!ds) {
    return {};
  }
  double gt[6] = {0, 1, 0, 0, 0, -1};
  ds->SetGeoTransform(gt);
  std::vector<float> elev(static_cast<size_t>(kW * kH), 10.0f);
  for (int r = 0; r < kH; ++r) {
    elev[static_cast<size_t>(r * kW + 0)] = 0.0f;  // ocean column
    elev[static_cast<size_t>(r * kW + 1)] = 3.0f;  // shore berm
  }
  // Embayment connected to ocean at col0 through a gap at (3,1) elev=1.
  elev[static_cast<size_t>(3 * kW + 1)] = 1.0f;
  elev[static_cast<size_t>(3 * kW + 2)] = 1.0f;
  elev[static_cast<size_t>(4 * kW + 1)] = 1.0f;
  elev[static_cast<size_t>(4 * kW + 2)] = 1.5f;
  GDALRasterBand* band = ds->GetRasterBand(1);
  expect(band != nullptr, "band");
  if (!band) {
    return {};
  }
  expect(band->RasterIO(GF_Write, 0, 0, kW, kH, elev.data(), kW, kH,
                        GDT_Float32, 0, 0, nullptr) == CE_None,
         "write elev");
  return path;
}

std::string write_toy_coast() {
  const std::string path = temp_file("storm_surge_test_coast.geojson");
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  // Shoreline along ocean/land interface (map x≈0.5).
  out << R"({"type":"FeatureCollection","features":[{"type":"Feature",)"
         R"("geometry":{"type":"LineString","coordinates":)"
         R"([[0.5, -0.5],[0.5, -3.5],[0.5, -7.5]]},"properties":{}}]})";
  out.close();
  return path;
}

std::string write_toy_series() {
  const std::string path = temp_file("storm_surge_test_series.txt");
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << "1.0\n2.0\n";
  out.close();
  return path;
}

void test_kernel_coast_seeds() {
  const std::string dem = write_toy_dem();
  const std::string coast = write_toy_coast();
  expect(!dem.empty() && !coast.empty(), "fixtures");

  const std::vector<double> levels = {2.0};
  const gis::detail::StormSurgeResult result =
      gis::detail::run_storm_surge(dem, coast, {}, levels, 1);
  expect(result.ok, "storm ok");
  expect(result.width == 8 && result.height == 8, "size");
  expect(!result.mask.empty(), "mask");
  // Ocean col0 must be wet.
  expect(result.mask[0] == 1, "ocean wet");
  // Embayment cell (3,2) elev=1 should flood from ocean at level 2.
  expect(result.mask[static_cast<size_t>(3 * 8 + 2)] == 1, "bay wet");
  // High berm (0,1)=3 stays dry at level 2 (except gap row).
  expect(result.mask[static_cast<size_t>(0 * 8 + 1)] == 0, "berm dry");
  expect(!result.depth.empty() && result.depth[0] > 0.f, "ocean depth");

  const gis::detail::StormSurgeWaterMesh mesh =
      gis::detail::build_storm_surge_water_mesh(result, -1);
  expect(mesh.xyz.size() >= 9, "water mesh verts");
  expect(mesh.indices.size() >= 3, "water mesh tris");
  expect(mesh.indices.size() % 3 == 0, "tri multiples");
  // Wet ocean column should produce at least one quad (2 tris).
  expect(mesh.indices.size() / 3 >= 2, "wet quads");

  const std::string mask_out = temp_file("storm_surge_test_mask.tif");
  const std::string depth_out = temp_file("storm_surge_test_depth.tif");
  const std::string poly_out = temp_file("storm_surge_test_poly.geojson");
  expect(gis::detail::write_storm_surge_mask_geotiff(mask_out, result, ""),
         "write mask");
  expect(gis::detail::write_storm_surge_depth_geotiff(depth_out, result, ""),
         "write depth");
  expect(gis::detail::polygonize_storm_surge_mask(poly_out, result),
         "polygonize");

  std::error_code ec;
  std::filesystem::remove(dem, ec);
  std::filesystem::remove(coast, ec);
  std::filesystem::remove(mask_out, ec);
  std::filesystem::remove(depth_out, ec);
  std::filesystem::remove(poly_out, ec);
}

void test_op_series() {
  const std::string dem = write_toy_dem();
  const std::string coast = write_toy_coast();
  const std::string series = write_toy_series();
  const std::string mask_out = temp_file("storm_surge_op_mask.tif");
  const std::string depth_out = temp_file("storm_surge_op_depth.tif");

  // Escape backslashes for JSON on Windows.
  auto jpath = [](const std::string& p) {
    std::string o;
    o.reserve(p.size() + 8);
    for (char c : p) {
      if (c == '\\') {
        o += '\\';
      }
      o += c;
    }
    return o;
  };

  const std::string json =
      std::string("{\"dem\":\"") + jpath(dem) + "\",\"coast\":\"" +
      jpath(coast) + "\",\"tide\":\"" + jpath(series) + "\",\"output\":\"" +
      jpath(mask_out) + "\",\"depth_output\":\"" + jpath(depth_out) + "\"}";
  expect(gis::detail::run_storm_surge_op(json), "op runner");

  std::error_code ec;
  std::filesystem::remove(dem, ec);
  std::filesystem::remove(coast, ec);
  std::filesystem::remove(series, ec);
  std::filesystem::remove(mask_out, ec);
  std::filesystem::remove(depth_out, ec);
}

std::string write_toy_impact() {
  const std::string path = temp_file("storm_surge_test_impact.geojson");
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  // Building polygon overlapping ocean column (x≈0.2–0.8, y≈-1.2–-0.2).
  out << R"({"type":"FeatureCollection","features":[)"
         R"({"type":"Feature","properties":{"name":"b1"},"geometry":)"
         R"({"type":"Polygon","coordinates":[[[0.2,-0.2],[0.8,-0.2],)"
         R"([0.8,-1.2],[0.2,-1.2],[0.2,-0.2]]]}},)"
         R"({"type":"Feature","properties":{"name":"dry"},"geometry":)"
         R"({"type":"Polygon","coordinates":[[[6.0,-6.0],[7.0,-6.0],)"
         R"([7.0,-7.0],[6.0,-7.0],[6.0,-6.0]]]}}]})";
  out.close();
  return path;
}

void test_stats_area_depth_overlap() {
  const std::string dem = write_toy_dem();
  const std::string coast = write_toy_coast();
  expect(!dem.empty() && !coast.empty(), "stats fixtures");

  const std::vector<double> levels = {2.0};
  const gis::detail::StormSurgeResult surge =
      gis::detail::run_storm_surge(dem, coast, {}, levels, 1);
  expect(surge.ok, "stats surge ok");

  int wet = 0;
  double area = 0;
  expect(gis::detail::compute_inundation_area(
             surge.mask.data(), surge.width, surge.height, surge.geotransform,
             &wet, &area),
         "area");
  expect(wet > 0 && area > 0.0, "wet area positive");
  expect(std::abs(area - static_cast<double>(wet) *
                             gis::detail::storm_surge_cell_area(
                                 surge.geotransform)) < 1e-9,
         "area = wet * cell");

  std::vector<gis::detail::StormSurgeDepthClass> classes;
  std::vector<unsigned char> class_mask;
  expect(gis::detail::classify_storm_surge_depth(
             surge.depth.data(), surge.mask.data(), surge.width, surge.height,
             surge.geotransform, {}, &classes, &class_mask),
         "classify");
  expect(classes.size() == 4, "four depth classes");
  int class_cells = 0;
  for (const auto& c : classes) {
    class_cells += c.cell_count;
  }
  expect(class_cells == wet, "class cells == wet");
  expect(class_mask.size() == surge.mask.size(), "class mask size");

  const std::string impact = write_toy_impact();
  const gis::detail::StormSurgeStatsResult stats =
      gis::detail::compute_storm_surge_stats(
          surge.mask.data(), surge.depth.data(), surge.width, surge.height,
          surge.geotransform, {}, impact, /*buffer_distance=*/0.5,
          /*build_class_mask=*/true);
  expect(stats.ok, "stats ok");
  expect(stats.inundation_area > 0.0, "stats inundation");
  expect(stats.depth_classes.size() == 4, "stats classes");
  expect(stats.overlap.computed, "overlap computed");
  expect(stats.overlap.impact_feature_count == 2, "impact features");
  expect(stats.overlap.intersect_feature_count >= 1, "intersected building");

  const std::string mask_out = temp_file("storm_surge_stats_mask.tif");
  const std::string depth_out = temp_file("storm_surge_stats_depth.tif");
  const std::string report = temp_file("storm_surge_stats.json");
  const std::string class_out = temp_file("storm_surge_class.tif");
  const std::string buffer_out = temp_file("storm_surge_buffer.geojson");
  expect(gis::detail::write_storm_surge_mask_geotiff(mask_out, surge, ""),
         "write mask for stats op");
  expect(gis::detail::write_storm_surge_depth_geotiff(depth_out, surge, ""),
         "write depth for stats op");

  auto jpath = [](const std::string& p) {
    std::string o;
    o.reserve(p.size() + 8);
    for (char c : p) {
      if (c == '\\') {
        o += '\\';
      }
      o += c;
    }
    return o;
  };
  const std::string json =
      std::string("{\"mask\":\"") + jpath(mask_out) + "\",\"depth\":\"" +
      jpath(depth_out) + "\",\"impact\":\"" + jpath(impact) +
      "\",\"buffer_distance\":0.5,\"output\":\"" + jpath(report) +
      "\",\"class_mask_output\":\"" + jpath(class_out) +
      "\",\"buffer_output\":\"" + jpath(buffer_out) + "\"}";
  expect(gis::detail::run_storm_surge_stats_op(json), "stats op");
  expect(gis::detail::run_builtin_op("native.storm_surge_stats", json),
         "catalog stats");

  std::ifstream in(report);
  expect(!!in, "report exists");
  std::string body((std::istreambuf_iterator<char>(in)),
                   std::istreambuf_iterator<char>());
  expect(body.find("\"inundation_area\"") != std::string::npos, "report area");
  expect(body.find("\"depth_classes\"") != std::string::npos, "report classes");

  std::error_code ec;
  std::filesystem::remove(dem, ec);
  std::filesystem::remove(coast, ec);
  std::filesystem::remove(impact, ec);
  std::filesystem::remove(mask_out, ec);
  std::filesystem::remove(depth_out, ec);
  std::filesystem::remove(report, ec);
  std::filesystem::remove(class_out, ec);
  std::filesystem::remove(buffer_out, ec);
}

void test_catalog_dispatch() {
  bool found = false;
  for (const auto& op : gis::detail::builtin_op_catalog()) {
    if (op.id && std::string_view(op.id) == "native.storm_surge_stats") {
      found = true;
      break;
    }
  }
  expect(found, "catalog has native.storm_surge_stats");
}

}  // namespace

int main() {
  test_kernel_coast_seeds();
  test_op_series();
  test_stats_area_depth_overlap();
  test_catalog_dispatch();
  if (g_fails == 0) {
    std::printf("analysis_storm_surge_test OK\n");
  }
  return g_fails == 0 ? 0 : 1;
}
