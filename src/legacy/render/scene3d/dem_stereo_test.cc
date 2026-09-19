// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>


#include "gdal_priv.h"
#include "gis/world/scene.h"
#include "legacy/render/scene3d/dem_height_field.h"
#include "legacy/render/scene3d/dem_to_world.h"
#include "legacy/render/scene3d/map_to_scene.h"
#include "legacy/render/scene3d/scene_to_world.h"
#include "render/math/aabb.h"

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

std::filesystem::path write_gtiff_fixture() {
  const auto path =
      std::filesystem::temp_directory_path() /
      ("smartgis_scene_dem_fixture_" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()) +
       ".tif");
  GDALAllRegister();
  GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
  if (!driver) {
    return {};
  }
  GDALDataset* dataset =
      driver->Create(path.string().c_str(), 8, 8, 1, GDT_Float32, nullptr);
  if (!dataset) {
    return {};
  }
  double gt[6] = {100.0, 1.0, 0.0, 40.0, 0.0, -1.0};
  dataset->SetGeoTransform(gt);
  std::vector<float> heights(64);
  for (int row = 0; row < 8; ++row) {
    for (int col = 0; col < 8; ++col) {
      heights[static_cast<size_t>(row * 8 + col)] =
          static_cast<float>(col * 100 + row);
    }
  }
  const CPLErr err = dataset->GetRasterBand(1)->RasterIO(
      GF_Write, 0, 0, 8, 8, heights.data(), 8, 8, GDT_Float32, 0, 0);
  GDALClose(dataset);
  if (err != CE_None) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
    return {};
  }
  return path;
}

}  // namespace

int main() {
  {
    render::DemHeightField dem;
    dem.fill_synthetic_china();
    expect(!dem.empty(), "synthetic china dem");
    expect(dem.cols() >= 8 && dem.rows() >= 8, "synthetic grid size");
    const float tibet = dem.sample_meters(88.0, 32.0);
    const float jiangsu = dem.sample_meters(119.0, 32.5);
    expect(tibet > jiangsu + 800.f, "tibet higher than jiangsu");
    expect(dem.sample(88.0, 32.0) > dem.sample(119.0, 32.5),
           "display height follows meters");
    float nx = 0;
    float ny = 0;
    float nz = 0;
    dem.sample_normal(88.0, 32.0, &nx, &ny, &nz);
    expect(ny > 0.2f, "terrain normal points up");
    std::vector<float> xyz;
    std::vector<unsigned> idx;
    std::vector<float> rgb;
    std::vector<float> nrm;
    expect(dem.build_mesh(96, &xyz, &idx, &rgb, &nrm), "build_mesh");
    expect(xyz.size() >= 12 && (xyz.size() % 3) == 0, "mesh vertices");
    expect(idx.size() >= 6 && (idx.size() % 3) == 0, "mesh triangles");
    expect(rgb.size() == xyz.size(), "hypsometric rgb");
    expect(nrm.size() == xyz.size(), "mesh normals");
    float x_min = 1e9f;
    float x_max = -1e9f;
    float z_min = 1e9f;
    float z_max = -1e9f;
    for (size_t i = 0; i + 2 < xyz.size(); i += 3) {
      x_min = (std::min)(x_min, xyz[i]);
      x_max = (std::max)(x_max, xyz[i]);
      z_min = (std::min)(z_min, xyz[i + 2]);
      z_max = (std::max)(z_max, xyz[i + 2]);
    }
    expect(z_max > z_min + 20.f, "north-south leftover Z span");
    expect(z_max > 45.f, "higher lat maps to leftover +Z");
    expect(z_min < 25.f, "lower lat maps to leftover -Z side");
    expect(x_min < 80.f && x_max > 125.f, "mesh AABB covers China-like lon");
    const size_t full_tris = idx.size();

    render::LonLatRing mainland;
    mainland.x = {100.0, 120.0, 120.0, 100.0};
    mainland.y = {30.0, 30.0, 40.0, 40.0};
    expect(render::any_ring_contains(110.0, 35.0, {mainland}),
           "mainland ring covers 110E 35N");
    {
      render::DemHeightField kept;
      kept.fill_synthetic_china();
      kept.mask_outside_rings({mainland});
      expect(kept.sample_meters(110.0, 35.0) > 1.f, "mask keeps mainland");
      std::vector<float> mx;
      std::vector<unsigned> mi;
      expect(kept.build_mesh(96, &mx, &mi, nullptr, nullptr),
             "mainland mesh");
      bool near_luoyang = false;
      for (size_t i = 0; i + 2 < mx.size(); i += 3) {
        if (std::fabs(mx[i] - 110.f) < 4.f &&
            std::fabs(mx[i + 2] - 35.f) < 4.f) {
          near_luoyang = true;
          break;
        }
      }
      expect(near_luoyang, "mesh keeps mainland vertex near 110,35");
    }

    {
      render::Aabb china;
      china.vcMin.set(73.f, 0.f, 17.5f);
      china.vcMax.set(135.f, 8.f, 54.f);
      china.vcCenter = (china.vcMax + china.vcMin) / 2.f;
      render::Vector3 eye;
      render::Vector3 target;
      float span = 0.f;
      render::leftover_frame_pose(china, &eye, &target, &span);
      expect(target.x > 90.f && target.x < 120.f, "look-at China lon");
      expect(eye.x > 80.f && eye.x < 130.f, "framed eye lon is China");
      expect(!(eye.x < 40.f && eye.z > 200.f), "not leftover origin pose");
      expect(eye.z < target.z, "eye south of target looks north");
      expect(span > 40.f, "China AABB span");
    }

    render::LonLatRing west;
    west.x = {70.0, 100.0, 100.0, 70.0};
    west.y = {20.0, 20.0, 50.0, 50.0};
    dem.mask_outside_rings({west});
    xyz.clear();
    idx.clear();
    expect(dem.build_mesh(96, &xyz, &idx, nullptr, nullptr), "masked mesh");
    expect(idx.size() < full_tris && idx.size() >= 6, "mask drops ocean quads");
    float lon_min = 1e9f;
    float lon_max = -1e9f;
    for (size_t i = 0; i + 2 < xyz.size(); i += 3) {
      lon_min = (std::min)(lon_min, xyz[i]);
      lon_max = (std::max)(lon_max, xyz[i]);
    }
    // Land-majority filter can shrink the active span further than the ring.
    expect(lon_max - lon_min < 55.f, "masked span is not full china bbox");
  }

  const std::filesystem::path tif = write_gtiff_fixture();
  expect(!tif.empty(), "write dem fixture");
  {
    render::DemHeightField dem;
    expect(dem.load_gdal_raster(tif.string().c_str()), "load_gdal_raster");
    expect(
        dem.sample_meters(107.5, 36.5) > dem.sample_meters(100.5, 39.5) - 1.f,
        "gtiff sample follows column height");
    // Fixture: heights[row]=col*100+row, north-up gt. Row 0 (north) is lower
    // than row 7 (south) in the encoded value — N/S must not be swapped.
    expect(dem.sample_meters(103.5, 39.5) < dem.sample_meters(103.5, 33.5),
           "gtiff north row stays north of south row");
  }

  {
    const std::string sample = render::find_sample_dem_path();
    if (!sample.empty()) {
      render::DemHeightField dem;
      expect(dem.load_gdal_raster(sample.c_str()), "load china_dem sample");
      expect(!dem.empty(), "china_dem not empty");
      expect(dem.sample_meters(88.0, 32.0) > dem.sample_meters(119.0, 32.5),
             "china_dem tibet higher than jiangsu");
    }
  }

  expect(
      render::label_priority_from_fields("北京市", "area", "", "110000") == 1,
      "provincial adcode");
  expect(
      render::label_priority_from_fields("南京市", "city", "", "320100") == 2,
      "prefecture adcode");
  expect(render::label_priority_from_fields("某县", "area", "", "320102") > 3,
         "county skipped at country view");
  expect(render::label_priority_from_fields("新疆", "region", "", "") == 1,
         "region kind");

  {
    render::MapLabelBox boxes[4];
    boxes[0] = {10, 10, 80, 28, 1};
    boxes[1] = {20, 12, 90, 30, 2};
    boxes[2] = {200, 40, 260, 58, 2};
    boxes[3] = {-400, -400, -300, -360, 0};
    std::vector<int> keep;
    const int n = render::declutter_map_labels(boxes, 4, 8, 400, 300, &keep);
    expect(n == 2, "declutter keeps two non-overlapping");
    expect(!keep.empty() && keep[0] == 0, "higher priority first");
    expect(n == 2 && keep[1] == 2, "second is far box");
  }

  // Prefecture-scale china_city rings (many vertices) must not do naive
  // point-in-polygon of every DEM cell against every ring on the UI thread.
  {
    render::DemHeightField dem;
    dem.fill_synthetic_china();
    std::vector<render::LonLatRing> rings;
    rings.reserve(80);
    for (int i = 0; i < 80; ++i) {
      render::LonLatRing ring;
      const double cx = 80.0 + static_cast<double>(i % 10) * 5.0;
      const double cy = 22.0 + static_cast<double>(i / 10) * 3.5;
      const int n = 120;
      ring.x.reserve(static_cast<size_t>(n));
      ring.y.reserve(static_cast<size_t>(n));
      for (int k = 0; k < n; ++k) {
        const double a = static_cast<double>(k) * 6.283185307179586 / n;
        ring.x.push_back(cx + 1.6 * std::cos(a));
        ring.y.push_back(cy + 1.2 * std::sin(a));
      }
      rings.push_back(std::move(ring));
    }
    const auto t0 = std::chrono::steady_clock::now();
    dem.mask_outside_rings(rings);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - t0)
                        .count();
    std::fprintf(stderr, "mask_outside_rings 80x120-gon: %lld ms\n",
                 static_cast<long long>(ms));
    expect(ms < 500, "prefecture-scale DEM mask stays off the UI-thread budget");
  }

  // SP4: DEM envelope + CPU mesh → gis::World kTerrain (GpuScene upload covered
  // in scene_gpu_test; avoid World vector ABI across legacy_render+gis+render).
  {
    render::DemHeightField dem;
    dem.fill_synthetic_china();
    double min_x = 0;
    double min_y = 0;
    double max_x = 0;
    double max_y = 0;
    dem.envelope(&min_x, &min_y, &max_x, &max_y);
    gis::World world;
    gis::Node* node =
        render::seed_dem_height_field_into_world(&world, dem, "china_dem", 48);
    expect(node != nullptr, "seed dem into world");
    expect(world.node_count() == 1, "one terrain node");
    expect(node->kind == gis::NodeKind::kTerrain, "kind terrain");
    expect(node->name == "china_dem", "terrain name");
    expect(node->min_x == min_x && node->max_x == max_x, "lon envelope");
    expect(node->min_y == min_y && node->max_y == max_y, "lat envelope");
    expect(node->max_z >= node->min_z, "elev span in World Z");
    const double expect_z0 =
        static_cast<double>(dem.min_meters() * dem.vertical_exaggeration());
    const double expect_z1 =
        static_cast<double>(dem.max_meters() * dem.vertical_exaggeration());
    expect(node->min_z == expect_z0 && node->max_z == expect_z1,
           "World Z matches exaggerated elev");
    expect(node->has_terrain_mesh(), "terrain cpu mesh attached");
    expect(node->terrain_positions.size() >= 9 &&
               (node->terrain_positions.size() % 3) == 0,
           "terrain positions xyz");
    expect(node->terrain_indices.size() >= 3 &&
               (node->terrain_indices.size() % 3) == 0,
           "terrain triangle indices");
  }

  expect(render::seed_dem_height_field_into_world(
             nullptr, render::DemHeightField{}, "x") == nullptr,
         "null world rejected");

  // SP4 knife 3: leftover Y-up AABB → GIS envelope (octree mirror pure seam).
  {
    double min_x = 0;
    double min_y = 0;
    double min_z = 0;
    double max_x = 0;
    double max_y = 0;
    double max_z = 0;
    render::leftover_yup_to_gis(100.0, 10.0, 20.0, 110.0, 50.0, 30.0, &min_x,
                                &min_y, &min_z, &max_x, &max_y, &max_z);
    expect(min_x == 100.0 && max_x == 110.0, "aabb lon");
    expect(min_y == 20.0 && max_y == 30.0, "aabb lat from leftover Z");
    expect(min_z == 10.0 && max_z == 50.0, "aabb elev → World Z");
    gis::World mirror;
    gis::Node* n =
        render::attach_gis_aabb(&mirror, "obj0", min_x, min_y, min_z, max_x,
                                max_y, max_z);
    expect(n != nullptr && n->kind == gis::NodeKind::kEmpty, "empty mirror");
    expect(mirror.node_count() == 1, "one mirror node");
  }

  std::error_code ec;
  std::filesystem::remove(tif, ec);

  if (g_fails != 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_fails);
    return 1;
  }
  return 0;
}
