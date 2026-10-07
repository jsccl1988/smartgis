// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scene/earth/register.h"

#include "content/public/plugin_host.h"
#include "plugin/product/world3d/scene/detail/host.h"
#include "plugin/runtime/host/capability/capability.h"
#include "plugin/runtime/host/capability/contribute.h"
#include "plugin/runtime/host/processing/args_json.h"
#include "plugin/runtime/host/processing/operation_result.h"
#include "tool/command/command.h"

#include <string>
#include <string_view>

namespace plugin {
namespace {

content::PluginHost* g_host = nullptr;

bool process_open_earth(content::PluginHost* host, std::string_view) {
  if (!detail::world3d_earth_ready(host)) {
    return detail::fail_no_scene_processing("world3d.open_earth");
  }
  if (!detail::world3d_scene_sink(host)->open_earth()) {
    set_operation_result(
        "{\"error\":\"open_failed\",\"op\":\"world3d.open_earth\"}");
    return false;
  }
  set_operation_result(
      "{\"ok\":true,\"op\":\"world3d.open_earth\",\"contour\":\"defaults\"}");
  return true;
}

bool process_fly_to(content::PluginHost* host, std::string_view args_json) {
  if (!detail::world3d_earth_ready(host)) {
    return detail::fail_no_scene_processing("world3d.fly_to");
  }
  rapidjson::Document args;
  if (!parse_args_json(args_json, &args)) {
    set_operation_result("{\"error\":\"bad_args\",\"op\":\"world3d.fly_to\"}");
    return false;
  }
  double lon = 0.0;
  double lat = 0.0;
  if (!args_json_double(args, "lon", &lon) ||
      !args_json_double(args, "lat", &lat)) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"world3d.fly_to\",\"need\":\"lon,lat\"}");
    return false;
  }
  double distance = 1.6;
  double span_deg = 4.0;
  (void)args_json_double(args, "distance", &distance);
  (void)args_json_double(args, "span_deg", &span_deg);
  if (!detail::world3d_scene_sink(host)->fly_to(
          lon, lat, static_cast<float>(distance), span_deg)) {
    set_operation_result("{\"error\":\"fly_failed\",\"op\":\"world3d.fly_to\"}");
    return false;
  }
  set_operation_result("{\"ok\":true,\"op\":\"world3d.fly_to\"}");
  return true;
}

bool process_attach_city_tileset(content::PluginHost* host,
                                 std::string_view args_json) {
  if (!detail::world3d_scene_sink(host)) {
    return detail::fail_no_scene_processing("world3d.attach_city_tileset");
  }
  rapidjson::Document args;
  if (!parse_args_json(args_json, &args)) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"world3d.attach_city_tileset\"}");
    return false;
  }
  std::string path;
  (void)args_json_string(args, "path", &path);
  if (!detail::world3d_scene_sink(host)->attach_tileset(path)) {
    set_operation_result(
        "{\"error\":\"attach_failed\",\"op\":\"world3d.attach_city_tileset\"}");
    return false;
  }
  set_operation_result("{\"ok\":true,\"op\":\"world3d.attach_city_tileset\"}");
  return true;
}

bool process_load_global_dem(content::PluginHost* host,
                             std::string_view args_json) {
  if (!detail::world3d_earth_ready(host)) {
    return detail::fail_no_scene_processing("world3d.load_global_dem");
  }
  rapidjson::Document args;
  if (!parse_args_json(args_json, &args)) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"world3d.load_global_dem\"}");
    return false;
  }
  std::string path;
  (void)args_json_string(args, "path", &path);
  std::string result;
  if (!detail::world3d_scene_sink(host)->load_global_dem(path, &result)) {
    if (result.empty()) {
      set_operation_result(
          "{\"error\":\"load_failed\",\"op\":\"world3d.load_global_dem\"}");
    } else {
      set_operation_result(result);
    }
    return false;
  }
  if (result.empty()) {
    set_operation_result("{\"ok\":true,\"op\":\"world3d.load_global_dem\"}");
  } else {
    set_operation_result(result);
  }
  return true;
}

bool process_set_satellite_cloud(content::PluginHost* host,
                                 std::string_view args_json) {
  if (!detail::world3d_earth_ready(host)) {
    return detail::fail_no_scene_processing("world3d.set_satellite_cloud");
  }
  rapidjson::Document args;
  if (!parse_args_json(args_json, &args)) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"world3d.set_satellite_cloud\"}");
    return false;
  }
  std::string path;
  (void)args_json_string(args, "path", &path);
  bool enabled = true;
  (void)args_json_bool(args, "enabled", &enabled);
  std::string result;
  if (!detail::world3d_scene_sink(host)->set_satellite_cloud(path, enabled,
                                                            &result)) {
    if (result.empty()) {
      set_operation_result(
          "{\"error\":\"cloud_failed\",\"op\":\"world3d.set_satellite_cloud\"}");
    } else {
      set_operation_result(result);
    }
    return false;
  }
  if (result.empty()) {
    set_operation_result(
        "{\"ok\":true,\"op\":\"world3d.set_satellite_cloud\"}");
  } else {
    set_operation_result(result);
  }
  return true;
}

bool process_set_atmosphere(content::PluginHost* host,
                            std::string_view args_json) {
  if (!detail::world3d_earth_ready(host)) {
    return detail::fail_no_scene_processing("world3d.set_atmosphere");
  }
  rapidjson::Document args;
  if (!parse_args_json(args_json, &args)) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"world3d.set_atmosphere\"}");
    return false;
  }
  bool sky = true;
  bool ocean = true;
  bool cloud = true;
  bool fog = true;
  (void)args_json_bool(args, "sky", &sky);
  (void)args_json_bool(args, "ocean", &ocean);
  (void)args_json_bool(args, "cloud", &cloud);
  (void)args_json_bool(args, "fog", &fog);
  if (!detail::world3d_scene_sink(host)->set_atmosphere(sky, ocean, cloud,
                                                       fog)) {
    set_operation_result(
        "{\"error\":\"atmo_failed\",\"op\":\"world3d.set_atmosphere\"}");
    return false;
  }
  set_operation_result("{\"ok\":true,\"op\":\"world3d.set_atmosphere\"}");
  return true;
}

bool process_apply_look(content::PluginHost* host, std::string_view args_json) {
  Scene3dSink* sink = detail::world3d_scene_sink(host);
  if (!sink || !sink->look_bridges_installed()) {
    return detail::fail_no_scene_processing("world3d.apply_look");
  }
  rapidjson::Document args;
  if (!parse_args_json(args_json, &args)) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"world3d.apply_look\"}");
    return false;
  }
  std::string mode;
  if (!args_json_string(args, "mode", &mode) || mode.empty()) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"world3d.apply_look\",\"need\":\"mode\"}");
    return false;
  }
  std::string result;
  if (!sink->apply_look(mode, &result)) {
    if (result.empty()) {
      set_operation_result(
          "{\"error\":\"look_failed\",\"op\":\"world3d.apply_look\"}");
    } else {
      set_operation_result(result);
    }
    return false;
  }
  if (result.empty()) {
    set_operation_result("{\"ok\":true,\"op\":\"world3d.apply_look\"}");
  } else {
    set_operation_result(result);
  }
  return true;
}

bool process_fly_globe(content::PluginHost* host, std::string_view args_json) {
  Scene3dSink* sink = detail::world3d_scene_sink(host);
  if (!sink || !sink->look_bridges_installed()) {
    return detail::fail_no_scene_processing("world3d.fly_globe");
  }
  rapidjson::Document args;
  if (!parse_args_json(args_json, &args)) {
    set_operation_result("{\"error\":\"bad_args\",\"op\":\"world3d.fly_globe\"}");
    return false;
  }
  double t = 0.48;
  (void)args_json_double(args, "t", &t);
  std::string result;
  if (!sink->fly_globe(static_cast<float>(t), &result)) {
    if (result.empty()) {
      set_operation_result(
          "{\"error\":\"fly_failed\",\"op\":\"world3d.fly_globe\"}");
    } else {
      set_operation_result(result);
    }
    return false;
  }
  if (result.empty()) {
    set_operation_result("{\"ok\":true,\"op\":\"world3d.fly_globe\"}");
  } else {
    set_operation_result(result);
  }
  return true;
}

bool handle_open_earth(const tool::CommandArgs&) {
  if (!detail::world3d_earth_ready(g_host)) {
    return detail::fail_no_scene("world3d.open_earth");
  }
  return process_open_earth(g_host, {});
}

bool handle_fly_to(const tool::CommandArgs& args) {
  if (!detail::world3d_earth_ready(g_host)) {
    return detail::fail_no_scene("world3d.fly_to");
  }
  return process_fly_to(g_host, args.payload);
}

bool handle_attach_city_tileset(const tool::CommandArgs& args) {
  if (!detail::world3d_scene_sink(g_host)) {
    return detail::fail_no_scene("world3d.attach_city_tileset");
  }
  return process_attach_city_tileset(g_host, args.payload);
}

bool handle_load_global_dem(const tool::CommandArgs& args) {
  if (!detail::world3d_earth_ready(g_host)) {
    return detail::fail_no_scene("world3d.load_global_dem");
  }
  return process_load_global_dem(g_host, args.payload);
}

bool handle_set_satellite_cloud(const tool::CommandArgs& args) {
  if (!detail::world3d_earth_ready(g_host)) {
    return detail::fail_no_scene("world3d.set_satellite_cloud");
  }
  return process_set_satellite_cloud(g_host, args.payload);
}

bool handle_set_atmosphere(const tool::CommandArgs& args) {
  if (!detail::world3d_earth_ready(g_host)) {
    return detail::fail_no_scene("world3d.set_atmosphere");
  }
  return process_set_atmosphere(g_host, args.payload);
}

bool handle_apply_look(const tool::CommandArgs& args) {
  Scene3dSink* sink = detail::world3d_scene_sink(g_host);
  if (!g_host || !sink || !sink->look_bridges_installed()) {
    return detail::fail_no_scene("world3d.apply_look");
  }
  return process_apply_look(g_host, args.payload);
}

bool handle_fly_globe(const tool::CommandArgs& args) {
  Scene3dSink* sink = detail::world3d_scene_sink(g_host);
  if (!g_host || !sink || !sink->look_bridges_installed()) {
    return detail::fail_no_scene("world3d.fly_globe");
  }
  return process_fly_globe(g_host, args.payload);
}

}  // namespace

namespace detail {

bool register_world3d_earth(content::PluginHost* host) {
  if (!host) {
    return false;
  }
  g_host = host;
  return contribute_command_aliases(
             host, kWorld3dPluginId,
             {{"world3d.open_earth", "打开真三维地球"}}, "tools",
             handle_open_earth) &&
         contribute_command_aliases(host, kWorld3dPluginId,
                                    {{"world3d.fly_to", "飞行到"}}, "tools",
                                    handle_fly_to) &&
         contribute_command_aliases(
             host, kWorld3dPluginId,
             {{"world3d.attach_city_tileset", "挂载城市瓦片"}}, "tools",
             handle_attach_city_tileset) &&
         contribute_command_aliases(
             host, kWorld3dPluginId,
             {{"world3d.load_global_dem", "加载全球DEM"}}, "tools",
             handle_load_global_dem) &&
         contribute_command_aliases(
             host, kWorld3dPluginId,
             {{"world3d.set_satellite_cloud", "卫星云图"}}, "tools",
             handle_set_satellite_cloud) &&
         contribute_command_aliases(
             host, kWorld3dPluginId,
             {{"world3d.set_atmosphere", "大气层开关"}}, "tools",
             handle_set_atmosphere) &&
         contribute_command_aliases(
             host, kWorld3dPluginId, {{"world3d.apply_look", "地球外观预设"}},
             "tools", handle_apply_look) &&
         contribute_command_aliases(
             host, kWorld3dPluginId, {{"world3d.fly_globe", "地球飞入"}},
             "tools", handle_fly_globe) &&
         contribute_processing_aliases(
             host, kWorld3dPluginId,
             {{"world3d.open_earth", "打开真三维地球"}},
             process_open_earth) &&
         contribute_processing_aliases(host, kWorld3dPluginId,
                                       {{"world3d.fly_to", "飞行到"}},
                                       process_fly_to) &&
         contribute_processing_aliases(
             host, kWorld3dPluginId,
             {{"world3d.attach_city_tileset", "挂载城市瓦片"}},
             process_attach_city_tileset) &&
         contribute_processing_aliases(
             host, kWorld3dPluginId,
             {{"world3d.load_global_dem", "加载全球DEM"}},
             process_load_global_dem) &&
         contribute_processing_aliases(
             host, kWorld3dPluginId,
             {{"world3d.set_satellite_cloud", "卫星云图"}},
             process_set_satellite_cloud) &&
         contribute_processing_aliases(
             host, kWorld3dPluginId,
             {{"world3d.set_atmosphere", "大气层开关"}},
             process_set_atmosphere) &&
         contribute_processing_aliases(
             host, kWorld3dPluginId, {{"world3d.apply_look", "地球外观预设"}},
             process_apply_look) &&
         contribute_processing_aliases(
             host, kWorld3dPluginId, {{"world3d.fly_globe", "地球飞入"}},
             process_fly_globe);
}

}  // namespace detail
}  // namespace plugin
