// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/traffic/commands.h"

#include <memory>
#include <string>
#include <string_view>

#include "content/public/plugin_host.h"
#include "gis/analysis/network/cost_path.h"
#include "plugin/product/traffic/views/shortest_path_dialog.h"
#include "plugin/product/traffic/present/present.h"
#include "plugin/runtime/host/processing/args_json.h"
#include "plugin/runtime/host/processing/operation_result.h"
#include "plugin/runtime/host/processing/reexport_file.h"
#include "plugin/runtime/widgets/about_dialog.h"
#include "plugin/runtime/widgets/owned_dialog.h"
#include "plugin/runtime/widgets/present_surface_picker.h"
#include "tool/command/command.h"

#include <rapidjson/document.h>

namespace plugin {
namespace {

constexpr const char* kPluginId = "smartgis.traffic";

std::string g_last_output;
std::string g_last_network;
gis::detail::CostPathResult g_last_path;
int g_last_frames = 24;

int traffic_prefix_points(int point_count, int frames, int frame_index) {
  if (point_count < 2 || frames <= 0) {
    return 0;
  }
  const int f = frame_index + 1;
  const int prefix = (point_count * f + frames - 1) / frames;
  return prefix < 2 ? 2 : prefix;
}


bool traffic_cost_path(content::PluginHost* host, std::string_view args_json) {
  if (host) {
    if (!g_last_path.ok) {
      set_operation_result(
          "{\"error\":\"path_failed\",\"op\":\"traffic.cost_path\"}");
      return false;
    }
    const int point_count = static_cast<int>(g_last_path.xy.size() / 2);
    content::GisDocument* gis = host->gis_document();
    if (!gis) {
      set_operation_result(
          "{\"error\":\"no_path_seam\",\"op\":\"traffic.cost_path\"}");
      return false;
    }
    if (!present_traffic_path(gis, g_last_path.xy.data(), point_count,
                              g_last_path.total_cost, g_last_network.c_str(),
                              point_count)) {
      set_operation_result(
          "{\"error\":\"no_path_seam\",\"op\":\"traffic.cost_path\"}");
      return false;
    }
    if (content::PluginHost::Playback* pb = host->playback()) {
      pb->clear();
      const int frames = g_last_frames > 0 ? g_last_frames : 1;
      for (int i = 0; i < frames; ++i) {
        pb->push_frame("{\"index\":" + std::to_string(i) + "}");
      }
      pb->set_index(static_cast<size_t>(frames - 1));
    }
    (void)host->present_dataset(kPluginId, "", 0);
    set_operation_result(
        std::string("{\"ok\":true,\"op\":\"traffic.cost_path\",\"points\":") +
        std::to_string(point_count) + ",\"cost\":" +
        std::to_string(g_last_path.total_cost) + "}");
    return true;
  }
  rapidjson::Document args;
  if (!parse_args_json(args_json, &args)) {
    set_operation_result("{\"error\":\"bad_args\",\"op\":\"traffic.cost_path\"}");
    return false;
  }
  std::string network;
  std::string output;
  if (!args_json_string(args, "network", &network) || network.empty() ||
      !args_json_string(args, "output", &output) || output.empty()) {
    set_operation_result("{\"error\":\"bad_args\",\"op\":\"traffic.cost_path\"}");
    return false;
  }
  double start_x = 0;
  double start_y = 0;
  double end_x = 0;
  double end_y = 0;
  if (!args_json_double(args, "start_x", &start_x) ||
      !args_json_double(args, "start_y", &start_y) ||
      !args_json_double(args, "end_x", &end_x) ||
      !args_json_double(args, "end_y", &end_y)) {
    set_operation_result("{\"error\":\"bad_args\",\"op\":\"traffic.cost_path\"}");
    return false;
  }
  std::string weight_field;
  args_json_string(args, "weight_field", &weight_field);
  int frames = 24;
  args_json_int(args, "frames", &frames);

  const gis::detail::CostPathResult path = gis::detail::run_cost_path(
      network, start_x, start_y, end_x, end_y, weight_field);
  if (!path.ok) {
    set_operation_result(
        std::string("{\"error\":\"") +
        (path.error.empty() ? "path_failed" : path.error) +
        "\",\"op\":\"traffic.cost_path\"}");
    return false;
  }
  if (!gis::detail::write_path_geojson(output, path)) {
    set_operation_result(
        "{\"error\":\"write_failed\",\"op\":\"traffic.cost_path\"}");
    return false;
  }
  g_last_output = output;
  g_last_network = network;
  g_last_frames = frames;
  g_last_path = path;
  const int point_count = static_cast<int>(path.xy.size() / 2);
  set_operation_result(
      std::string("{\"ok\":true,\"op\":\"traffic.cost_path\",\"points\":") +
      std::to_string(point_count) + ",\"cost\":" +
      std::to_string(path.total_cost) + "}");
  return true;
}

bool traffic_export_path(content::PluginHost*, std::string_view args_json) {
  return reexport_cached_file(&g_last_output, args_json, "traffic.export_path");
}

bool traffic_present_frame(content::PluginHost* host,
                           std::string_view args_json) {
  if (!host || !g_last_path.ok) {
    set_operation_result(
        "{\"error\":\"no_traffic_session\",\"op\":\"traffic.present_frame\"}");
    return false;
  }
  content::GisDocument* gis = host->gis_document();
  if (!gis) {
    set_operation_result(
        "{\"error\":\"no_path_seam\",\"op\":\"traffic.present_frame\"}");
    return false;
  }
  int index = 0;
  rapidjson::Document args;
  if (parse_args_json(args_json, &args)) {
    args_json_int(args, "index", &index);
  }
  const int point_count = static_cast<int>(g_last_path.xy.size() / 2);
  const int frames = g_last_frames > 0 ? g_last_frames : 1;
  if (index < 0) {
    index = 0;
  }
  if (index >= frames) {
    index = frames - 1;
  }
  const int prefix = traffic_prefix_points(point_count, frames, index);
  if (!present_traffic_path(gis, g_last_path.xy.data(), point_count,
                            g_last_path.total_cost, g_last_network.c_str(),
                            prefix)) {
    set_operation_result(
        "{\"error\":\"present_failed\",\"op\":\"traffic.present_frame\"}");
    return false;
  }
  if (content::PluginHost::Playback* pb = host->playback()) {
    pb->set_index(static_cast<size_t>(index));
  }
  set_operation_result("{\"ok\":true,\"op\":\"traffic.present_frame\"}");
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

bool register_traffic(content::PluginHost* host) {
  if (!host) {
    return false;
  }
  if (!host->contribute_command(
          kPluginId, "traffic.shortest_path", "最佳路径分析", "tools",
          [host](const tool::CommandArgs&) {
            return host->open_dialog("traffic.shortest_path");
          })) {
    return false;
  }
  if (!host->contribute_command(
          kPluginId, "traffic.about", "关于", "tools",
          [host](const tool::CommandArgs&) {
            return host->open_dialog("traffic.about");
          })) {
    return false;
  }
  if (!host->contribute_dialog(
          kPluginId, {"traffic.shortest_path", "最佳路径分析"},
          [host](content::PluginHost*) {
            show_dialog(L"最佳路径分析",
                        wrap_with_present_surface(
                            host, std::make_unique<ShortestPathDialog>(host)));
          })) {
    return false;
  }
  if (!host->contribute_dialog(
          kPluginId, {"traffic.about", "关于"},
          [](content::PluginHost*) {
            show_dialog(L"关于",
                        std::make_unique<AboutDialog>(
                            "Traffic\nUrban least-cost path (native.cost_path)"));
          })) {
    return false;
  }
  return host->contribute_processing(
             kPluginId, {"traffic.cost_path", "Traffic cost path"},
             traffic_cost_path) &&
         host->contribute_processing(
             kPluginId, {"traffic.export_path", "Export traffic path GeoJSON"},
             traffic_export_path) &&
         host->contribute_processing(
             kPluginId, {"traffic.present_frame", "Re-present traffic frame"},
             traffic_present_frame) &&
         host->contribute_export_frame(
             kPluginId, {"traffic_beijing", 116.34, 39.885, 116.46, 39.930}) &&
         host->contribute_command(
             kPluginId, "traffic.export_path", "导出路径", "tools",
             [host](const tool::CommandArgs&) {
               return host->run_processing("traffic.export_path", "{}");
             });
}

}  // namespace plugin
