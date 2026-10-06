// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/report/commands.h"

#include <string>
#include <string_view>

#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/capability.h"
#include "plugin/runtime/host/processing/operation_result.h"
#include "tool/command/command.h"

#include <rapidjson/document.h>

namespace plugin {
namespace {

constexpr const char* kPluginId = "smartgis.report";

bool json_get_string(std::string_view json, const char* key, std::string* out) {
  if (!out || !key || json.empty()) {
    return false;
  }
  rapidjson::Document doc;
  doc.Parse(json.data(), static_cast<rapidjson::SizeType>(json.size()));
  if (doc.HasParseError() || !doc.IsObject()) {
    return false;
  }
  const auto it = doc.FindMember(key);
  if (it == doc.MemberEnd() || !it->value.IsString()) {
    return false;
  }
  *out = std::string(it->value.GetString(), it->value.GetStringLength());
  return !out->empty();
}

bool process_open(content::PluginHost* host, std::string_view args_json) {
  std::string path;
  if (!json_get_string(args_json, "path", &path) &&
      !json_get_string(args_json, "dir", &path)) {
    if (!args_json.empty() && args_json.front() != '{') {
      path.assign(args_json);
    }
  }
  if (path.empty()) {
    set_operation_result("{\"error\":\"bad_args\"}");
    return false;
  }
  ReportBridge* report = report_bridge(host);
  if (!report || !report->open(path)) {
    set_operation_result("{\"error\":\"report_open_failed\"}");
    return false;
  }
  set_operation_result("{\"ok\":true,\"op\":\"report.open\"}");
  return true;
}

bool process_post(content::PluginHost* host, std::string_view args_json) {
  std::string json;
  if (!json_get_string(args_json, "json", &json)) {
    json.assign(args_json);
  }
  if (json.empty()) {
    set_operation_result("{\"error\":\"bad_args\"}");
    return false;
  }
  ReportBridge* report = report_bridge(host);
  if (!report || !report->post(json)) {
    set_operation_result("{\"error\":\"report_post_failed\"}");
    return false;
  }
  set_operation_result("{\"ok\":true,\"op\":\"report.post\"}");
  return true;
}

bool process_scenario(content::PluginHost* host, std::string_view args_json) {
  if (!process_open(host, args_json)) {
    return false;
  }
  constexpr const char* kSeries =
      "{\"series\":[{\"label\":\"X\",\"value\":22},{\"label\":\"Y\",\"value\":41},"
      "{\"label\":\"Z\",\"value\":15}]}";
  return process_post(host, kSeries);
}

}  // namespace

bool register_report(content::PluginHost* host) {
  if (!host) {
    return false;
  }
  tool::CommandCatalog* catalog = host->commands();
  if (catalog && catalog->contains("report.scenario.showcase")) {
    return true;
  }
  const bool have_open = catalog && catalog->contains("report.open");
  if (!have_open) {
    if (!host->contribute_command(
            kPluginId, "report.open", "打开报告", "tools",
            [host](const tool::CommandArgs& args) {
              return process_open(host, args.payload);
            })) {
      return false;
    }
    if (!host->contribute_command(
            kPluginId, "report.post", "推送报告数据", "tools",
            [host](const tool::CommandArgs& args) {
              return process_post(host, args.payload);
            })) {
      return false;
    }
  }
  if (!host->contribute_command(
          kPluginId, "report.scenario.showcase", "Harness report scenario",
          "tools",
          [host](const tool::CommandArgs& args) {
            return process_scenario(host, args.payload);
          })) {
    return false;
  }
  if (!have_open) {
    if (!host->contribute_processing(
            kPluginId, {"report.open", "Open local HTML report directory"},
            process_open) ||
        !host->contribute_processing(
            kPluginId, {"report.post", "Post JSON to the report dock"},
            process_post)) {
      return false;
    }
  }
  return host->contribute_processing(
      kPluginId,
      {"report.scenario.showcase", "Open sample report and post series"},
      process_scenario);
}

}  // namespace plugin
