// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/commands.h"
#include "plugin/product/world3d/grid/dem/loader/heightmap_loader.h"
#include "plugin/product/world3d/grid/dem/loader/trimesh_loader.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "gis/geo/ops/geometry_traits.h"
#include "content/public/plugin_host.h"
#include "gdal_priv.h"
#include "plugin/runtime/host/processing/operation_result.h"

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

std::filesystem::path write_xyz_fixture() {
  const auto path =
      std::filesystem::temp_directory_path() / "smartgis_dem_trimesh_fixture.xyz";
  std::ofstream out(path);
  out << "x,y,z\n";
  out << "0,0,0\n";
  out << "1,0,0\n";
  out << "1,1,0\n";
  out << "0,1,0\n";
  out.close();
  return path;
}

std::filesystem::path write_gtiff_fixture() {
  const auto path =
      std::filesystem::temp_directory_path() / "smartgis_dem_heightmap_fixture.tif";
  GDALAllRegister();
  GDALDriver* driver =
      GetGDALDriverManager()->GetDriverByName("GTiff");
  if (!driver) {
    return {};
  }
  GDALDataset* dataset = driver->Create(path.string().c_str(), 2, 2, 1,
                                        GDT_Float32, nullptr);
  if (!dataset) {
    return {};
  }
  float heights[4] = {1.f, 2.f, 3.f, 4.f};
  CPLErr err = dataset->GetRasterBand(1)->RasterIO(
      GF_Write, 0, 0, 2, 2, heights, 2, 2, GDT_Float32, 0, 0);
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
  const std::filesystem::path xyz = write_xyz_fixture();
  {
    plugin::TrimeshFileFmt fmt;
    fmt.nSeparatorType = plugin::ST_COMMA;
    fmt.nCol = 3;
    fmt.iX = 0;
    fmt.iY = 1;
    fmt.iZ = 2;
    fmt.nHeadSkip = 1;
    fmt.nLineSkip = 0;

    OGRTriangulatedSurface surface;
    expect(plugin::load_ascii_xyz_trimesh(xyz.string().c_str(), fmt, 1.f, 1.f, 1.f,
                                          &surface) == geo::k_ok,
           "load_ascii_xyz_trimesh");
    expect(surface.getNumGeometries() >= 1, "trimesh has triangles");
  }

  const std::filesystem::path tif = write_gtiff_fixture();
  expect(!tif.empty(), "write gtiff fixture");
  {
    plugin::HeightmapLoadOptions options;
    options.x_scale = 1.f;
    options.y_scale = 1.f;
    options.z_scale = 1.f;
    OGRTriangulatedSurface surface;
    expect(plugin::load_heightmap(tif.string().c_str(), options,
                                  &surface) == geo::k_ok,
           "load_heightmap");
    expect(surface.getNumGeometries() == 2, "heightmap has 2 triangles");
  }

  {
    content::PluginHost* host =
        content::create_plugin_host(nullptr, nullptr, nullptr);
    expect(plugin::register_world3d(host), "register_world3d");

    std::string path_esc;
    for (char c : xyz.string()) {
      if (c == '\\') {
        path_esc += "\\\\";
      } else {
        path_esc += c;
      }
    }
    const std::string trimesh_args =
        std::string("{\"vertex_path\":\"") + path_esc +
        "\",\"separator\":\"comma\",\"head_skip\":1,\"line_skip\":0,"
        "\"col_x\":0,\"col_y\":1,\"col_z\":2,\"x_scale\":1,\"y_scale\":1,"
        "\"z_scale\":1}";
    expect(!host->run_processing("world3d.trimesh_from_xyz", trimesh_args),
           "world3d.trimesh_from_xyz refuses without a map seam");
    expect(plugin::operation_result().find("no_map_seam") != std::string::npos,
           "world3d.trimesh_from_xyz structured error");

    std::string tif_esc;
    for (char c : tif.string()) {
      if (c == '\\') {
        tif_esc += "\\\\";
      } else {
        tif_esc += c;
      }
    }
    const std::string heightmap_args =
        std::string("{\"heightmap_path\":\"") + tif_esc +
        "\",\"x_scale\":1,\"y_scale\":1,\"z_scale\":1,\"x_start\":0,"
        "\"y_start\":0,\"z_start\":0}";
    expect(!host->run_processing("world3d.heightmap_from_raster", heightmap_args),
           "world3d.heightmap_from_raster refuses without a map seam");
    expect(plugin::operation_result().find("no_map_seam") != std::string::npos,
           "world3d.heightmap_from_raster structured error");

    expect(!host->run_processing(
               "world3d.trimesh_from_xyz",
               "{\"vertex_path\":\"Z:/no/such/dem_fixture.xyz\"}"),
           "missing xyz returns false");
    expect(!host->run_processing(
               "world3d.heightmap_from_raster",
               "{\"heightmap_path\":\"Z:/no/such/dem_fixture.tif\"}"),
           "missing heightmap returns false");

    delete host;
  }

  std::error_code ec;
  std::filesystem::remove(xyz, ec);
  std::filesystem::remove(tif, ec);

  if (g_fails != 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_fails);
    return 1;
  }
  return 0;
}
