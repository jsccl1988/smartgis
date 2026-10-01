// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/stormsurge/commands.h"

#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "content/public/plugin_host.h"
#include "gis/analysis/raster/dem/storm_surge.h"
#include "gis/analysis/raster/dem/storm_surge_stats.h"
#include "plugin/product/stormsurge/views/run_dialog.h"
#include "plugin/runtime/host/processing/operation_result.h"
#include "plugin/runtime/widgets/about_dialog.h"
#include "plugin/runtime/widgets/owned_dialog.h"
#include "tool/command/command.h"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace plugin {
namespace {

constexpr const char* kPluginId = "smartgis.stormsurge";

StormSurgeMaskWriter g_mask_writer;
StormSurgeWaterMeshWriter g_water_mesh_writer;
std::string g_last_output;
std::string g_last_depth_output;
std::string g_coast_path;

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

double result_water_level(const gis::detail::StormSurgeResult& result) {
  if (!result.surge_levels.empty()) {
    return result.surge_levels.back();
  }
  return 0.0;
}

bool stormsurge_run(content::PluginHost*, std::string_view args_json) {
  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    set_operation_result("{\"error\":\"bad_args\",\"op\":\"stormsurge.run\"}");
    return false;
  }
  std::string dem;
  std::string output;
  if (!json_get_string(args, "dem", &dem) || dem.empty() ||
      !json_get_string(args, "output", &output) || output.empty()) {
    set_operation_result("{\"error\":\"bad_args\",\"op\":\"stormsurge.run\"}");
    return false;
  }
  std::string coast;
  json_get_string(args, "coast", &coast);
  if (coast.empty()) {
    json_get_string(args, "shoreline", &coast);
  }
  if (coast.empty()) {
    coast = g_coast_path;
  }

  std::vector<double> seed_xy;
  double seed_x = 0;
  double seed_y = 0;
  if (json_get_double(args, "seed_x", &seed_x) &&
      json_get_double(args, "seed_y", &seed_y)) {
    seed_xy.push_back(seed_x);
    seed_xy.push_back(seed_y);
  }

  std::vector<double> levels;
  const auto levels_it = args.FindMember("surge_levels");
  if (levels_it != args.MemberEnd() && levels_it->value.IsArray()) {
    for (const auto& v : levels_it->value.GetArray()) {
      if (v.IsNumber()) {
        levels.push_back(v.GetDouble());
      }
    }
  }
  if (levels.empty()) {
    double tide_level = 0;
    double water_level = 0;
    if (json_get_double(args, "tide_level", &tide_level)) {
      levels.push_back(tide_level);
    } else if (json_get_double(args, "water_level", &water_level)) {
      levels.push_back(water_level);
    } else {
      double water_depth = 0;
      if (json_get_double(args, "water_depth", &water_depth)) {
        // Kernel op resolves depth→absolute; here require absolute level.
        set_operation_result(
            "{\"error\":\"need_tide_or_level\",\"op\":\"stormsurge.run\"}");
        return false;
      }
    }
  }
  // Optional tide CSV path is accepted by native.storm_surge via run_storm_surge_op;
  // product path prefers explicit levels; pass through when levels still empty.
  std::string tide_series;
  json_get_string(args, "tide", &tide_series);
  if (levels.empty() && !tide_series.empty()) {
    // Delegate series parse to catalog op (writes files + returns ok).
    if (!gis::detail::run_storm_surge_op(args_json)) {
      set_operation_result(
          "{\"error\":\"stormsurge_failed\",\"op\":\"stormsurge.run\"}");
      return false;
    }
    g_last_output = output;
    if (!coast.empty()) {
      g_coast_path = coast;
    }
    set_operation_result(
        "{\"ok\":true,\"op\":\"stormsurge.run\",\"kernel\":\"native.storm_surge\","
        "\"viz\":\"file_only\"}");
    return true;
  }
  if (levels.empty()) {
    set_operation_result("{\"error\":\"bad_args\",\"op\":\"stormsurge.run\"}");
    return false;
  }
  if (coast.empty() && seed_xy.size() < 2) {
    set_operation_result(
        "{\"error\":\"need_coast_or_seed\",\"op\":\"stormsurge.run\"}");
    return false;
  }

  int frames = 1;
  json_get_int(args, "frames", &frames);
  std::string frames_dir;
  json_get_string(args, "frames_dir", &frames_dir);

  gis::detail::StormSurgeResult result =
      gis::detail::run_storm_surge(dem, coast, seed_xy, levels, frames);
  if (!result.ok ||
      !gis::detail::write_storm_surge_mask_geotiff(output, result, frames_dir)) {
    set_operation_result(
        std::string("{\"error\":\"") +
        (result.error.empty() ? "stormsurge_failed" : result.error) +
        "\",\"op\":\"stormsurge.run\"}");
    return false;
  }
  g_last_output = output;
  if (!coast.empty()) {
    g_coast_path = coast;
  }

  std::string depth_output;
  json_get_string(args, "depth_output", &depth_output);
  if (!depth_output.empty()) {
    if (!gis::detail::write_storm_surge_depth_geotiff(depth_output, result,
                                                      frames_dir)) {
      set_operation_result(
          "{\"error\":\"depth_write_failed\",\"op\":\"stormsurge.run\"}");
      return false;
    }
    g_last_depth_output = depth_output;
  }

  // Optional P2 stats in the same run when stats_output is set.
  std::string stats_output;
  json_get_string(args, "stats_output", &stats_output);
  if (!stats_output.empty()) {
    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> w(buf);
    w.StartObject();
    w.Key("mask");
    w.String(output.c_str());
    if (!g_last_depth_output.empty()) {
      w.Key("depth");
      w.String(g_last_depth_output.c_str());
    }
    std::string impact;
    json_get_string(args, "impact", &impact);
    if (!impact.empty()) {
      w.Key("impact");
      w.String(impact.c_str());
    }
    double buffer_distance = 0;
    if (json_get_double(args, "buffer_distance", &buffer_distance) &&
        buffer_distance > 0) {
      w.Key("buffer_distance");
      w.Double(buffer_distance);
    }
    w.Key("output");
    w.String(stats_output.c_str());
    std::string class_mask_output;
    if (json_get_string(args, "class_mask_output", &class_mask_output) &&
        !class_mask_output.empty()) {
      w.Key("class_mask_output");
      w.String(class_mask_output.c_str());
    }
    w.EndObject();
    if (!gis::detail::run_storm_surge_stats_op(buf.GetString())) {
      set_operation_result(
          "{\"error\":\"stats_failed\",\"op\":\"stormsurge.run\"}");
      return false;
    }
  }

  const double water_level = result_water_level(result);
  if (g_mask_writer) {
    const int frame_count =
        result.frame_masks.empty()
            ? 1
            : static_cast<int>(result.frame_masks.size());
    for (int i = 0; i < frame_count; ++i) {
      const unsigned char* mask =
          result.frame_masks.empty()
              ? result.mask.data()
              : result.frame_masks[static_cast<size_t>(i)].data();
      const double frame_level =
          (i < static_cast<int>(result.surge_levels.size()))
              ? result.surge_levels[static_cast<size_t>(i)]
              : water_level;
      if (!g_mask_writer(mask, result.width, result.height, result.geotransform,
                         i, frame_count, frame_level)) {
        set_operation_result(
            "{\"error\":\"no_stormsurge_seam\",\"op\":\"stormsurge.run\"}");
        return false;
      }
    }
  }

  const int frame_count =
      result.frame_masks.empty()
          ? 1
          : static_cast<int>(result.frame_masks.size());
  if (g_water_mesh_writer) {
    int pushed = 0;
    for (int i = 0; i < frame_count; ++i) {
      const gis::detail::StormSurgeWaterMesh mesh =
          gis::detail::build_storm_surge_water_mesh(result, i);
      const int point_count = static_cast<int>(mesh.xyz.size() / 3);
      const int triangle_count = static_cast<int>(mesh.indices.size() / 3);
      if (point_count < 3 || triangle_count < 1) {
        // Empty wet mask on an early tide frame is OK; skip store/push.
        continue;
      }
      if (!g_water_mesh_writer(mesh.xyz.data(), point_count, mesh.indices.data(),
                               triangle_count, i, frame_count)) {
        set_operation_result(
            "{\"error\":\"no_stormsurge_water_mesh\",\"op\":\"stormsurge.run\"}");
        return false;
      }
      ++pushed;
    }
    if (pushed == 0) {
      set_operation_result(
          "{\"error\":\"empty_water_mesh\",\"op\":\"stormsurge.run\"}");
      return false;
    }
  }

  set_operation_result(
      std::string("{\"ok\":true,\"op\":\"stormsurge.run\",\"width\":") +
      std::to_string(result.width) + ",\"height\":" +
      std::to_string(result.height) +
      ",\"kernel\":\"native.storm_surge\"" +
      (g_mask_writer ? "" : ",\"viz\":\"file_only\"") +
      (g_water_mesh_writer ? ",\"water_mesh\":true" : "") + "}");
  return true;
}

bool stormsurge_export(content::PluginHost*, std::string_view args_json) {
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
        "{\"error\":\"no_output\",\"op\":\"stormsurge.export\"}");
    return false;
  }
  if (!g_last_output.empty() && dest != g_last_output) {
    FILE* in = nullptr;
    FILE* out = nullptr;
    if (fopen_s(&in, g_last_output.c_str(), "rb") != 0 || !in) {
      set_operation_result(
          "{\"error\":\"missing_source\",\"op\":\"stormsurge.export\"}");
      return false;
    }
    if (fopen_s(&out, dest.c_str(), "wb") != 0 || !out) {
      fclose(in);
      set_operation_result(
          "{\"error\":\"write_failed\",\"op\":\"stormsurge.export\"}");
      return false;
    }
    char buf[4096];
    size_t n = 0;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
      if (fwrite(buf, 1, n, out) != n) {
        fclose(in);
        fclose(out);
        set_operation_result(
            "{\"error\":\"write_failed\",\"op\":\"stormsurge.export\"}");
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
          "{\"error\":\"missing_output\",\"op\":\"stormsurge.export\"}");
      return false;
    }
    fclose(f);
  }
  set_operation_result(
      std::string("{\"ok\":true,\"op\":\"stormsurge.export\",\"output\":\"") +
      dest + "\"}");
  return true;
}

bool stormsurge_load_coast(content::PluginHost*, std::string_view args_json) {
  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"stormsurge.load_coast\"}");
    return false;
  }
  std::string coast;
  if (!json_get_string(args, "coast", &coast) || coast.empty()) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"stormsurge.load_coast\"}");
    return false;
  }
  std::error_code ec;
  if (!std::filesystem::exists(coast, ec)) {
    set_operation_result(
        "{\"error\":\"missing_coast\",\"op\":\"stormsurge.load_coast\"}");
    return false;
  }
  g_coast_path = coast;
  set_operation_result(
      std::string("{\"ok\":true,\"op\":\"stormsurge.load_coast\",\"coast\":\"") +
      coast + "\"}");
  return true;
}

bool stormsurge_stats(content::PluginHost*, std::string_view args_json) {
  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"stormsurge.stats\"}");
    return false;
  }
  std::string mask;
  json_get_string(args, "mask", &mask);
  if (mask.empty()) {
    mask = g_last_output;
  }
  std::string output;
  json_get_string(args, "output", &output);
  if (output.empty()) {
    output = "stormsurge_stats.json";
  }
  if (mask.empty()) {
    set_operation_result(
        "{\"error\":\"no_mask\",\"op\":\"stormsurge.stats\"}");
    return false;
  }

  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.StartObject();
  w.Key("mask");
  w.String(mask.c_str());
  std::string depth;
  json_get_string(args, "depth", &depth);
  if (depth.empty()) {
    depth = g_last_depth_output;
  }
  if (!depth.empty()) {
    w.Key("depth");
    w.String(depth.c_str());
  }
  std::string impact;
  json_get_string(args, "impact", &impact);
  if (!impact.empty()) {
    w.Key("impact");
    w.String(impact.c_str());
  }
  double buffer_distance = 0;
  if (json_get_double(args, "buffer_distance", &buffer_distance)) {
    w.Key("buffer_distance");
    w.Double(buffer_distance);
  }
  w.Key("output");
  w.String(output.c_str());
  std::string class_mask_output;
  if (json_get_string(args, "class_mask_output", &class_mask_output) &&
      !class_mask_output.empty()) {
    w.Key("class_mask_output");
    w.String(class_mask_output.c_str());
  }
  std::string buffer_output;
  if (json_get_string(args, "buffer_output", &buffer_output) &&
      !buffer_output.empty()) {
    w.Key("buffer_output");
    w.String(buffer_output.c_str());
  }
  std::string overlap_output;
  if (json_get_string(args, "overlap_output", &overlap_output) &&
      !overlap_output.empty()) {
    w.Key("overlap_output");
    w.String(overlap_output.c_str());
  }
  w.EndObject();

  if (!gis::detail::run_storm_surge_stats_op(buf.GetString())) {
    set_operation_result(
        "{\"error\":\"stats_failed\",\"op\":\"stormsurge.stats\"}");
    return false;
  }
  set_operation_result(
      std::string("{\"ok\":true,\"op\":\"stormsurge.stats\",\"kernel\":"
                  "\"native.storm_surge_stats\",\"output\":\"") +
      output + "\"}");
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

void set_stormsurge_mask_writer(StormSurgeMaskWriter writer) {
  g_mask_writer = std::move(writer);
}

void set_stormsurge_water_mesh_writer(StormSurgeWaterMeshWriter writer) {
  g_water_mesh_writer = std::move(writer);
}

bool register_stormsurge(content::PluginHost* host) {
  if (!host) {
    return false;
  }
  if (!host->contribute_command(
          kPluginId, "stormsurge.run", "风暴潮淹没分析", "tools",
          [host](const tool::CommandArgs&) {
            return host->open_dialog("stormsurge.run");
          })) {
    return false;
  }
  if (!host->contribute_command(
          kPluginId, "stormsurge.load_coast", "加载岸线", "tools",
          [host](const tool::CommandArgs&) {
            return host->run_processing("stormsurge.load_coast", "{}");
          })) {
    return false;
  }
  if (!host->contribute_command(
          kPluginId, "stormsurge.about", "关于", "tools",
          [host](const tool::CommandArgs&) {
            return host->open_dialog("stormsurge.about");
          })) {
    return false;
  }
  if (!host->contribute_command(
          kPluginId, "stormsurge.stats", "淹没统计报告", "tools",
          [host](const tool::CommandArgs&) {
            return host->run_processing("stormsurge.stats", "{}");
          })) {
    return false;
  }
  if (!host->contribute_dialog(
          kPluginId, {"stormsurge.run", "风暴潮淹没分析"},
          [host](content::PluginHost*) {
            show_dialog(L"风暴潮淹没分析",
                        std::make_unique<StormSurgeRunDialog>(host));
          })) {
    return false;
  }
  if (!host->contribute_dialog(
          kPluginId, {"stormsurge.about", "关于"},
          [](content::PluginHost*) {
            show_dialog(L"关于",
                        std::make_unique<AboutDialog>(
                            "StormSurge\nDEM+coast+tide inundation "
                            "(native.storm_surge)\n"
                            "P2 stats: native.storm_surge_stats / "
                            "stormsurge.stats"));
          })) {
    return false;
  }
  return host->contribute_processing(
             kPluginId, {"stormsurge.run", "Storm surge inundation"},
             stormsurge_run) &&
         host->contribute_processing(
             kPluginId, {"stormsurge.export", "Export storm-surge mask"},
             stormsurge_export) &&
         host->contribute_processing(
             kPluginId, {"stormsurge.load_coast", "Load coast polyline"},
             stormsurge_load_coast) &&
         host->contribute_processing(
             kPluginId, {"stormsurge.stats", "Storm-surge disaster stats"},
             stormsurge_stats) &&
         host->contribute_command(
             kPluginId, "stormsurge.export", "导出淹没掩膜", "tools",
             [host](const tool::CommandArgs&) {
               return host->run_processing("stormsurge.export", "{}");
             });
}

}  // namespace plugin
