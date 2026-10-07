// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/report/commands.h"

#include <string>
#include <string_view>

#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/capability.h"
#include "plugin/runtime/host/capability/pack_ensure.h"
#include "plugin/runtime/host/capability/scenario.h"
#include "plugin/runtime/host/processing/args_json.h"
#include "plugin/runtime/host/processing/operation_result.h"
#include "tool/command/command.h"

#include <rapidjson/document.h>

namespace plugin {
namespace {

constexpr const char* kPluginId = "smartgis.report";

struct ReportPackOnce {
  ReportPackOnce() {
    register_command_pack("report", [](content::PluginHost* host) {
      return register_report(host);
    });
  }
} k_report_pack_once;

bool process_open(content::PluginHost* host, std::string_view args_json) {
  std::string path;
  rapidjson::Document args;
  if (parse_args_json(args_json, &args)) {
    if (!args_json_string(args, "path", &path)) {
      (void)args_json_string(args, "dir", &path);
    }
  } else if (!args_json.empty() && args_json.front() != '{') {
    path.assign(args_json);
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
  rapidjson::Document args;
  if (parse_args_json(args_json, &args)) {
    (void)args_json_string(args, "json", &json);
  }
  if (json.empty()) {
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
            const bool ok = process_scenario(host, args.payload);
            set_harness_scenario_exit(ok ? 0 : 1);
            return ok;
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
