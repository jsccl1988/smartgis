// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/map2d/verbs.h"

#include <cstdint>
#include <string>
#include <string_view>

#include <rapidjson/document.h>

#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/capability.h"
#include "plugin/runtime/host/processing/operation_result.h"
#include "tool/command/command.h"

namespace plugin {
namespace {

constexpr const char* kPluginId = "smartgis.map2d";

bool parse_args(std::string_view json, rapidjson::Document* out) {
  if (!out) {
    return false;
  }
  if (json.empty()) {
    out->SetObject();
    return true;
  }
  out->Parse(json.data(), static_cast<rapidjson::SizeType>(json.size()));
  return !out->HasParseError() && out->IsObject();
}

bool json_string(const rapidjson::Value& obj, const char* key, std::string* out) {
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

bool json_double(const rapidjson::Value& obj, const char* key, double* out) {
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

bool fail_no_map(const char* op) {
  set_operation_result(std::string("{\"error\":\"no_map_device\",\"op\":\"") +
                       op + "\"}");
  return false;
}

Map2dSink* sink_of(content::PluginHost* host) { return map2d_sink(host); }

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

bool process_open_map(content::PluginHost* host, std::string_view) {
  Map2dSink* sink = sink_of(host);
  if (!sink || !sink->view_bridges_installed()) {
    return fail_no_map("map2d.open_map");
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
  Map2dSink* sink = sink_of(host);
  if (!sink || !sink->view_bridges_installed()) {
    return fail_no_map("map2d.frame_to");
  }
  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    set_operation_result("{\"error\":\"bad_args\",\"op\":\"map2d.frame_to\"}");
    return false;
  }
  double lon = 0.0;
  double lat = 0.0;
  if (!json_double(args, "lon", &lon) || !json_double(args, "lat", &lat)) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"map2d.frame_to\",\"need\":\"lon,lat\"}");
    return false;
  }
  double span_deg = 4.0;
  (void)json_double(args, "span_deg", &span_deg);
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
  Map2dSink* sink = sink_of(host);
  if (!sink) {
    return fail_no_map("map2d.attach_dataset");
  }
  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"map2d.attach_dataset\"}");
    return false;
  }
  std::string path;
  (void)json_string(args, "path", &path);
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
  Map2dSink* sink = sink_of(host);
  if (!sink || !sink->view_bridges_installed()) {
    return fail_no_map("map2d.load_hillshade");
  }
  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"map2d.load_hillshade\"}");
    return false;
  }
  std::string path;
  (void)json_string(args, "path", &path);
  std::string result;
  return finish_result(sink->load_hillshade(path, &result), result,
                       "map2d.load_hillshade", "load_failed");
}

bool process_apply_look(content::PluginHost* host, std::string_view args_json) {
  Map2dSink* sink = sink_of(host);
  if (!sink || !sink->look_bridges_installed()) {
    return fail_no_map("map2d.apply_look");
  }
  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    set_operation_result("{\"error\":\"bad_args\",\"op\":\"map2d.apply_look\"}");
    return false;
  }
  std::string mode;
  if (!json_string(args, "mode", &mode) || mode.empty()) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"map2d.apply_look\",\"need\":\"mode\"}");
    return false;
  }
  std::string result;
  return finish_result(sink->apply_look(mode, &result), result,
                       "map2d.apply_look", "look_failed");
}

bool process_frame_fly(content::PluginHost* host, std::string_view args_json) {
  Map2dSink* sink = sink_of(host);
  if (!sink || !sink->look_bridges_installed()) {
    return fail_no_map("map2d.frame_fly");
  }
  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    set_operation_result("{\"error\":\"bad_args\",\"op\":\"map2d.frame_fly\"}");
    return false;
  }
  double t = 0.48;
  (void)json_double(args, "t", &t);
  std::string result;
  return finish_result(sink->frame_fly(static_cast<float>(t), &result), result,
                       "map2d.frame_fly", "fly_failed");
}

bool process_export_bmp(content::PluginHost* host, std::string_view args_json) {
  Map2dSink* sink = sink_of(host);
  if (!sink || !sink->present_bridges_installed()) {
    return fail_no_map("map2d.export_bmp");
  }
  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    set_operation_result("{\"error\":\"bad_args\",\"op\":\"map2d.export_bmp\"}");
    return false;
  }
  std::string path;
  if (!json_string(args, "path", &path) || path.empty()) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"map2d.export_bmp\",\"need\":\"path\"}");
    return false;
  }
  double w = 1280.0;
  double h = 720.0;
  (void)json_double(args, "width", &w);
  (void)json_double(args, "height", &h);
  if (!sink->export_bmp(path, static_cast<int>(w), static_cast<int>(h))) {
    set_operation_result(
        "{\"error\":\"export_failed\",\"op\":\"map2d.export_bmp\"}");
    return false;
  }
  set_operation_result("{\"ok\":true,\"op\":\"map2d.export_bmp\"}");
  return true;
}

bool process_present_gpu(content::PluginHost* host, std::string_view args_json) {
  Map2dSink* sink = sink_of(host);
  if (!sink || !sink->present_bridges_installed()) {
    return fail_no_map("map2d.present_gpu");
  }
  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"map2d.present_gpu\"}");
    return false;
  }
  double w = 1280.0;
  double h = 720.0;
  (void)json_double(args, "width", &w);
  (void)json_double(args, "height", &h);
  if (!sink->present_gpu(static_cast<uint32_t>(w), static_cast<uint32_t>(h))) {
    set_operation_result(
        "{\"error\":\"present_failed\",\"op\":\"map2d.present_gpu\"}");
    return false;
  }
  set_operation_result("{\"ok\":true,\"op\":\"map2d.present_gpu\"}");
  return true;
}

bool process_add_standin(content::PluginHost* host, std::string_view args_json) {
  Map2dSink* sink = sink_of(host);
  if (!sink) {
    return fail_no_map("map2d.add_standin_layer");
  }
  rapidjson::Document args;
  if (!parse_args(args_json, &args)) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"map2d.add_standin_layer\"}");
    return false;
  }
  std::string name = "standin";
  (void)json_string(args, "name", &name);
  double lon = 0.0;
  double lat = 0.0;
  double half = 0.5;
  if (!json_double(args, "lon", &lon) || !json_double(args, "lat", &lat)) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"map2d.add_standin_layer\","
        "\"need\":\"lon,lat\"}");
    return false;
  }
  (void)json_double(args, "half_deg", &half);
  if (!sink->add_standin_layer(name, lon, lat, half)) {
    set_operation_result(
        "{\"error\":\"add_failed\",\"op\":\"map2d.add_standin_layer\"}");
    return false;
  }
  set_operation_result("{\"ok\":true,\"op\":\"map2d.add_standin_layer\"}");
  return true;
}

bool contribute_one(content::PluginHost* host, const char* id, const char* title,
                    bool (*fn)(content::PluginHost*, std::string_view)) {
  return host->contribute_command(
             kPluginId, id, title, "tools",
             [host, fn](const tool::CommandArgs& args) {
               return fn(host, args.payload);
             }) &&
         host->contribute_processing(kPluginId, {id, title}, fn);
}

}  // namespace

bool register_map2d_sink_verbs(content::PluginHost* host) {
  if (!host) {
    return false;
  }
  if (tool::CommandCatalog* catalog = host->commands()) {
    if (catalog->contains("map2d.open_map")) {
      return true;
    }
  }
  return contribute_one(host, "map2d.open_map", "打开二维地图", process_open_map) &&
         contribute_one(host, "map2d.frame_to", "二维框选", process_frame_to) &&
         contribute_one(host, "map2d.attach_dataset", "挂载二维数据",
                        process_attach_dataset) &&
         contribute_one(host, "map2d.load_hillshade", "加载二维DEM",
                        process_load_hillshade) &&
         contribute_one(host, "map2d.apply_look", "二维外观预设",
                        process_apply_look) &&
         contribute_one(host, "map2d.frame_fly", "二维飞入", process_frame_fly) &&
         contribute_one(host, "map2d.export_bmp", "导出二维BMP",
                        process_export_bmp) &&
         contribute_one(host, "map2d.present_gpu", "二维GPU呈现",
                        process_present_gpu) &&
         contribute_one(host, "map2d.add_standin_layer", "二维占位多边形",
                        process_add_standin);
}

}  // namespace plugin
