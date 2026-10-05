// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/grid/dem/register.h"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "content/public/plugin_host.h"
#include "gis/geo/ops/geometry_traits.h"
#include "gis/geo/ops/indexed_tin.h"
#include "plugin/product/world3d/commands.h"
#include "plugin/product/world3d/detail/contribute.h"
#include "plugin/product/world3d/grid/dem/present/surface.h"
#include "plugin/product/world3d/grid/dem/loader/heightmap_loader.h"
#include "plugin/product/world3d/grid/dem/loader/trimesh_loader.h"
#include "plugin/product/world3d/grid/dem/dialog/heightmap_loader_dialog.h"
#include "plugin/product/world3d/grid/dem/dialog/trimesh_loader_dialog.h"
#include "plugin/runtime/host/processing/operation_result.h"
#include "plugin/runtime/widgets/about_dialog.h"
#include "plugin/runtime/widgets/owned_dialog.h"
#include "plugin/runtime/widgets/present_surface_picker.h"
#include "tool/command/command.h"

#include <rapidjson/document.h>

namespace plugin {
namespace {

bool commit_surface(content::PluginHost* host, const OGRTriangulatedSurface& surface,
                    const char* op) {
  const int triangle_count =
      const_cast<OGRTriangulatedSurface&>(surface).getNumGeometries();
  if (triangle_count < 1) {
    return false;
  }
  std::vector<double> xyz(static_cast<size_t>(triangle_count) * 9);
  std::vector<int> triangles(static_cast<size_t>(triangle_count) * 3);
  int live = 0;
  for (int t = 0; t < triangle_count; ++t) {
    OGRPoint a;
    OGRPoint b;
    OGRPoint c;
    if (!geo::tin_patch_points(surface, t, &a, &b, &c)) {
      continue;
    }
    const int base = live * 3;
    xyz[static_cast<size_t>(base) * 3] = a.getX();
    xyz[static_cast<size_t>(base) * 3 + 1] = a.getY();
    xyz[static_cast<size_t>(base) * 3 + 2] = a.getZ();
    xyz[static_cast<size_t>(base) * 3 + 3] = b.getX();
    xyz[static_cast<size_t>(base) * 3 + 4] = b.getY();
    xyz[static_cast<size_t>(base) * 3 + 5] = b.getZ();
    xyz[static_cast<size_t>(base) * 3 + 6] = c.getX();
    xyz[static_cast<size_t>(base) * 3 + 7] = c.getY();
    xyz[static_cast<size_t>(base) * 3 + 8] = c.getZ();
    triangles[static_cast<size_t>(live) * 3] = base;
    triangles[static_cast<size_t>(live) * 3 + 1] = base + 1;
    triangles[static_cast<size_t>(live) * 3 + 2] = base + 2;
    ++live;
  }
  if (live < 1) {
    return false;
  }
  if (!host) {
    return true;
  }
  if (!host->gis_document()) {
    return false;
  }
  if (!present_world3d_surface(host->gis_document(), host->scene3d_sink(),
                               xyz.data(), live * 3, triangles.data(), live,
                               op)) {
    return false;
  }
  (void)host->present_dataset("smartgis.world3d", "", 1);
  return true;
}

bool parse_args(std::string_view json, rapidjson::Document* out) {
  if (!out) {
    return false;
  }
  out->Parse(json.data(), static_cast<rapidjson::SizeType>(json.size()));
  return !out->HasParseError() && out->IsObject();
}

bool json_get_double(const rapidjson::Value& obj, const char* key, double* out) {
  if (!out || !key || !obj.IsObject()) {
    return false;
  }
  const auto it = obj.FindMember(key);
  if (it == obj.MemberEnd() || !it->value.IsNumber()) {
    return false;
  }
  *out = it->value.GetDouble();
  return true;
}

bool json_get_long(const rapidjson::Value& obj, const char* key, long* out) {
  if (!out || !key || !obj.IsObject()) {
    return false;
  }
  const auto it = obj.FindMember(key);
  if (it == obj.MemberEnd() || !it->value.IsNumber()) {
    return false;
  }
  *out = it->value.GetInt64();
  return true;
}

bool json_get_string(const rapidjson::Value& obj,
                     const char* key,
                     std::string* out) {
  if (!out || !key || !obj.IsObject()) {
    return false;
  }
  const auto it = obj.FindMember(key);
  if (it == obj.MemberEnd() || !it->value.IsString()) {
    return false;
  }
  *out = std::string(it->value.GetString(), it->value.GetStringLength());
  return true;
}

int separator_from_name(std::string_view name) {
  if (name == "tab") {
    return ST_TAB;
  }
  if (name == "comma") {
    return ST_COMMA;
  }
  return ST_SPACE;
}

bool trimesh_from_xyz(content::PluginHost* host, std::string_view args_json) {
  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    set_operation_result("{\"error\":\"bad_args\",\"op\":\"world3d.trimesh_from_xyz\"}");
    return false;
  }
  std::string vertex_path;
  if (!json_get_string(args, "vertex_path", &vertex_path) ||
      vertex_path.empty()) {
    set_operation_result("{\"error\":\"bad_args\",\"op\":\"world3d.trimesh_from_xyz\"}");
    return false;
  }

  std::string separator = "space";
  json_get_string(args, "separator", &separator);
  long head_skip = 0;
  long line_skip = 0;
  long col_x = 0;
  long col_y = 1;
  long col_z = 2;
  json_get_long(args, "head_skip", &head_skip);
  json_get_long(args, "line_skip", &line_skip);
  json_get_long(args, "col_x", &col_x);
  json_get_long(args, "col_y", &col_y);
  json_get_long(args, "col_z", &col_z);

  double x_scale = 0.05;
  double y_scale = 0.05;
  double z_scale = 0.05;
  json_get_double(args, "x_scale", &x_scale);
  json_get_double(args, "y_scale", &y_scale);
  json_get_double(args, "z_scale", &z_scale);

  const int n_col =
      std::max({static_cast<int>(col_x), static_cast<int>(col_y),
                static_cast<int>(col_z)}) +
      1;

  TrimeshFileFmt fmt;
  fmt.nSeparatorType = separator_from_name(separator);
  fmt.nCol = n_col;
  fmt.iX = static_cast<int>(col_x);
  fmt.iY = static_cast<int>(col_y);
  fmt.iZ = static_cast<int>(col_z);
  fmt.nHeadSkip = static_cast<int>(head_skip);
  fmt.nLineSkip = static_cast<int>(line_skip);

  OGRTriangulatedSurface surface;
  const long rc =
      load_ascii_xyz_trimesh(vertex_path.c_str(), fmt, static_cast<float>(x_scale),
                             static_cast<float>(y_scale),
                             static_cast<float>(z_scale), &surface);
  const int n_patch = surface.getNumGeometries();
  if (rc != geo::k_ok || n_patch < 1) {
    set_operation_result("{\"error\":\"load_failed\",\"op\":\"world3d.trimesh_from_xyz\"}");
    return false;
  }
  if (!commit_surface(host, surface, "world3d.trimesh_from_xyz")) {
    set_operation_result(
        std::string("{\"error\":\"no_map_seam\",\"op\":\"world3d.trimesh_from_xyz\",\"points\":") +
        std::to_string(n_patch * 3) + "}");
    return false;
  }
  set_operation_result(
      std::string("{\"ok\":true,\"op\":\"world3d.trimesh_from_xyz\",\"points\":") +
      std::to_string(n_patch * 3) + "}");
  return true;
}

bool heightmap_from_raster(content::PluginHost* host, std::string_view args_json) {
  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"world3d.heightmap_from_raster\"}");
    return false;
  }
  std::string heightmap_path;
  if (!json_get_string(args, "heightmap_path", &heightmap_path) ||
      heightmap_path.empty()) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"world3d.heightmap_from_raster\"}");
    return false;
  }

  HeightmapLoadOptions options;
  double x_scale = 1.0;
  double y_scale = 1.0;
  double z_scale = 0.1;
  double x_start = 0.0;
  double y_start = 0.0;
  double z_start = 0.0;
  json_get_double(args, "x_scale", &x_scale);
  json_get_double(args, "y_scale", &y_scale);
  json_get_double(args, "z_scale", &z_scale);
  json_get_double(args, "x_start", &x_start);
  json_get_double(args, "y_start", &y_start);
  json_get_double(args, "z_start", &z_start);
  options.x_scale = static_cast<float>(x_scale);
  options.y_scale = static_cast<float>(y_scale);
  options.z_scale = static_cast<float>(z_scale);
  options.x_start = static_cast<float>(x_start);
  options.y_start = static_cast<float>(y_start);
  options.z_start = static_cast<float>(z_start);

  OGRTriangulatedSurface surface;
  const long rc =
      load_heightmap(heightmap_path.c_str(), options, &surface);
  const int n_patch = surface.getNumGeometries();
  if (rc != geo::k_ok || n_patch < 2) {
    set_operation_result(
        "{\"error\":\"load_failed\",\"op\":\"world3d.heightmap_from_raster\"}");
    return false;
  }
  if (!commit_surface(host, surface, "world3d.heightmap_from_raster")) {
    set_operation_result(
        std::string(
            "{\"error\":\"no_map_seam\",\"op\":\"world3d.heightmap_from_raster\",\"points\":") +
        std::to_string(n_patch * 3) + "}");
    return false;
  }
  set_operation_result(
      std::string("{\"ok\":true,\"op\":\"world3d.heightmap_from_raster\",\"points\":") +
      std::to_string(n_patch * 3) + "}");
  return true;
}

void show_dialog(const wchar_t* title, std::unique_ptr<ui::views::View> body) {
  if (!body) {
    return;
  }
  const int width = body->preferred_size().width;
  const int height = body->preferred_size().height;
  show_owned_dialog(title, width, height, std::move(body));
}

}  // namespace

namespace detail {

bool register_world3d_dem(content::PluginHost* host) {
  if (!host) {
    return false;
  }

  if (!contribute_command_aliases(
          host, {{"world3d.load_trimesh", "离散点生成DEM"}}, "tools",
          [host](const tool::CommandArgs&) {
            return host->open_dialog("world3d.trimesh_loader");
          })) {
    return false;
  }
  if (!contribute_command_aliases(
          host, {{"world3d.load_heightmap", "高度图生成DEM"}}, "tools",
          [host](const tool::CommandArgs&) {
            return host->open_dialog("world3d.heightmap_loader");
          })) {
    return false;
  }
  if (!contribute_command_aliases(
          host, {{"world3d.about", "关于"}}, "tools",
          [host](const tool::CommandArgs&) {
            return host->open_dialog("world3d.about");
          })) {
    return false;
  }

  if (!host->contribute_dialog(
          kWorld3dPluginId, {"world3d.trimesh_loader", "离散点生成DEM"},
          [host](content::PluginHost*) {
            show_dialog(L"离散点生成DEM",
                        wrap_with_present_surface(
                            host, std::make_unique<TrimeshLoaderDialog>(host)));
          })) {
    return false;
  }
  if (!host->contribute_dialog(
          kWorld3dPluginId, {"world3d.heightmap_loader", "高度图生成DEM"},
          [host](content::PluginHost*) {
            show_dialog(L"高度图生成DEM",
                        wrap_with_present_surface(
                            host,
                            std::make_unique<HeightmapLoaderDialog>(host)));
          })) {
    return false;
  }
  if (!host->contribute_dialog(
          kWorld3dPluginId, {"world3d.about", "关于"},
          [](content::PluginHost*) {
            show_dialog(L"关于", std::make_unique<AboutDialog>(
                                     "DEM Creater\nSmartGIS builtin plugin"));
          })) {
    return false;
  }

  if (!contribute_processing_aliases(
          host, {{"world3d.trimesh_from_xyz", "Trimesh from XYZ"}},
          trimesh_from_xyz)) {
    return false;
  }
  return contribute_processing_aliases(
      host, {{"world3d.heightmap_from_raster", "Heightmap from raster"}},
      heightmap_from_raster);
}

}  // namespace detail
}  // namespace plugin
