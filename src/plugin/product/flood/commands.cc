// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/flood/commands.h"

#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>

#include "content/public/plugin_host.h"
#include "cpl_conv.h"
#include "gdal_priv.h"
#include "gis/analysis/raster/dem/flood_fill.h"
#include "plugin/product/flood/views/inundate_dialog.h"
#include "plugin/runtime/host/processing/operation_result.h"
#include "plugin/runtime/widgets/about_dialog.h"
#include "plugin/runtime/widgets/owned_dialog.h"
#include "tool/command/command.h"

#include <rapidjson/document.h>

namespace plugin {
namespace {

constexpr const char* kPluginId = "smartgis.flood";

FloodMaskWriter g_mask_writer;
std::string g_last_output;

bool parse_args(std::string_view json, rapidjson::Document* out) {
  if (!out) {
    return false;
  }
  out->Parse(json.data(), static_cast<rapidjson::SizeType>(json.size()));
  return !out->HasParseError() && out->IsObject();
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

bool json_get_int(const rapidjson::Value& obj, const char* key, int* out) {
  if (!out || !key || !obj.IsObject()) {
    return false;
  }
  const auto it = obj.FindMember(key);
  if (it == obj.MemberEnd() || !it->value.IsNumber()) {
    return false;
  }
  *out = it->value.GetInt();
  return true;
}

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

bool flood_inundate(content::PluginHost*, std::string_view args_json) {
  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    set_operation_result("{\"error\":\"bad_args\",\"op\":\"flood.inundate\"}");
    return false;
  }
  std::string dem;
  std::string output;
  if (!json_get_string(args, "dem", &dem) || dem.empty() ||
      !json_get_string(args, "output", &output) || output.empty()) {
    set_operation_result("{\"error\":\"bad_args\",\"op\":\"flood.inundate\"}");
    return false;
  }
  double seed_x = 0;
  double seed_y = 0;
  if (!json_get_double(args, "seed_x", &seed_x) ||
      !json_get_double(args, "seed_y", &seed_y)) {
    set_operation_result("{\"error\":\"bad_args\",\"op\":\"flood.inundate\"}");
    return false;
  }
  double water_level = 0;
  double water_depth = 0;
  const bool has_level = json_get_double(args, "water_level", &water_level);
  const bool has_depth = json_get_double(args, "water_depth", &water_depth);
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
  json_get_int(args, "frames", &frames);
  std::string frames_dir;
  json_get_string(args, "frames_dir", &frames_dir);

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

  if (!g_mask_writer) {
    set_operation_result(
        "{\"error\":\"no_flood_seam\",\"op\":\"flood.inundate\"}");
    return false;
  }
  const int frame_count =
      result.frame_masks.empty()
          ? 1
          : static_cast<int>(result.frame_masks.size());
  for (int i = 0; i < frame_count; ++i) {
    const unsigned char* mask =
        result.frame_masks.empty()
            ? result.mask.data()
            : result.frame_masks[static_cast<size_t>(i)].data();
    if (!g_mask_writer(mask, result.width, result.height, result.geotransform,
                       i, frame_count, result.water_level)) {
      set_operation_result(
          "{\"error\":\"no_flood_seam\",\"op\":\"flood.inundate\"}");
      return false;
    }
  }

  set_operation_result(
      std::string("{\"ok\":true,\"op\":\"flood.inundate\",\"width\":") +
      std::to_string(result.width) + ",\"height\":" +
      std::to_string(result.height) + "}");
  return true;
}

bool flood_export_mask(content::PluginHost*, std::string_view args_json) {
  rapidjson::Document args;
  std::string dest = g_last_output;
  if (parse_args(args_json, &args)) {
    std::string out;
    if (json_get_string(args, "output", &out) && !out.empty()) {
      dest = out;
    }
  }
  if (dest.empty()) {
    set_operation_result(
        "{\"error\":\"no_output\",\"op\":\"flood.export_mask\"}");
    return false;
  }
  if (!g_last_output.empty() && dest != g_last_output) {
    FILE* in = nullptr;
    FILE* out = nullptr;
    if (fopen_s(&in, g_last_output.c_str(), "rb") != 0 || !in) {
      set_operation_result(
          "{\"error\":\"missing_source\",\"op\":\"flood.export_mask\"}");
      return false;
    }
    if (fopen_s(&out, dest.c_str(), "wb") != 0 || !out) {
      fclose(in);
      set_operation_result(
          "{\"error\":\"write_failed\",\"op\":\"flood.export_mask\"}");
      return false;
    }
    char buf[4096];
    size_t n = 0;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
      if (fwrite(buf, 1, n, out) != n) {
        fclose(in);
        fclose(out);
        set_operation_result(
            "{\"error\":\"write_failed\",\"op\":\"flood.export_mask\"}");
        return false;
      }
    }
    fclose(in);
    fclose(out);
    g_last_output = dest;
  } else {
    FILE* f = nullptr;
    if (fopen_s(&f, dest.c_str(), "rb") != 0 || !f) {
      set_operation_result(
          "{\"error\":\"missing_output\",\"op\":\"flood.export_mask\"}");
      return false;
    }
    fclose(f);
  }
  set_operation_result(
      std::string("{\"ok\":true,\"op\":\"flood.export_mask\",\"output\":\"") +
      dest + "\"}");
  return true;
}

void show_dialog(const wchar_t* title, std::unique_ptr<ui::views::View> body) {
  if (!body) {
    return;
  }
  show_owned_dialog(title, body->preferred_size().width,
                    body->preferred_size().height, std::move(body));
}

}  // namespace

void set_flood_mask_writer(FloodMaskWriter writer) {
  g_mask_writer = std::move(writer);
}

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
            show_dialog(L"洪水淹没分析",
                        std::make_unique<InundateDialog>(host));
          })) {
    return false;
  }
  if (!host->contribute_dialog(
          kPluginId, {"flood.about", "关于"},
          [](content::PluginHost*) {
            show_dialog(L"关于",
                        std::make_unique<AboutDialog>(
                            "Flood\nDEM inundation (native.flood_fill)"));
          })) {
    return false;
  }
  return host->contribute_processing(
             kPluginId, {"flood.inundate", "DEM flood inundation"},
             flood_inundate) &&
         host->contribute_processing(
             kPluginId, {"flood.export_mask", "Export flood mask GeoTIFF"},
             flood_export_mask) &&
         host->contribute_command(
             kPluginId, "flood.export_mask", "导出淹没掩膜", "tools",
             [host](const tool::CommandArgs&) {
               return host->run_processing("flood.export_mask", "{}");
             });
}

}  // namespace plugin
