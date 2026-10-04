// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Load a public china_dem.tif subsample through PointCloud3d::Read3DPointCloud.
// DEM source: Mapzen/Nextzen terrain stack (SRTM/GMTED/…); see
// testing/data/china_city.LICENSE.txt.

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "vista/terrain/dem/dem_height_field.h"
#include "scenic/scene3d/primitive/surface/pointcloud.h"

namespace {

int g_fails = 0;

void expect(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

void hypsometric_rgb(float z_m, int* r, int* g, int* b) {
  if (z_m < 50.f) {
    *r = 40;
    *g = 120;
    *b = 60;
  } else if (z_m < 500.f) {
    *r = 90;
    *g = 150;
    *b = 70;
  } else if (z_m < 1500.f) {
    *r = 160;
    *g = 140;
    *b = 80;
  } else if (z_m < 3500.f) {
    *r = 140;
    *g = 110;
    *b = 70;
  } else {
    *r = 230;
    *g = 230;
    *b = 235;
  }
}

// Write leftover Read3DPointCloud CSV: x,z,y,r,g,b (lon, elev_m, lat, RGB).
bool write_public_dem_cloud(const render::DemHeightField& dem,
                            const std::filesystem::path& out_path,
                            size_t* wrote) {
  double minx = 0;
  double miny = 0;
  double maxx = 0;
  double maxy = 0;
  dem.envelope(&minx, &miny, &maxx, &maxy);
  const int stride = 8;
  const int cols = dem.cols();
  const int rows = dem.rows();
  if (cols < 4 || rows < 4) {
    return false;
  }
  std::ofstream out(out_path, std::ios::out | std::ios::trunc);
  if (!out) {
    return false;
  }
  size_t n = 0;
  for (int row = 0; row < rows; row += stride) {
    for (int col = 0; col < cols; col += stride) {
      const double t = (cols <= 1) ? 0.0 : static_cast<double>(col) / (cols - 1);
      const double u = (rows <= 1) ? 0.0 : static_cast<double>(row) / (rows - 1);
      const double lon = minx + t * (maxx - minx);
      // Envelope miny/maxy are south/north; row 0 is north in DEM samples.
      const double lat = maxy - u * (maxy - miny);
      const float z = dem.sample_meters(lon, lat);
      if (!(z == z) || z < -500.f) {
        continue;
      }
      int r = 0;
      int g = 0;
      int b = 0;
      hypsometric_rgb(z, &r, &g, &b);
      out << lon << ',' << z << ',' << lat << ',' << r << ',' << g << ',' << b
          << '\n';
      ++n;
    }
  }
  *wrote = n;
  return n >= 100u;
}

}  // namespace

int main() {
  const std::string dem_path = render::find_sample_dem_path();
  expect(!dem_path.empty(), "find china_dem.tif sample");
  if (dem_path.empty()) {
    std::fprintf(stderr, "missing china_dem — copy testing/data via china_map_samples\n");
    return 2;
  }

  render::DemHeightField dem;
  expect(dem.load_gdal_raster(dem_path.c_str()), "load public china_dem.tif");
  expect(!dem.empty(), "china_dem not empty");
  expect(dem.sample_meters(88.0, 32.0) > dem.sample_meters(119.0, 32.5),
         "tibet higher than jiangsu (public DEM signal)");

  const auto tmp =
      std::filesystem::temp_directory_path() / "smartgis_pointcloud_public.txt";
  size_t wrote = 0;
  expect(write_public_dem_cloud(dem, tmp, &wrote), "write DEM→pointcloud CSV");
  expect(wrote >= 100u, "enough public DEM samples");

  scenic::detail::PointCloud3d cloud;
  const std::string cloud_path = tmp.string();
  expect(cloud.read_point_cloud(cloud_path),
         "read_point_cloud public DEM subsample");

  // Second parse must also succeed (format stable / non-empty).
  scenic::detail::PointCloud3d cloud2;
  expect(cloud2.read_point_cloud(cloud_path), "re-read public sample");

  std::error_code ec;
  std::filesystem::remove(tmp, ec);

  if (g_fails != 0) {
    std::fprintf(stderr, "pointcloud_load_test: %d failed\n", g_fails);
    return 1;
  }
  std::fprintf(stdout, "pointcloud_load_test: ok dem=%s wrote=%zu\n",
               dem_path.c_str(), wrote);
  return 0;
}
