// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/report/commands.h"

#include <string>
#include <string_view>

#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/capability.h"
#include "plugin/runtime/host/capability/pack_ensure.h"
#include "plugin/runtime/host/processing/args_json.h"
#include "plugin/runtime/host/processing/operation_result.h"
#include "tool/command/command.h"

#include <rapidjson/document.h>

namespace plugin {
namespace {

constexpr const char* kPluginId = "smartgis.report";

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

}  // namespace

bool register_report(content::PluginHost* host) {
  if (!host) {
    return false;
  }
  // contribute_command is idempotent on handlers_. Do not probe
  // CommandCatalog first: a smashed id string AVs in contains()
  // (_Large_mode_engaged) before the report pack can install.
  auto contribute = [&](const char* id, const char* title,
                        tool::CommandHandler handler) {
    return host->contribute_command(kPluginId, id, title, "tools",
                                    std::move(handler));
  };
  if (!contribute("report.open", "打开报告",
                  [host](const tool::CommandArgs& args) {
                    return process_open(host, args.payload);
                  }) ||
      !contribute("report.post", "推送报告数据",
                  [host](const tool::CommandArgs& args) {
                    return process_post(host, args.payload);
                  })) {
    return false;
  }
  return host->contribute_processing(
             kPluginId, {"report.open", "Open local HTML report directory"},
             process_open) &&
         host->contribute_processing(
             kPluginId, {"report.post", "Post JSON to the report dock"},
             process_post);
}

namespace {

struct ReportPackOnce {
  ReportPackOnce() { register_command_pack("report", register_report); }
} k_report_pack_once;

}  // namespace

}  // namespace plugin
