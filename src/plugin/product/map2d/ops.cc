// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/map2d/ops.h"

#include <cstdint>
#include <string>
#include <string_view>

#include <rapidjson/document.h>

#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/capability.h"
#include "plugin/runtime/host/capability/contribute.h"
#include "plugin/runtime/host/processing/args_json.h"
#include "plugin/runtime/host/processing/operation_result.h"
#include "tool/command/command.h"

namespace plugin {
namespace {

constexpr const char* kPluginId = "smartgis.map2d";

enum class SinkNeed { kAny, kView, kLook, kPresent };

bool fail_no_map(const char* op) {
  set_operation_result(std::string("{\"error\":\"no_map_device\",\"op\":\"") +
                       op + "\"}");
  return false;
}

bool fail_bad_args(const char* op, const char* need = nullptr) {
  if (need && need[0]) {
    set_operation_result(std::string("{\"error\":\"bad_args\",\"op\":\"") + op +
                         "\",\"need\":\"" + need + "\"}");
  } else {
    set_operation_result(std::string("{\"error\":\"bad_args\",\"op\":\"") + op +
                         "\"}");
  }
  return false;
}

bool finish_result(bool ok, const std::string& result, const char* op,
                   const char* fail_code) {
  if (!ok) {
    if (result.empty()) {
      set_operation_result(std::string("{\"error\":\"") + fail_code +
                           "\",\"op\":\"" + op + "\"}");
    } else {
      set_operation_result(result);
    }
    return false;
  }
  if (result.empty()) {
    set_operation_result(std::string("{\"ok\":true,\"op\":\"") + op + "\"}");
  } else {
    set_operation_result(result);
  }
  return true;
}

Map2dSink* require_sink(content::PluginHost* host, const char* op,
                        SinkNeed need) {
  Map2dSink* sink = map2d_sink(host);
  if (!sink) {
    fail_no_map(op);
    return nullptr;
  }
  switch (need) {
    case SinkNeed::kView:
      if (!sink->view_bridges_installed()) {
        fail_no_map(op);
        return nullptr;
      }
      break;
    case SinkNeed::kLook:
      if (!sink->look_bridges_installed()) {
        fail_no_map(op);
        return nullptr;
      }
      break;
    case SinkNeed::kPresent:
      if (!sink->present_bridges_installed()) {
        fail_no_map(op);
        return nullptr;
      }
      break;
    case SinkNeed::kAny:
      break;
  }
  return sink;
}

bool process_open_map(content::PluginHost* host, std::string_view) {
  Map2dSink* sink = require_sink(host, "map2d.open_map", SinkNeed::kView);
  if (!sink) {
    return false;
  }
  if (!sink->open_map()) {
    set_operation_result("{\"error\":\"open_failed\",\"op\":\"map2d.open_map\"}");
    return false;
  }
  set_operation_result(
      "{\"ok\":true,\"op\":\"map2d.open_map\",\"contour\":\"defaults\"}");
  return true;
}

bool process_frame_to(content::PluginHost* host, std::string_view args_json) {
  Map2dSink* sink = require_sink(host, "map2d.frame_to", SinkNeed::kView);
  if (!sink) {
    return false;
  }
  rapidjson::Document args;
  if (!parse_args_json(args_json, &args)) {
    return fail_bad_args("map2d.frame_to");
  }
  double lon = 0.0;
  double lat = 0.0;
  if (!args_json_double(args, "lon", &lon) ||
      !args_json_double(args, "lat", &lat)) {
    return fail_bad_args("map2d.frame_to", "lon,lat");
  }
  double span_deg = 4.0;
  (void)args_json_double(args, "span_deg", &span_deg);
  if (!sink->frame_to(lon, lat, span_deg)) {
    set_operation_result(
        "{\"error\":\"frame_failed\",\"op\":\"map2d.frame_to\"}");
    return false;
  }
  set_operation_result("{\"ok\":true,\"op\":\"map2d.frame_to\"}");
  return true;
}

bool process_attach_dataset(content::PluginHost* host,
                            std::string_view args_json) {
  Map2dSink* sink = require_sink(host, "map2d.attach_dataset", SinkNeed::kAny);
  if (!sink) {
    return false;
  }
  rapidjson::Document args;
  if (!parse_args_json(args_json, &args)) {
    return fail_bad_args("map2d.attach_dataset");
  }
  std::string path;
  (void)args_json_string(args, "path", &path);
  if (!sink->attach_dataset(path)) {
    set_operation_result(
        "{\"error\":\"attach_failed\",\"op\":\"map2d.attach_dataset\"}");
    return false;
  }
  set_operation_result("{\"ok\":true,\"op\":\"map2d.attach_dataset\"}");
  return true;
}

bool process_load_hillshade(content::PluginHost* host,
                            std::string_view args_json) {
  Map2dSink* sink = require_sink(host, "map2d.load_hillshade", SinkNeed::kView);
  if (!sink) {
    return false;
  }
  rapidjson::Document args;
  if (!parse_args_json(args_json, &args)) {
    return fail_bad_args("map2d.load_hillshade");
  }
  std::string path;
  (void)args_json_string(args, "path", &path);
  std::string result;
  return finish_result(sink->load_hillshade(path, &result), result,
                       "map2d.load_hillshade", "load_failed");
}

bool process_apply_look(content::PluginHost* host, std::string_view args_json) {
  Map2dSink* sink = require_sink(host, "map2d.apply_look", SinkNeed::kLook);
  if (!sink) {
    return false;
  }
  rapidjson::Document args;
  if (!parse_args_json(args_json, &args)) {
    return fail_bad_args("map2d.apply_look");
  }
  std::string mode;
  if (!args_json_string(args, "mode", &mode) || mode.empty()) {
    return fail_bad_args("map2d.apply_look", "mode");
  }
  std::string result;
  return finish_result(sink->apply_look(mode, &result), result,
                       "map2d.apply_look", "look_failed");
}

bool process_frame_fly(content::PluginHost* host, std::string_view args_json) {
  Map2dSink* sink = require_sink(host, "map2d.frame_fly", SinkNeed::kLook);
  if (!sink) {
    return false;
  }
  rapidjson::Document args;
  if (!parse_args_json(args_json, &args)) {
    return fail_bad_args("map2d.frame_fly");
  }
  double t = 0.48;
  (void)args_json_double(args, "t", &t);
  std::string result;
  return finish_result(sink->frame_fly(static_cast<float>(t), &result), result,
                       "map2d.frame_fly", "fly_failed");
}

bool process_export_bmp(content::PluginHost* host, std::string_view args_json) {
  Map2dSink* sink = require_sink(host, "map2d.export_bmp", SinkNeed::kPresent);
  if (!sink) {
    return false;
  }
  rapidjson::Document args;
  if (!parse_args_json(args_json, &args)) {
    return fail_bad_args("map2d.export_bmp");
  }
  std::string path;
  if (!args_json_string(args, "path", &path) || path.empty()) {
    return fail_bad_args("map2d.export_bmp", "path");
  }
  double w = 1280.0;
  double h = 720.0;
  (void)args_json_double(args, "width", &w);
  (void)args_json_double(args, "height", &h);
  if (!sink->export_bmp(path, static_cast<int>(w), static_cast<int>(h))) {
    set_operation_result(
        "{\"error\":\"export_failed\",\"op\":\"map2d.export_bmp\"}");
    return false;
  }
  set_operation_result("{\"ok\":true,\"op\":\"map2d.export_bmp\"}");
  return true;
}

bool process_present_gpu(content::PluginHost* host, std::string_view args_json) {
  Map2dSink* sink = require_sink(host, "map2d.present_gpu", SinkNeed::kPresent);
  if (!sink) {
    return false;
  }
  rapidjson::Document args;
  if (!parse_args_json(args_json, &args)) {
    return fail_bad_args("map2d.present_gpu");
  }
  double w = 1280.0;
  double h = 720.0;
  (void)args_json_double(args, "width", &w);
  (void)args_json_double(args, "height", &h);
  if (!sink->present_gpu(static_cast<uint32_t>(w), static_cast<uint32_t>(h))) {
    set_operation_result(
        "{\"error\":\"present_failed\",\"op\":\"map2d.present_gpu\"}");
    return false;
  }
  set_operation_result("{\"ok\":true,\"op\":\"map2d.present_gpu\"}");
  return true;
}

bool process_add_standin(content::PluginHost* host, std::string_view args_json) {
  Map2dSink* sink =
      require_sink(host, "map2d.add_standin_layer", SinkNeed::kAny);
  if (!sink) {
    return false;
  }
  rapidjson::Document args;
  if (!parse_args_json(args_json, &args)) {
    return fail_bad_args("map2d.add_standin_layer");
  }
  std::string name = "standin";
  (void)args_json_string(args, "name", &name);
  double lon = 0.0;
  double lat = 0.0;
  double half = 0.5;
  if (!args_json_double(args, "lon", &lon) ||
      !args_json_double(args, "lat", &lat)) {
    return fail_bad_args("map2d.add_standin_layer", "lon,lat");
  }
  (void)args_json_double(args, "half_deg", &half);
  if (!sink->add_standin_layer(name, lon, lat, half)) {
    set_operation_result(
        "{\"error\":\"add_failed\",\"op\":\"map2d.add_standin_layer\"}");
    return false;
  }
  set_operation_result("{\"ok\":true,\"op\":\"map2d.add_standin_layer\"}");
  return true;
}

struct SinkOp {
  const char* id;
  const char* title;
  bool (*fn)(content::PluginHost*, std::string_view);
};

bool register_sink_ops(content::PluginHost* host) {
  if (tool::CommandCatalog* catalog = host->commands()) {
    if (catalog->contains("map2d.open_map")) {
      return true;
    }
  }
  const SinkOp ops[] = {
      {"map2d.open_map", "打开二维地图", process_open_map},
      {"map2d.frame_to", "二维框选", process_frame_to},
      {"map2d.attach_dataset", "挂载二维数据", process_attach_dataset},
      {"map2d.load_hillshade", "加载二维DEM", process_load_hillshade},
      {"map2d.apply_look", "二维外观预设", process_apply_look},
      {"map2d.frame_fly", "二维飞入", process_frame_fly},
      {"map2d.export_bmp", "导出二维BMP", process_export_bmp},
      {"map2d.present_gpu", "二维GPU呈现", process_present_gpu},
      {"map2d.add_standin_layer", "二维占位多边形", process_add_standin},
  };
  for (const SinkOp& op : ops) {
    if (!contribute_op(host, kPluginId, op.id, op.title, "tools", op.fn)) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool register_map2d_sink_ops(content::PluginHost* host) {
  if (!host) {
    return false;
  }
  return register_sink_ops(host);
}

}  // namespace plugin
