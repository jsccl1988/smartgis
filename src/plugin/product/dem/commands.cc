// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/dem/commands.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

#include "legacy/core/macros/macros.h"
#include <memory>

#include "content/public/plugin_host.h"
#include "plugin/product/dem/processing/grid_loader.h"
#include "plugin/product/dem/processing/tin_loader.h"
#include "plugin/product/dem/views/grid_loader_dialog.h"
#include "plugin/product/dem/views/tin_loader_dialog.h"
#include "plugin/runtime/host/operation_result.h"
#include "plugin/runtime/widgets/about_dialog.h"
#include "plugin/runtime/widgets/owned_dialog.h"
#include "tool/command/command.h"

#include <rapidjson/document.h>

namespace plugin {
namespace {

constexpr const char* kPluginId = "smartgis.dem";

DemSurfaceWriter g_surface_writer;

void set_dem_surface_writer_store(DemSurfaceWriter writer) {
  g_surface_writer = std::move(writer);
}

bool commit_surface(const Smt3DSurface& surface, const char* op) {
  const int point_count = surface.get_point_count();
  const int triangle_count = surface.get_triangle_count();
  if (!g_surface_writer || point_count < 3 || triangle_count < 1) {
    return false;
  }
  std::vector<double> xyz(static_cast<size_t>(point_count) * 3);
  for (int i = 0; i < point_count; ++i) {
    const OGRPoint point = surface.get_point(i);
    xyz[static_cast<size_t>(i) * 3] = point.getX();
    xyz[static_cast<size_t>(i) * 3 + 1] = point.getY();
    xyz[static_cast<size_t>(i) * 3 + 2] = point.getZ();
  }
  std::vector<int> triangles(static_cast<size_t>(triangle_count) * 3);
  int live = 0;
  for (int t = 0; t < triangle_count; ++t) {
    const base::SmtTriangle tri = surface.get_triangle(t);
    if (tri.bDelete) {
      continue;
    }
    triangles[static_cast<size_t>(live) * 3] = static_cast<int>(tri.a);
    triangles[static_cast<size_t>(live) * 3 + 1] = static_cast<int>(tri.b);
    triangles[static_cast<size_t>(live) * 3 + 2] = static_cast<int>(tri.c);
    ++live;
  }
  if (live < 1) {
    return false;
  }
  return g_surface_writer(xyz.data(), point_count, triangles.data(), live, op);
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
  if (name == "space") {
    return ST_SPACE;
  }
  return ST_COMMA;
}

// Keys match TinLoaderDialog::build_json / GridLoaderDialog::build_json.
bool tin_from_xyz(content::PluginHost*, std::string_view args_json) {
  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    set_operation_result("{\"error\":\"bad_args\",\"op\":\"dem.tin_from_xyz\"}");
    return false;
  }
  std::string vertex_path;
  if (!json_get_string(args, "vertex_path", &vertex_path) ||
      vertex_path.empty()) {
    set_operation_result("{\"error\":\"bad_args\",\"op\":\"dem.tin_from_xyz\"}");
    return false;
  }

  std::string separator = "comma";
  json_get_string(args, "separator", &separator);

  long head_skip = 1;
  long line_skip = 4;
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

  TinFileFmt fmt;
  fmt.nSeparatorType = separator_from_name(separator);
  fmt.nCol = n_col;
  fmt.iX = static_cast<int>(col_x);
  fmt.iY = static_cast<int>(col_y);
  fmt.iZ = static_cast<int>(col_z);
  fmt.nHeadSkip = static_cast<int>(head_skip);
  fmt.nLineSkip = static_cast<int>(line_skip);

  Smt3DSurface surface;
  const long rc =
      load_ascii_xyz_tin(vertex_path.c_str(), fmt, static_cast<float>(x_scale),
                         static_cast<float>(y_scale),
                         static_cast<float>(z_scale), &surface);
  if (rc != SMT_ERR_NONE || surface.get_point_count() < 3) {
    set_operation_result("{\"error\":\"load_failed\",\"op\":\"dem.tin_from_xyz\"}");
    return false;
  }
  if (!commit_surface(surface, "dem.tin_from_xyz")) {
    set_operation_result(
        std::string("{\"error\":\"no_map_seam\",\"op\":\"dem.tin_from_xyz\",\"points\":") +
        std::to_string(surface.get_point_count()) + "}");
    return false;
  }
  set_operation_result(
      std::string("{\"ok\":true,\"op\":\"dem.tin_from_xyz\",\"points\":") +
      std::to_string(surface.get_point_count()) + "}");
  return true;
}

bool grid_from_heightmap(content::PluginHost*, std::string_view args_json) {
  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"dem.grid_from_heightmap\"}");
    return false;
  }
  std::string heightmap_path;
  if (!json_get_string(args, "heightmap_path", &heightmap_path) ||
      heightmap_path.empty()) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"dem.grid_from_heightmap\"}");
    return false;
  }

  GridLoadOptions options;
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

  Smt3DSurface surface;
  const long rc =
      load_heightmap_grid(heightmap_path.c_str(), options, &surface);
  if (rc != SMT_ERR_NONE || surface.get_point_count() < 4) {
    set_operation_result(
        "{\"error\":\"load_failed\",\"op\":\"dem.grid_from_heightmap\"}");
    return false;
  }
  if (!commit_surface(surface, "dem.grid_from_heightmap")) {
    set_operation_result(
        std::string(
            "{\"error\":\"no_map_seam\",\"op\":\"dem.grid_from_heightmap\",\"points\":") +
        std::to_string(surface.get_point_count()) + "}");
    return false;
  }
  set_operation_result(
      std::string("{\"ok\":true,\"op\":\"dem.grid_from_heightmap\",\"points\":") +
      std::to_string(surface.get_point_count()) + "}");
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

void set_dem_surface_writer(DemSurfaceWriter writer) {
  set_dem_surface_writer_store(std::move(writer));
}

bool register_dem(content::PluginHost* host) {
  if (!host) {
    return false;
  }

  if (!host->contribute_command(
          kPluginId, "dem.load_tin", "离散点生成DEM", "tools",
          [host](const tool::CommandArgs&) {
            return host->open_dialog("dem.tin_loader");
          })) {
    return false;
  }
  if (!host->contribute_command(
          kPluginId, "dem.load_grid", "高度图生成DEM", "tools",
          [host](const tool::CommandArgs&) {
            return host->open_dialog("dem.grid_loader");
          })) {
    return false;
  }
  if (!host->contribute_command(
          kPluginId, "dem.about", "关于", "tools",
          [host](const tool::CommandArgs&) {
            return host->open_dialog("dem.about");
          })) {
    return false;
  }

  if (!host->contribute_dialog(
          kPluginId, {"dem.tin_loader", "离散点生成DEM"},
          [host](content::PluginHost*) {
            show_dialog(L"离散点生成DEM",
                        std::make_unique<TinLoaderDialog>(host));
          })) {
    return false;
  }
  if (!host->contribute_dialog(
          kPluginId, {"dem.grid_loader", "高度图生成DEM"},
          [host](content::PluginHost*) {
            show_dialog(L"高度图生成DEM",
                        std::make_unique<GridLoaderDialog>(host));
          })) {
    return false;
  }
  if (!host->contribute_dialog(
          kPluginId, {"dem.about", "关于"},
          [](content::PluginHost*) {
            show_dialog(L"关于", std::make_unique<AboutDialog>(
                                     "DEM Creater\nSmartGIS builtin plugin"));
          })) {
    return false;
  }

  if (!host->contribute_processing(
          kPluginId, {"dem.tin_from_xyz", "TIN from XYZ"}, tin_from_xyz)) {
    return false;
  }
  if (!host->contribute_processing(
          kPluginId, {"dem.grid_from_heightmap", "Heightmap to DEM grid"},
          grid_from_heightmap)) {
    return false;
  }

  return true;
}

}  // namespace plugin
