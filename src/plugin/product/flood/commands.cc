// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/flood/commands.h"

#include <cmath>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/core/log.h"
#include "content/public/plugin_host.h"
#include "cpl_conv.h"
#include "gdal_priv.h"
#include "gis/analysis/raster/dem/flood_fill.h"
#include "plugin/product/flood/views/inundate_dialog.h"
#include "plugin/product/flood/present/present.h"
#include "plugin/runtime/host/capability/pack_ensure.h"
#include "plugin/runtime/host/processing/args_json.h"
#include "plugin/runtime/host/processing/operation_result.h"
#include "plugin/runtime/host/processing/playback_frames.h"
#include "plugin/runtime/host/processing/processing_phase.h"
#include "plugin/runtime/host/processing/reexport_file.h"
#include "plugin/runtime/widgets/about_dialog.h"
#include "plugin/runtime/widgets/owned_dialog.h"
#include "tool/command/command.h"

#include <rapidjson/document.h>

namespace plugin {
namespace {

constexpr const char* kPluginId = "smartgis.flood";

std::string g_last_output;
gis::detail::FloodFillResult g_last_fill;

bool sample_dem_z(const std::string& dem, double x, double y, double* z) {
  if (!z) {
    return false;
  }
  GDALAllRegister();
  GDALDatasetUniquePtr ds(
      static_cast<GDALDataset*>(GDALOpen(dem.c_str(), GA_ReadOnly)));
  if (!ds) {
    return false;
  }
  double gt[6] = {};
  if (ds->GetGeoTransform(gt) != CE_None) {
    return false;
  }
  const double det = gt[1] * gt[5] - gt[2] * gt[4];
  if (std::abs(det) < 1e-18) {
    return false;
  }
  const double dx = x - gt[0];
  const double dy = y - gt[3];
  const int col =
      static_cast<int>(std::floor((gt[5] * dx - gt[2] * dy) / det));
  const int row =
      static_cast<int>(std::floor((-gt[4] * dx + gt[1] * dy) / det));
  GDALRasterBand* band = ds->GetRasterBand(1);
  float sample = 0;
  if (!band ||
      band->RasterIO(GF_Read, col, row, 1, 1, &sample, 1, 1, GDT_Float32, 0, 0,
                     nullptr) != CE_None) {
    return false;
  }
  *z = static_cast<double>(sample);
  return true;
}

bool flood_inundate_compute(std::string_view args_json) {
  // A failed level must not leave the previous mask looking current.
  g_last_fill.ok = false;
  rapidjson::Document args;
  if (!parse_args_json(args_json, &args)) {
    set_operation_result("{\"error\":\"bad_args\",\"op\":\"flood.inundate\"}");
    return false;
  }
  std::string dem;
  std::string output;
  if (!args_json_string(args, "dem", &dem) || dem.empty() ||
      !args_json_string(args, "output", &output) || output.empty()) {
    set_operation_result("{\"error\":\"bad_args\",\"op\":\"flood.inundate\"}");
    return false;
  }
  double seed_x = 0;
  double seed_y = 0;
  if (!args_json_double(args, "seed_x", &seed_x) ||
      !args_json_double(args, "seed_y", &seed_y)) {
    set_operation_result("{\"error\":\"bad_args\",\"op\":\"flood.inundate\"}");
    return false;
  }
  double water_level = 0;
  double water_depth = 0;
  const bool has_level = args_json_double(args, "water_level", &water_level);
  const bool has_depth = args_json_double(args, "water_depth", &water_depth);
  if (!has_level && !has_depth) {
    set_operation_result("{\"error\":\"bad_args\",\"op\":\"flood.inundate\"}");
    return false;
  }
  if (!has_level) {
    double seed_z = 0;
    if (!sample_dem_z(dem, seed_x, seed_y, &seed_z)) {
      set_operation_result(
          "{\"error\":\"dem_sample_failed\",\"op\":\"flood.inundate\"}");
      return false;
    }
    water_level = seed_z + water_depth;
  }
  int frames = 1;
  args_json_int(args, "frames", &frames);
  std::string frames_dir;
  args_json_string(args, "frames_dir", &frames_dir);

  gis::detail::FloodFillResult result =
      gis::detail::run_flood_fill(dem, seed_x, seed_y, water_level, frames);
  if (!result.ok ||
      !gis::detail::write_flood_mask_geotiff(output, result, frames_dir)) {
    set_operation_result(
        std::string("{\"error\":\"") +
        (result.error.empty() ? "flood_failed" : result.error) +
        "\",\"op\":\"flood.inundate\"}");
    return false;
  }
  g_last_output = output;
  g_last_fill = std::move(result);
  set_operation_result(
      std::string("{\"ok\":true,\"op\":\"flood.inundate\",\"width\":") +
      std::to_string(g_last_fill.width) + ",\"height\":" +
      std::to_string(g_last_fill.height) + "}");
  return true;
}

bool flood_inundate_present(content::PluginHost* host) {
  if (!g_last_fill.ok) {
    set_operation_result(
        "{\"error\":\"flood_failed\",\"op\":\"flood.inundate\"}");
    return false;
  }
  const gis::detail::FloodFillResult& result = g_last_fill;
  const int frame_count =
      result.frame_masks.empty()
          ? 1
          : static_cast<int>(result.frame_masks.size());
  auto mask_at = [&](int i) -> const unsigned char* {
    return result.frame_masks.empty()
               ? result.mask.data()
               : result.frame_masks[static_cast<size_t>(i)].data();
  };
  content::GisDocument* gis =
      require_gis_document(host, "flood.inundate", "no_flood_seam");
  if (!gis) {
    LOGGING(LOG_ERROR, "flood.inundate: gis document missing");
    return false;
  }
  bool painted = present_flood_style(gis);
  for (int i = 0; painted && i < frame_count; ++i) {
    painted = present_flood_mask(
        gis, mask_at(i), result.width, result.height, result.geotransform,
        result.water_level, i == 0, /*add_water_standin=*/false);
  }
  if (!painted) {
    LOGGING(LOG_ERROR,
            "flood.inundate: present_flood_mask failed frames=%d %dx%d",
            frame_count, result.width, result.height);
    set_operation_result(
        "{\"error\":\"no_flood_seam\",\"op\":\"flood.inundate\"}");
    return false;
  }
  fill_playback_index_frames(host, frame_count);
  (void)host->present_dataset(kPluginId, "", 0);
  set_operation_result(
      std::string("{\"ok\":true,\"op\":\"flood.inundate\",\"width\":") +
      std::to_string(result.width) + ",\"height\":" +
      std::to_string(result.height) + "}");
  return true;
}

bool flood_inundate(content::PluginHost* host, std::string_view args_json) {
  return run_processing_phases(host, args_json, flood_inundate_compute,
                               flood_inundate_present);
}

// Same fill as flood.inundate, but the present phase does not paint.
// The level-curve pack batches masks, then presents one selected level.
bool flood_level_mask_present(content::PluginHost*) {
  if (!g_last_fill.ok) {
    set_operation_result(
        "{\"error\":\"flood_failed\",\"op\":\"flood.level_mask\"}");
    return false;
  }
  set_operation_result(
      std::string("{\"ok\":true,\"op\":\"flood.level_mask\",\"width\":") +
      std::to_string(g_last_fill.width) + ",\"height\":" +
      std::to_string(g_last_fill.height) + "}");
  return true;
}

bool flood_level_mask(content::PluginHost* host, std::string_view args_json) {
  return run_processing_phases(host, args_json, flood_inundate_compute,
                               flood_level_mask_present);
}

bool flood_export_mask(content::PluginHost*, std::string_view args_json) {
  return reexport_cached_file(&g_last_output, args_json, "flood.export_mask");
}

bool flood_present_frame(content::PluginHost* host,
                         std::string_view args_json) {
  if (!host || !g_last_fill.ok) {
    set_operation_result(
        "{\"error\":\"no_flood_session\",\"op\":\"flood.present_frame\"}");
    return false;
  }
  content::GisDocument* gis =
      require_gis_document(host, "flood.present_frame", "no_flood_seam");
  if (!gis) {
    return false;
  }
  const int frame_count =
      g_last_fill.frame_masks.empty()
          ? 1
          : static_cast<int>(g_last_fill.frame_masks.size());
  const int index = parse_frame_index_arg(args_json, frame_count);
  const unsigned char* mask =
      g_last_fill.frame_masks.empty()
          ? g_last_fill.mask.data()
          : g_last_fill.frame_masks[static_cast<size_t>(index)].data();
  if (!present_flood_style(gis) ||
      !present_flood_mask(gis, mask, g_last_fill.width, g_last_fill.height,
                          g_last_fill.geotransform, g_last_fill.water_level,
                          /*rebuild_terrain=*/true,
                          /*add_water_standin=*/true)) {
    set_operation_result(
        "{\"error\":\"present_failed\",\"op\":\"flood.present_frame\"}");
    return false;
  }
  set_playback_frame_index(host, index);
  set_operation_result("{\"ok\":true,\"op\":\"flood.present_frame\"}");
  return true;
}

}  // namespace

bool register_flood(content::PluginHost* host) {
  if (!host) {
    return false;
  }
  if (!host->contribute_command(
          kPluginId, "flood.inundate", "洪水淹没分析", "tools",
          [host](const tool::CommandArgs&) {
            return host->open_dialog("flood.inundate");
          })) {
    return false;
  }
  if (!host->contribute_command(
          kPluginId, "flood.about", "关于", "tools",
          [host](const tool::CommandArgs&) {
            return host->open_dialog("flood.about");
          })) {
    return false;
  }
  if (!host->contribute_dialog(
          kPluginId, {"flood.inundate", "洪水淹没分析"},
          [host](content::PluginHost*) {
            present_dialog(L"洪水淹没分析",
                             std::make_unique<InundateDialog>(host));
          })) {
    return false;
  }
  if (!host->contribute_dialog(
          kPluginId, {"flood.about", "关于"},
          [](content::PluginHost*) {
            present_dialog(L"关于",
                             std::make_unique<AboutDialog>(
                                 "Flood\nDEM inundation (native.flood_fill)"));
          })) {
    return false;
  }
  return host->contribute_processing(
             kPluginId, {"flood.inundate", "DEM flood inundation"},
             flood_inundate) &&
         host->contribute_processing(
             kPluginId,
             {"flood.level_mask", "One water level mask without present"},
             flood_level_mask) &&
         host->contribute_processing(
             kPluginId, {"flood.export_mask", "Export flood mask GeoTIFF"},
             flood_export_mask) &&
         host->contribute_processing(
             kPluginId, {"flood.present_frame", "Re-present flood frame"},
             flood_present_frame) &&
         host->contribute_export_frame(
             kPluginId, {"flood_wuhan", 114.15, 30.45, 114.45, 30.65}) &&
         host->contribute_command(
             kPluginId, "flood.export_mask", "导出淹没掩膜", "tools",
             [host](const tool::CommandArgs&) {
               return host->run_processing("flood.export_mask", "{}");
             });
}

namespace {

struct FloodPackOnce {
  FloodPackOnce() { register_command_pack("flood", register_flood); }
} k_flood_pack_once;

}  // namespace

}  // namespace plugin
