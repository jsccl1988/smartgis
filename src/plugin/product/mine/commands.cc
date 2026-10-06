// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/mine/commands.h"

#include <memory>
#include <string>
#include <string_view>

#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/capability.h"
#include "gis/analysis/geology/borehole.h"
#include "gis/analysis/geology/prism_volume.h"
#include "gis/analysis/geology/stratum_tin.h"
#include "plugin/product/mine/present/stratum.h"
#include "plugin/product/mine/views/interpolate_dialog.h"
#include "plugin/runtime/host/processing/operation_result.h"
#include "plugin/runtime/widgets/about_dialog.h"
#include "plugin/runtime/widgets/owned_dialog.h"
#include "plugin/runtime/widgets/present_surface_picker.h"
#include "tool/command/command.h"

#include <rapidjson/document.h>

namespace plugin {
namespace {

constexpr const char* kPluginId = "smartgis.mine";

gis::detail::BoreholeSet g_last_holes;
gis::detail::StratumTin g_last_tin;
std::string g_last_csv;

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

bool publish_viz(content::PluginHost* host,
                 const gis::detail::StratumTin& tin,
                 const gis::detail::BoreholeSet& holes,
                 const char* op) {
  g_last_tin = tin;
  g_last_holes = holes;
  if (!host) {
    return true;
  }
  if (!host->gis_document()) {
    set_operation_result(
        std::string("{\"error\":\"no_mine_seam\",\"op\":\"") + op + "\"}");
    return false;
  }
  std::string err;
  if (!present_mine_stratum(host->gis_document(), plugin::scene3d_sink(host),
                            nullptr, tin, holes, &err)) {
    set_operation_result(
        std::string("{\"error\":\"") +
        (err.empty() ? "no_mine_seam" : err) + "\",\"op\":\"" + op + "\"}");
    return false;
  }
  (void)host->present_dataset(kPluginId, "", 1);
  return true;
}

bool mine_load_boreholes(content::PluginHost* host, std::string_view args_json) {
  if (host) {
    gis::detail::StratumTin empty_tin;
    return publish_viz(host, g_last_tin.ok ? g_last_tin : empty_tin, g_last_holes,
                       "mine.load_boreholes");
  }
  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"mine.load_boreholes\"}");
    return false;
  }
  std::string input;
  if (!json_get_string(args, "input", &input) || input.empty()) {
    // Accept "path" alias for harness convenience.
    if (!json_get_string(args, "path", &input) || input.empty()) {
      set_operation_result(
          "{\"error\":\"bad_args\",\"op\":\"mine.load_boreholes\"}");
      return false;
    }
  }
  gis::detail::BoreholeSet holes = gis::detail::load_boreholes_csv(input);
  if (!holes.ok) {
    set_operation_result(
        std::string("{\"error\":\"") +
        (holes.error.empty() ? "load_failed" : holes.error) +
        "\",\"op\":\"mine.load_boreholes\"}");
    return false;
  }
  g_last_holes = holes;
  g_last_csv = input;

  gis::detail::StratumTin empty_tin;
  if (!publish_viz(host, empty_tin, holes, "mine.load_boreholes")) {
    return false;
  }

  set_operation_result(
      std::string("{\"ok\":true,\"op\":\"mine.load_boreholes\",\"contacts\":") +
      std::to_string(holes.contacts.size()) + "}");
  return true;
}

bool mine_interpolate_stratum(content::PluginHost* host,
                              std::string_view args_json) {
  if (host) {
    return publish_viz(host, g_last_tin, g_last_holes,
                       "mine.interpolate_stratum");
  }
  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"mine.interpolate_stratum\"}");
    return false;
  }
  std::string input;
  std::string stratum_id;
  if (!json_get_string(args, "input", &input) || input.empty() ||
      !json_get_string(args, "stratum_id", &stratum_id) || stratum_id.empty()) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"mine.interpolate_stratum\"}");
    return false;
  }
  std::string output;
  json_get_string(args, "output", &output);

  gis::detail::BoreholeSet holes = gis::detail::load_boreholes_csv(input);
  if (!holes.ok) {
    set_operation_result(
        std::string("{\"error\":\"") +
        (holes.error.empty() ? "load_failed" : holes.error) +
        "\",\"op\":\"mine.interpolate_stratum\"}");
    return false;
  }
  g_last_holes = holes;
  g_last_csv = input;

  gis::detail::StratumTin tin =
      gis::detail::interpolate_stratum_tin(holes, stratum_id);
  if (!tin.ok) {
    set_operation_result(
        std::string("{\"error\":\"") +
        (tin.error.empty() ? "interpolate_failed" : tin.error) +
        "\",\"op\":\"mine.interpolate_stratum\"}");
    return false;
  }

  if (!output.empty()) {
    // Optional mesh JSON via kernel op (same args shape).
    if (!gis::detail::run_stratum_interpolate_op(args_json)) {
      set_operation_result(
          "{\"error\":\"write_failed\",\"op\":\"mine.interpolate_stratum\"}");
      return false;
    }
  }

  if (!publish_viz(host, tin, holes, "mine.interpolate_stratum")) {
    return false;
  }

  const int tri_count = static_cast<int>(tin.indices.size() / 3);
  set_operation_result(
      std::string("{\"ok\":true,\"op\":\"mine.interpolate_stratum\",\"stratum\":\"") +
      tin.stratum_id + "\",\"triangles\":" + std::to_string(tri_count) +
      ",\"contacts\":" + std::to_string(holes.contacts.size()) + "}");
  return true;
}

bool mine_prism_volume(content::PluginHost*, std::string_view args_json) {
  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"mine.prism_volume\"}");
    return false;
  }
  std::string input;
  std::string top_id;
  std::string bottom_id;
  if (!json_get_string(args, "input", &input) || input.empty() ||
      !json_get_string(args, "top_stratum_id", &top_id) || top_id.empty() ||
      !json_get_string(args, "bottom_stratum_id", &bottom_id) ||
      bottom_id.empty()) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"mine.prism_volume\"}");
    return false;
  }
  std::string output;
  json_get_string(args, "output", &output);

  gis::detail::BoreholeSet holes = gis::detail::load_boreholes_csv(input);
  if (!holes.ok) {
    set_operation_result(
        std::string("{\"error\":\"") +
        (holes.error.empty() ? "load_failed" : holes.error) +
        "\",\"op\":\"mine.prism_volume\"}");
    return false;
  }
  g_last_holes = holes;
  g_last_csv = input;

  const gis::detail::PrismVolumeResult vol =
      gis::detail::prism_volume_between(holes, top_id, bottom_id);
  if (!vol.ok) {
    set_operation_result(
        std::string("{\"error\":\"") +
        (vol.error.empty() ? "volume_failed" : vol.error) +
        "\",\"op\":\"mine.prism_volume\"}");
    return false;
  }

  if (!output.empty() &&
      !gis::detail::run_stratum_prism_volume_op(args_json)) {
    set_operation_result(
        "{\"error\":\"write_failed\",\"op\":\"mine.prism_volume\"}");
    return false;
  }

  // Volume only. Lithology overlay is committed by interpolate_stratum;
  // rebuilding the studio mesh here stalls FlyCube present (harness rc 124).

  set_operation_result(
      std::string("{\"ok\":true,\"op\":\"mine.prism_volume\",\"volume\":") +
      std::to_string(vol.volume) + ",\"stratum\":\"" + vol.stratum_id + "\"}");
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

bool register_mine(content::PluginHost* host) {
  if (!host) {
    return false;
  }
  if (!host->contribute_command(
          kPluginId, "mine.interpolate_stratum", "地层TIN插值", "tools",
          [host](const tool::CommandArgs&) {
            return host->open_dialog("mine.interpolate_stratum");
          })) {
    return false;
  }
  if (!host->contribute_command(
          kPluginId, "mine.load_boreholes", "导入钻孔CSV", "tools",
          [host](const tool::CommandArgs&) {
            return host->open_dialog("mine.interpolate_stratum");
          })) {
    return false;
  }
  if (!host->contribute_command(
          kPluginId, "mine.prism_volume", "棱柱体积估算", "tools",
          [host](const tool::CommandArgs&) {
            return host->open_dialog("mine.interpolate_stratum");
          })) {
    return false;
  }
  if (!host->contribute_command(
          kPluginId, "mine.about", "关于", "tools",
          [host](const tool::CommandArgs&) {
            return host->open_dialog("mine.about");
          })) {
    return false;
  }
  if (!host->contribute_dialog(
          kPluginId, {"mine.interpolate_stratum", "地层TIN插值"},
          [host](content::PluginHost*) {
            show_dialog(L"地层TIN插值",
                        wrap_with_present_surface(
                            host, std::make_unique<InterpolateDialog>(host)));
          })) {
    return false;
  }
  if (!host->contribute_dialog(
          kPluginId, {"mine.about", "关于"},
          [](content::PluginHost*) {
            show_dialog(L"关于",
                        std::make_unique<AboutDialog>(
                            "Mine\nBorehole CSV → stratum TIN + prism volume"));
          })) {
    return false;
  }
  return host->contribute_processing(
             kPluginId, {"mine.load_boreholes", "Load borehole CSV"},
             mine_load_boreholes) &&
         host->contribute_processing(
             kPluginId,
             {"mine.interpolate_stratum", "Interpolate stratum TIN"},
             mine_interpolate_stratum) &&
         host->contribute_processing(
             kPluginId, {"mine.prism_volume", "Prism volume between strata"},
             mine_prism_volume) &&
         host->contribute_export_frame(
             kPluginId, {"mine_boreholes", 116.34, 39.87, 116.41, 39.93});
}

}  // namespace plugin
