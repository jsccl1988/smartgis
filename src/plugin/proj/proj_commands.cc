// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/proj/proj_commands.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>
#include <string_view>

#include "content/public/plugin_host.h"
#include "plugin/proj/map_prj_dialog.h"
#include "tool/command.h"

#if defined(PLUGIN_PROJ_VIEWS_USE_PROJ_API)
#include "algorithm/proj/projection.h"
#endif

namespace plugin {
namespace {

constexpr const char kPluginId[] = "smartgis.proj";
// Guard against pathological grid args that would hang or OOM the host.
constexpr int kMaxGridCells = 250000;

TransformXyOutput g_last_xy_output;

bool is_finite_number(double v) {
  return std::isfinite(v) != 0;
}

bool json_get_double(std::string_view json, const char* key, double* out) {
  if (!out || !key || json.empty()) {
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
  if (end == start || !is_finite_number(v)) {
    return false;
  }
  *out = v;
  return true;
}

bool json_get_long(std::string_view json, const char* key, long* out) {
  if (!out || !key || json.empty()) {
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

bool run_transform_xy(std::string_view args_json) {
  g_last_xy_output = {};

  double l = 120.0;
  double b = 36.0;
  long scale_ruler = 1;
  if (!json_get_double(args_json, "L", &l) ||
      !json_get_double(args_json, "B", &b)) {
    return false;
  }
  json_get_long(args_json, "scale_ruler", &scale_ruler);
  if (scale_ruler <= 0) {
    scale_ruler = 1;
  }

#if defined(PLUGIN_PROJ_VIEWS_USE_PROJ_API)
  using namespace base;
  using namespace geo;

  Projection src = {};
  Projection dst = {};
  if (init_projection(&src) != SMT_ERR_NONE ||
      init_projection(&dst) != SMT_ERR_NONE ||
      load_longlat_ellipsoid(&src, kIugg1975A, kIugg1975B) != SMT_ERR_NONE ||
      load_tmerc_crs(&dst, kIugg1975A, kIugg1975B,
                     gauss_kruger_central_meridian(l)) != SMT_ERR_NONE) {
    free_projection(&src);
    free_projection(&dst);
    return false;
  }

  dbfPoint point(l, b);
  if (project_point(&src, &dst, &point) != SMT_ERR_NONE ||
      !is_finite_number(point.x) || !is_finite_number(point.y)) {
    free_projection(&src);
    free_projection(&dst);
    return false;
  }

  const double scale = static_cast<double>(scale_ruler);
  g_last_xy_output.x = point.x / scale;
  g_last_xy_output.y = point.y / scale;
  g_last_xy_output.valid = true;

  free_projection(&src);
  free_projection(&dst);
  return true;
#else
  (void)l;
  (void)b;
  (void)scale_ruler;
  return false;
#endif
}

bool run_transform_grid(std::string_view args_json) {
  double dl = 0.0;
  double db = 0.0;
  double lmin = 0.0;
  double bmin = 0.0;
  double lmax = 0.0;
  double bmax = 0.0;
  long scale_ruler = 1;
  if (!json_get_double(args_json, "dL", &dl) ||
      !json_get_double(args_json, "dB", &db) ||
      !json_get_double(args_json, "Lmin", &lmin) ||
      !json_get_double(args_json, "Bmin", &bmin) ||
      !json_get_double(args_json, "Lmax", &lmax) ||
      !json_get_double(args_json, "Bmax", &bmax)) {
    return false;
  }
  json_get_long(args_json, "scale_ruler", &scale_ruler);
  if (dl == 0.0 || db == 0.0 || scale_ruler <= 0) {
    return false;
  }

#if defined(PLUGIN_PROJ_VIEWS_USE_PROJ_API)
  using namespace base;
  using namespace geo;

  Projection src = {};
  Projection dst = {};
  const double lon_0 = gauss_kruger_central_meridian((lmax + lmin) * 0.5);
  if (init_projection(&src) != SMT_ERR_NONE ||
      init_projection(&dst) != SMT_ERR_NONE ||
      load_longlat_ellipsoid(&src, kIugg1975A, kIugg1975B) != SMT_ERR_NONE ||
      load_tmerc_crs(&dst, kIugg1975A, kIugg1975B, lon_0) != SMT_ERR_NONE) {
    free_projection(&src);
    free_projection(&dst);
    return false;
  }

  const double row_span = std::abs((bmax - bmin) / db);
  const double col_span = std::abs((lmax - lmin) / dl);
  if (!is_finite_number(row_span) || !is_finite_number(col_span) ||
      row_span > static_cast<double>(std::numeric_limits<int>::max() - 1) ||
      col_span > static_cast<double>(std::numeric_limits<int>::max() - 1)) {
    free_projection(&src);
    free_projection(&dst);
    return false;
  }

  const int n_row = static_cast<int>(row_span) + 1;
  const int n_col = static_cast<int>(col_span) + 1;
  if (n_row <= 0 || n_col <= 0) {
    free_projection(&src);
    free_projection(&dst);
    return false;
  }
  const int64_t cells =
      static_cast<int64_t>(n_row) * static_cast<int64_t>(n_col);
  if (cells <= 0 || cells > kMaxGridCells) {
    free_projection(&src);
    free_projection(&dst);
    return false;
  }

  const double scale = static_cast<double>(scale_ruler);
  bool ok = true;
  for (int i = 0; i < n_row && ok; ++i) {
    for (int j = 0; j < n_col; ++j) {
      dbfPoint point(j * dl + lmin, i * db + bmin);
      if (project_point(&src, &dst, &point) != SMT_ERR_NONE ||
          !is_finite_number(point.x) || !is_finite_number(point.y)) {
        ok = false;
        break;
      }
      (void)(point.x / scale);
      (void)(point.y / scale);
    }
  }

  free_projection(&src);
  free_projection(&dst);
  return ok;
#else
  (void)lmin;
  (void)bmin;
  (void)lmax;
  (void)bmax;
  return false;
#endif
}

}  // namespace

TransformXyOutput consume_transform_xy_output() {
  TransformXyOutput out = g_last_xy_output;
  g_last_xy_output.valid = false;
  return out;
}

bool register_proj(content::PluginHost* host) {
  if (!host) {
    return false;
  }

  if (!host->contribute_dialog(
          kPluginId, {"proj.dialog", "投影变换"}, [](content::PluginHost* h) {
            if (!h) {
              return;
            }
            // Views shell constructs the dialog; Widget hosting is owned by chrome.
            MapPrjDialog dialog(h);
            (void)dialog;
          })) {
    return false;
  }

  if (!host->contribute_processing(
          kPluginId, {"proj.transform_xy", "Transform XY"},
          [](content::PluginHost*, std::string_view args_json) {
            return run_transform_xy(args_json);
          })) {
    return false;
  }

  if (!host->contribute_processing(
          kPluginId, {"proj.transform_grid", "Transform grid"},
          [](content::PluginHost*, std::string_view args_json) {
            return run_transform_grid(args_json);
          })) {
    return false;
  }

  return host->contribute_command(kPluginId, "proj.do_prj", "投影变换", "tools",
                                  [host](const tool::CommandArgs&) {
                                    return host->open_dialog("proj.dialog");
                                  });
}

}  // namespace plugin
