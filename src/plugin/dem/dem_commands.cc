// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/dem/dem_commands.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <string_view>

#include "base/core/core.h"
#include "content/public/plugin_host.h"
#include "plugin/dem/grid_loader.h"
#include "plugin/dem/grid_loader_dialog.h"
#include "plugin/dem/tin_loader.h"
#include "plugin/dem/tin_loader_dialog.h"
#include "plugin/widgets/about_dialog.h"
#include "tool/command.h"

namespace plugin {
namespace {

constexpr const char* kPluginId = "smartgis.dem";

bool json_get_double(std::string_view json, const char* key, double* out) {
  if (!out || !key) {
    return false;
  }
  const std::string needle = std::string("\"") + key + "\":";
  const size_t pos = json.find(needle);
  if (pos == std::string_view::npos) {
    return false;
  }
  const char* start = json.data() + pos + needle.size();
  char* end = nullptr;
  const double v = std::strtod(start, &end);
  if (end == start) {
    return false;
  }
  *out = v;
  return true;
}

bool json_get_long(std::string_view json, const char* key, long* out) {
  if (!out || !key) {
    return false;
  }
  const std::string needle = std::string("\"") + key + "\":";
  const size_t pos = json.find(needle);
  if (pos == std::string_view::npos) {
    return false;
  }
  const char* start = json.data() + pos + needle.size();
  char* end = nullptr;
  const long v = std::strtol(start, &end, 10);
  if (end == start) {
    return false;
  }
  *out = v;
  return true;
}

bool json_get_string(std::string_view json, const char* key, std::string* out) {
  if (!out || !key) {
    return false;
  }
  const std::string needle = std::string("\"") + key + "\":\"";
  const size_t pos = json.find(needle);
  if (pos == std::string_view::npos) {
    return false;
  }
  size_t i = pos + needle.size();
  std::string value;
  while (i < json.size() && json[i] != '"') {
    if (json[i] == '\\' && i + 1 < json.size()) {
      value += json[i + 1];
      i += 2;
      continue;
    }
    value += json[i];
    ++i;
  }
  if (i >= json.size() || json[i] != '"') {
    return false;
  }
  *out = value;
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
  std::string vertex_path;
  if (!json_get_string(args_json, "vertex_path", &vertex_path) ||
      vertex_path.empty()) {
    return false;
  }

  std::string separator = "comma";
  json_get_string(args_json, "separator", &separator);

  long head_skip = 1;
  long line_skip = 4;
  long col_x = 0;
  long col_y = 1;
  long col_z = 2;
  json_get_long(args_json, "head_skip", &head_skip);
  json_get_long(args_json, "line_skip", &line_skip);
  json_get_long(args_json, "col_x", &col_x);
  json_get_long(args_json, "col_y", &col_y);
  json_get_long(args_json, "col_z", &col_z);

  double x_scale = 0.05;
  double y_scale = 0.05;
  double z_scale = 0.05;
  json_get_double(args_json, "x_scale", &x_scale);
  json_get_double(args_json, "y_scale", &y_scale);
  json_get_double(args_json, "z_scale", &z_scale);

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
  // No EditSession / map write seam on the Views host yet; success means the
  // surface was built. Texture / 2D layer fields are ignored until that lands.
  return rc == SMT_ERR_NONE && surface.get_point_count() >= 3;
}

bool grid_from_heightmap(content::PluginHost*, std::string_view args_json) {
  std::string heightmap_path;
  if (!json_get_string(args_json, "heightmap_path", &heightmap_path) ||
      heightmap_path.empty()) {
    return false;
  }

  GridLoadOptions options;
  double x_scale = 1.0;
  double y_scale = 1.0;
  double z_scale = 0.1;
  double x_start = 0.0;
  double y_start = 0.0;
  double z_start = 0.0;
  json_get_double(args_json, "x_scale", &x_scale);
  json_get_double(args_json, "y_scale", &y_scale);
  json_get_double(args_json, "z_scale", &z_scale);
  json_get_double(args_json, "x_start", &x_start);
  json_get_double(args_json, "y_start", &y_start);
  json_get_double(args_json, "z_start", &z_start);
  options.x_scale = static_cast<float>(x_scale);
  options.y_scale = static_cast<float>(y_scale);
  options.z_scale = static_cast<float>(z_scale);
  options.x_start = static_cast<float>(x_start);
  options.y_start = static_cast<float>(y_start);
  options.z_start = static_cast<float>(z_start);

  Smt3DSurface surface;
  const long rc =
      load_heightmap_grid(heightmap_path.c_str(), options, &surface);
  return rc == SMT_ERR_NONE && surface.get_point_count() >= 4;
}

}  // namespace

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
          [host](content::PluginHost*) { TinLoaderDialog dialog(host); })) {
    return false;
  }
  if (!host->contribute_dialog(
          kPluginId, {"dem.grid_loader", "高度图生成DEM"},
          [host](content::PluginHost*) { GridLoaderDialog dialog(host); })) {
    return false;
  }
  if (!host->contribute_dialog(
          kPluginId, {"dem.about", "关于"},
          [](content::PluginHost*) {
            AboutDialog dialog("DEM Creater\nSmartGIS builtin plugin");
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
