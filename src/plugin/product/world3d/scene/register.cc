// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scene/register.h"

#include <functional>
#include <string>
#include <string_view>
#include <utility>

#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/capability.h"
#include "plugin/product/world3d/commands.h"
#include "plugin/product/world3d/detail/contribute.h"
#include "plugin/product/world3d/scene/present/contour.h"
#include "plugin/product/world3d/scene/present/pointcloud.h"
#include "plugin/product/world3d/scene/present/standin.h"
#include "plugin/product/world3d/scene/present/style.h"
#include "vista/assets/pointcloud/pdal_io.h"
#include "plugin/runtime/host/processing/operation_result.h"
#include "tool/command/command.h"
#include "ui/views/dialogs/file_picker.h"
#include "ui/views/dialogs/message_box.h"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace plugin {
namespace {

constexpr const char* kPluginId = "smartgis.world3d";

content::PluginHost* g_host = nullptr;

plugin::Scene3dSink* scene_sink(content::PluginHost* host) {
  return plugin::scene3d_sink(host);
}

bool earth_sink_ready(content::PluginHost* host) {
  plugin::Scene3dSink* sink = scene_sink(host);
  return sink && sink->earth_bridges_installed();
}

bool process_add_pointcloud(content::PluginHost* host,
                            std::string_view args_json);
bool process_add_sphere(content::PluginHost* host, std::string_view args_json);
bool process_add_water(content::PluginHost* host, std::string_view args_json);
bool process_add_terrain_heightmap(content::PluginHost* host,
                                   std::string_view args_json);
bool process_add_terrain_trimesh(content::PluginHost* host,
                                 std::string_view args_json);
bool process_create_trimesh(content::PluginHost* host,
                            std::string_view args_json);
bool process_layer_points_to_3d(content::PluginHost* host,
                                std::string_view args_json);
bool process_layer_lines_to_3d(content::PluginHost* host,
                               std::string_view args_json);
bool process_layer_polygons_to_3d(content::PluginHost* host,
                                  std::string_view args_json);
bool process_open_earth(content::PluginHost* host, std::string_view args_json);
bool process_fly_to(content::PluginHost* host, std::string_view args_json);
bool process_attach_city_tileset(content::PluginHost* host,
                                 std::string_view args_json);
bool process_load_global_dem(content::PluginHost* host,
                             std::string_view args_json);
bool process_set_satellite_cloud(content::PluginHost* host,
                                 std::string_view args_json);
bool process_set_atmosphere(content::PluginHost* host,
                            std::string_view args_json);
bool process_apply_look(content::PluginHost* host, std::string_view args_json);
bool process_fly_globe(content::PluginHost* host, std::string_view args_json);

bool fail_no_scene(const char* command) {
  set_operation_result(std::string("{\"error\":\"no_scene_device\",\"command\":\"") +
                       command + "\"}");
  ui::views::show_message_box(
      ui::views::MessageBoxKind::kError,
      "No scene render device is attached to the host.");
  return false;
}

bool fail_no_scene_processing(const char* command) {
  set_operation_result(std::string("{\"error\":\"no_scene_device\",\"command\":\"") +
                       command + "\"}");
  return false;
}

bool present_standin(content::PluginHost* host, const char* name, double lon,
                     double lat, double half_deg, const char* op) {
  if (!host || !host->gis_document()) {
    return fail_no_scene_processing(op);
  }
  if (!present_world3d_standin_mesh(host->gis_document(), scene_sink(host),
                                    name, lon, lat, half_deg)) {
    set_operation_result(std::string("{\"error\":\"add_failed\",\"op\":\"") +
                         op + "\"}");
    return false;
  }
  (void)apply_world3d_mesh_style(host->gis_document());
  (void)host->present_dataset(kPluginId, "", 1);
  set_operation_result(std::string("{\"ok\":true,\"op\":\"") + op + "\"}");
  return true;
}

bool present_active_layer(content::PluginHost* host, const char* op) {
  if (!host || !host->gis_document()) {
    return fail_no_scene_processing(op);
  }
  if (host->gis_document()->feature_count() == 0) {
    set_operation_result(std::string("{\"error\":\"add_failed\",\"op\":\"") +
                         op + "\"}");
    return false;
  }
  (void)host->present_dataset(kPluginId, "", 1);
  set_operation_result(std::string("{\"ok\":true,\"op\":\"") + op + "\"}");
  return true;
}

bool commit_pointcloud(content::PluginHost* host, const float* xyz,
                       int point_count, const uint8_t* rgba, const char* op) {
  if (!host || !host->gis_document()) {
    return fail_no_scene_processing(op);
  }
  if (!present_world3d_pointcloud_xyz(host->gis_document(), xyz, point_count,
                                      rgba)) {
    set_operation_result(std::string("{\"error\":\"add_failed\",\"op\":\"") +
                         op + "\"}");
    return false;
  }
  (void)apply_world3d_mesh_style(host->gis_document());
  (void)host->present_dataset(kPluginId, "", 1);
  return true;
}

bool parse_scene_args(std::string_view json, rapidjson::Document* out) {
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

bool scene_json_get_string(const rapidjson::Value& obj,
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

bool scene_json_get_double(const rapidjson::Value& obj, const char* key,
                           double* out) {
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

bool handle_add_pointcloud(const tool::CommandArgs&) {
  constexpr wchar_t kFilter[] =
      L"Point Cloud (*.las;*.laz;*.txt)\0*.las;*.laz;*.txt\0"
      L"LAS (*.las)\0*.las\0"
      L"LAZ (*.laz)\0*.laz\0"
      L"Text (*.txt)\0*.txt\0"
      L"All Files (*.*)\0*.*\0";
  const ui::views::FilePickerResult pick =
      ui::views::pick_open_file(kFilter);
  if (!pick.accepted || pick.path.empty()) {
    return false;
  }
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.StartObject();
  w.Key("path");
  w.String(pick.path.c_str());
  w.EndObject();
  return process_add_pointcloud(g_host, buf.GetString());
}

bool handle_add_sphere(const tool::CommandArgs&) {
  if (!g_host || !g_host->gis_document()) {
    return fail_no_scene("model3d.add_sphere");
  }
  return process_add_sphere(g_host, {});
}

bool handle_add_water(const tool::CommandArgs&) {
  if (!g_host || !g_host->gis_document()) {
    return fail_no_scene("model3d.add_water");
  }
  return process_add_water(g_host, {});
}

bool handle_open_earth(const tool::CommandArgs&) {
  if (!earth_sink_ready(g_host)) {
    return fail_no_scene("world3d.open_earth");
  }
  return process_open_earth(g_host, {});
}

bool handle_fly_to(const tool::CommandArgs& args) {
  if (!earth_sink_ready(g_host)) {
    return fail_no_scene("world3d.fly_to");
  }
  return process_fly_to(g_host, args.payload);
}

bool handle_attach_city_tileset(const tool::CommandArgs& args) {
  if (!scene_sink(g_host)) {
    return fail_no_scene("world3d.attach_city_tileset");
  }
  return process_attach_city_tileset(g_host, args.payload);
}

bool scene_json_get_bool(const rapidjson::Value& obj, const char* key,
                         bool* out) {
  if (!out || !key || !obj.IsObject()) {
    return false;
  }
  const auto it = obj.FindMember(key);
  if (it == obj.MemberEnd() || !it->value.IsBool()) {
    return false;
  }
  *out = it->value.GetBool();
  return true;
}

bool handle_load_global_dem(const tool::CommandArgs& args) {
  if (!earth_sink_ready(g_host)) {
    return fail_no_scene("world3d.load_global_dem");
  }
  return process_load_global_dem(g_host, args.payload);
}

bool handle_set_satellite_cloud(const tool::CommandArgs& args) {
  if (!earth_sink_ready(g_host)) {
    return fail_no_scene("world3d.set_satellite_cloud");
  }
  return process_set_satellite_cloud(g_host, args.payload);
}

bool handle_set_atmosphere(const tool::CommandArgs& args) {
  if (!earth_sink_ready(g_host)) {
    return fail_no_scene("world3d.set_atmosphere");
  }
  return process_set_atmosphere(g_host, args.payload);
}

bool handle_apply_look(const tool::CommandArgs& args) {
  if (!g_host || !scene_sink(g_host) ||
      !scene_sink(g_host)->look_bridges_installed()) {
    return fail_no_scene("world3d.apply_look");
  }
  return process_apply_look(g_host, args.payload);
}

bool handle_fly_globe(const tool::CommandArgs& args) {
  if (!g_host || !scene_sink(g_host) ||
      !scene_sink(g_host)->look_bridges_installed()) {
    return fail_no_scene("world3d.fly_globe");
  }
  return process_fly_globe(g_host, args.payload);
}

bool handle_add_terrain_heightmap(const tool::CommandArgs&) {
  if (!g_host || !g_host->gis_document()) {
    return fail_no_scene("model3d.add_terrain_heightmap");
  }
  return process_add_terrain_heightmap(g_host, {});
}

bool handle_add_terrain_trimesh(const tool::CommandArgs&) {
  if (!g_host || !g_host->gis_document()) {
    return fail_no_scene("model3d.add_terrain_trimesh");
  }
  return process_add_terrain_trimesh(g_host, {});
}

bool handle_create_trimesh(const tool::CommandArgs&) {
  if (!g_host || !g_host->gis_document()) {
    return fail_no_scene("model3d.create_trimesh");
  }
  if (process_create_trimesh(g_host, {})) {
    ui::views::show_message_box(ui::views::MessageBoxKind::kInfo,
                                "Trimesh created successfully.");
    return true;
  }
  return fail_no_scene("model3d.create_trimesh");
}

bool handle_layer_points_to_3d(const tool::CommandArgs&) {
  if (!g_host || !g_host->gis_document()) {
    return fail_no_scene("model3d.layer_points_to_3d");
  }
  if (process_layer_points_to_3d(g_host, {})) {
    return true;
  }
  ui::views::show_message_box(ui::views::MessageBoxKind::kInfo,
                              "Please activate a point layer.");
  return false;
}

bool handle_layer_lines_to_3d(const tool::CommandArgs&) {
  if (!g_host || !g_host->gis_document()) {
    return fail_no_scene("model3d.layer_lines_to_3d");
  }
  if (process_layer_lines_to_3d(g_host, {})) {
    return true;
  }
  ui::views::show_message_box(ui::views::MessageBoxKind::kInfo,
                              "Please activate a line layer.");
  return false;
}

bool handle_layer_polygons_to_3d(const tool::CommandArgs&) {
  if (!g_host || !g_host->gis_document()) {
    return fail_no_scene("model3d.layer_polygons_to_3d");
  }
  if (process_layer_polygons_to_3d(g_host, {})) {
    return true;
  }
  ui::views::show_message_box(ui::views::MessageBoxKind::kInfo,
                              "Please activate a polygon layer.");
  return false;
}

bool process_add_pointcloud(content::PluginHost* host,
                            std::string_view args_json) {
  rapidjson::Document args;
  if (!parse_scene_args(args_json, &args)) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"model3d.add_pointcloud\"}");
    return false;
  }
  std::string path;
  if (!scene_json_get_string(args, "path", &path) || path.empty()) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"model3d.add_pointcloud\"}");
    return false;
  }
  if (!host || !host->gis_document()) {
    return fail_no_scene_processing("model3d.add_pointcloud");
  }
  if (!present_world3d_pointcloud(host->gis_document(), path)) {
    set_operation_result(
        "{\"error\":\"add_failed\",\"op\":\"model3d.add_pointcloud\"}");
    return false;
  }
  (void)apply_world3d_mesh_style(host->gis_document());
  (void)host->present_dataset(kPluginId, "", 1);
  set_operation_result("{\"ok\":true,\"op\":\"model3d.add_pointcloud\"}");
  return true;
}

bool attach_pointcloud_result(content::PluginHost* host,
                              const vista::PointCloud& cloud, const char* op) {
  const uint8_t* rgba = cloud.has_color() ? cloud.rgba.data() : nullptr;
  if (!commit_pointcloud(host, cloud.xyz.data(),
                         static_cast<int>(cloud.point_count()), rgba, op)) {
    return false;
  }
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.StartObject();
  w.Key("ok");
  w.Bool(true);
  w.Key("op");
  w.String(op);
  w.Key("point_count");
  w.Uint64(cloud.point_count());
  w.Key("pdal");
  w.Bool(vista::pdal_is_available());
  w.EndObject();
  set_operation_result(buf.GetString());
  return true;
}

bool process_pdal_read(content::PluginHost* host, std::string_view args_json) {
  rapidjson::Document args;
  if (!parse_scene_args(args_json, &args)) {
    set_operation_result("{\"error\":\"bad_args\",\"op\":\"world3d.pdal_read\"}");
    return false;
  }
  std::string path;
  if (!scene_json_get_string(args, "path", &path) || path.empty()) {
    set_operation_result("{\"error\":\"bad_args\",\"op\":\"world3d.pdal_read\"}");
    return false;
  }
  vista::PdalReadOptions opts;
  if (args.HasMember("z_min") && args["z_min"].IsNumber() &&
      args.HasMember("z_max") && args["z_max"].IsNumber()) {
    opts.has_z_range = true;
    opts.z_min = args["z_min"].GetDouble();
    opts.z_max = args["z_max"].GetDouble();
  }
  if (args.HasMember("max_points") && args["max_points"].IsUint64()) {
    opts.max_points = static_cast<size_t>(args["max_points"].GetUint64());
  }
  vista::PointCloud cloud;
  if (!vista::run_pdal_read(path.c_str(), opts, &cloud)) {
    const char* err =
        cloud.error.empty() ? "pdal_read_failed" : cloud.error.c_str();
    set_operation_result(std::string("{\"error\":\"") + err +
                         "\",\"op\":\"world3d.pdal_read\"}");
    return false;
  }
  bool attach = true;
  if (args.HasMember("attach") && args["attach"].IsBool()) {
    attach = args["attach"].GetBool();
  }
  if (!attach) {
    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> w(buf);
    w.StartObject();
    w.Key("ok");
    w.Bool(true);
    w.Key("op");
    w.String("world3d.pdal_read");
    w.Key("point_count");
    w.Uint64(cloud.point_count());
    w.Key("pdal");
    w.Bool(vista::pdal_is_available());
    w.EndObject();
    set_operation_result(buf.GetString());
    return true;
  }
  return attach_pointcloud_result(host, cloud, "world3d.pdal_read");
}

bool process_pdal_pipeline(content::PluginHost* host,
                           std::string_view args_json) {
  rapidjson::Document args;
  if (!parse_scene_args(args_json, &args)) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"world3d.pdal_pipeline\"}");
    return false;
  }
  std::string pipeline_json;
  if (args.HasMember("pipeline") && args["pipeline"].IsArray()) {
    rapidjson::Document wrap;
    wrap.SetObject();
    rapidjson::Value pipe(args["pipeline"], wrap.GetAllocator());
    wrap.AddMember("pipeline", pipe, wrap.GetAllocator());
    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> w(buf);
    wrap.Accept(w);
    pipeline_json = buf.GetString();
  } else if (!scene_json_get_string(args, "pipeline_json", &pipeline_json) ||
             pipeline_json.empty()) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"world3d.pdal_pipeline\"}");
    return false;
  }
  vista::PointCloud cloud;
  if (!vista::run_pdal_pipeline_json(pipeline_json, &cloud)) {
    const char* err =
        cloud.error.empty() ? "pdal_pipeline_failed" : cloud.error.c_str();
    set_operation_result(std::string("{\"error\":\"") + err +
                         "\",\"op\":\"world3d.pdal_pipeline\"}");
    return false;
  }
  bool attach = true;
  if (args.HasMember("attach") && args["attach"].IsBool()) {
    attach = args["attach"].GetBool();
  }
  if (!attach) {
    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> w(buf);
    w.StartObject();
    w.Key("ok");
    w.Bool(true);
    w.Key("op");
    w.String("world3d.pdal_pipeline");
    w.Key("point_count");
    w.Uint64(cloud.point_count());
    w.Key("pdal");
    w.Bool(vista::pdal_is_available());
    w.EndObject();
    set_operation_result(buf.GetString());
    return true;
  }
  return attach_pointcloud_result(host, cloud, "world3d.pdal_pipeline");
}

bool process_add_sphere(content::PluginHost* host, std::string_view) {
  return present_standin(host, "Sphere", 105.0, 35.0, 0.4,
                         "model3d.add_sphere");
}

bool process_add_water(content::PluginHost* host, std::string_view) {
  return present_standin(host, "Water", 110.0, 30.0, 0.6, "model3d.add_water");
}

bool process_add_terrain_heightmap(content::PluginHost* host,
                                   std::string_view) {
  return present_standin(host, "Terrain heightmap", 100.0, 35.0, 1.0,
                         "model3d.add_terrain_heightmap");
}

bool process_add_terrain_trimesh(content::PluginHost* host, std::string_view) {
  return present_standin(host, "Terrain trimesh", 102.0, 36.0, 1.0,
                         "model3d.add_terrain_trimesh");
}

bool process_create_trimesh(content::PluginHost* host, std::string_view) {
  return present_active_layer(host, "model3d.create_trimesh");
}

bool process_layer_points_to_3d(content::PluginHost* host, std::string_view) {
  return present_active_layer(host, "model3d.layer_points_to_3d");
}

bool process_layer_lines_to_3d(content::PluginHost* host, std::string_view) {
  return present_active_layer(host, "model3d.layer_lines_to_3d");
}

bool process_layer_polygons_to_3d(content::PluginHost* host, std::string_view) {
  return present_active_layer(host, "model3d.layer_polygons_to_3d");
}

bool process_open_earth(content::PluginHost* host,
                        std::string_view /*args_json*/) {
  if (!earth_sink_ready(host)) {
    return fail_no_scene_processing("world3d.open_earth");
  }
  if (!scene_sink(host)->open_earth()) {
    set_operation_result("{\"error\":\"open_failed\",\"op\":\"world3d.open_earth\"}");
    return false;
  }
  // Contour suite is applied inside apply_china_scene3d_product_defaults
  // (open_earth bridge). Result notes the default face.
  set_operation_result(
      "{\"ok\":true,\"op\":\"world3d.open_earth\",\"contour\":\"defaults\"}");
  return true;
}

bool process_fly_to(content::PluginHost* host, std::string_view args_json) {
  if (!earth_sink_ready(host)) {
    return fail_no_scene_processing("world3d.fly_to");
  }
  rapidjson::Document args;
  if (!parse_scene_args(args_json, &args)) {
    set_operation_result("{\"error\":\"bad_args\",\"op\":\"world3d.fly_to\"}");
    return false;
  }
  double lon = 0.0;
  double lat = 0.0;
  if (!scene_json_get_double(args, "lon", &lon) ||
      !scene_json_get_double(args, "lat", &lat)) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"world3d.fly_to\",\"need\":\"lon,lat\"}");
    return false;
  }
  double distance = 1.6;
  double span_deg = 4.0;
  (void)scene_json_get_double(args, "distance", &distance);
  (void)scene_json_get_double(args, "span_deg", &span_deg);
  if (!scene_sink(host)->fly_to(lon, lat, static_cast<float>(distance),
                                    span_deg)) {
    set_operation_result("{\"error\":\"fly_failed\",\"op\":\"world3d.fly_to\"}");
    return false;
  }
  set_operation_result("{\"ok\":true,\"op\":\"world3d.fly_to\"}");
  return true;
}

bool process_attach_city_tileset(content::PluginHost* host,
                                 std::string_view args_json) {
  if (!scene_sink(host)) {
    return fail_no_scene_processing("world3d.attach_city_tileset");
  }
  rapidjson::Document args;
  if (!parse_scene_args(args_json, &args)) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"world3d.attach_city_tileset\"}");
    return false;
  }
  std::string path;
  (void)scene_json_get_string(args, "path", &path);
  if (!scene_sink(host)->attach_tileset(path)) {
    set_operation_result(
        "{\"error\":\"attach_failed\",\"op\":\"world3d.attach_city_tileset\"}");
    return false;
  }
  set_operation_result("{\"ok\":true,\"op\":\"world3d.attach_city_tileset\"}");
  return true;
}

bool process_load_global_dem(content::PluginHost* host,
                             std::string_view args_json) {
  if (!earth_sink_ready(host)) {
    return fail_no_scene_processing("world3d.load_global_dem");
  }
  rapidjson::Document args;
  if (!parse_scene_args(args_json, &args)) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"world3d.load_global_dem\"}");
    return false;
  }
  std::string path;
  (void)scene_json_get_string(args, "path", &path);
  std::string result;
  if (!scene_sink(host)->load_global_dem(path, &result)) {
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
  if (!earth_sink_ready(host)) {
    return fail_no_scene_processing("world3d.set_satellite_cloud");
  }
  rapidjson::Document args;
  if (!parse_scene_args(args_json, &args)) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"world3d.set_satellite_cloud\"}");
    return false;
  }
  std::string path;
  (void)scene_json_get_string(args, "path", &path);
  bool enabled = true;
  (void)scene_json_get_bool(args, "enabled", &enabled);
  std::string result;
  if (!scene_sink(host)->set_satellite_cloud(path, enabled, &result)) {
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
  if (!earth_sink_ready(host)) {
    return fail_no_scene_processing("world3d.set_atmosphere");
  }
  rapidjson::Document args;
  if (!parse_scene_args(args_json, &args)) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"world3d.set_atmosphere\"}");
    return false;
  }
  bool sky = true;
  bool ocean = true;
  bool cloud = true;
  bool fog = true;
  (void)scene_json_get_bool(args, "sky", &sky);
  (void)scene_json_get_bool(args, "ocean", &ocean);
  (void)scene_json_get_bool(args, "cloud", &cloud);
  (void)scene_json_get_bool(args, "fog", &fog);
  if (!scene_sink(host)->set_atmosphere(sky, ocean, cloud, fog)) {
    set_operation_result(
        "{\"error\":\"atmo_failed\",\"op\":\"world3d.set_atmosphere\"}");
    return false;
  }
  set_operation_result("{\"ok\":true,\"op\":\"world3d.set_atmosphere\"}");
  return true;
}

bool process_apply_look(content::PluginHost* host, std::string_view args_json) {
  plugin::Scene3dSink* sink = scene_sink(host);
  if (!sink || !sink->look_bridges_installed()) {
    return fail_no_scene_processing("world3d.apply_look");
  }
  rapidjson::Document args;
  if (!parse_scene_args(args_json, &args)) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"world3d.apply_look\"}");
    return false;
  }
  std::string mode;
  if (!scene_json_get_string(args, "mode", &mode) || mode.empty()) {
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
  plugin::Scene3dSink* sink = scene_sink(host);
  if (!sink || !sink->look_bridges_installed()) {
    return fail_no_scene_processing("world3d.fly_globe");
  }
  rapidjson::Document args;
  if (!parse_scene_args(args_json, &args)) {
    set_operation_result("{\"error\":\"bad_args\",\"op\":\"world3d.fly_globe\"}");
    return false;
  }
  double t = 0.48;
  (void)scene_json_get_double(args, "t", &t);
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

}  // namespace

namespace detail {

bool register_world3d_scene(content::PluginHost* host) {
  if (!host) {
    return false;
  }
  g_host = host;

  return contribute_command_aliases(
             host,
             {{"model3d.add_pointcloud", "Add point cloud"},
              {"world3d.add_pointcloud", "加载点云"}},
             "tools", handle_add_pointcloud) &&
         contribute_command_aliases(host, {{"model3d.add_sphere", "Add sphere"}},
                                    "tools", handle_add_sphere) &&
         contribute_command_aliases(
             host, {{"model3d.add_water", "Add water surface"}}, "tools",
             handle_add_water) &&
         contribute_command_aliases(
             host,
             {{"model3d.add_terrain_heightmap", "Add terrain heightmap"}},
             "tools", handle_add_terrain_heightmap) &&
         contribute_command_aliases(
             host, {{"model3d.add_terrain_trimesh", "Add terrain trimesh"}},
             "tools", handle_add_terrain_trimesh) &&
         contribute_command_aliases(
             host, {{"model3d.create_trimesh", "Create trimesh"}}, "tools",
             handle_create_trimesh) &&
         contribute_command_aliases(
             host, {{"model3d.layer_points_to_3d", "2D points to 3D"}}, "tools",
             handle_layer_points_to_3d) &&
         contribute_command_aliases(
             host, {{"model3d.layer_lines_to_3d", "2D lines to 3D"}}, "tools",
             handle_layer_lines_to_3d) &&
         contribute_command_aliases(
             host, {{"model3d.layer_polygons_to_3d", "2D polygons to 3D"}},
             "tools", handle_layer_polygons_to_3d) &&
         contribute_command_aliases(
             host, {{"world3d.open_earth", "打开真三维地球"}}, "tools",
             handle_open_earth) &&
         contribute_command_aliases(host, {{"world3d.fly_to", "飞行到"}},
                                    "tools", handle_fly_to) &&
         contribute_command_aliases(
             host, {{"world3d.attach_city_tileset", "挂载城市瓦片"}}, "tools",
             handle_attach_city_tileset) &&
         contribute_command_aliases(
             host, {{"world3d.load_global_dem", "加载全球DEM"}}, "tools",
             handle_load_global_dem) &&
         contribute_command_aliases(
             host, {{"world3d.set_satellite_cloud", "卫星云图"}}, "tools",
             handle_set_satellite_cloud) &&
         contribute_command_aliases(
             host, {{"world3d.set_atmosphere", "大气层开关"}}, "tools",
             handle_set_atmosphere) &&
         contribute_command_aliases(
             host, {{"world3d.apply_look", "地球外观预设"}}, "tools",
             handle_apply_look) &&
         contribute_command_aliases(
             host, {{"world3d.fly_globe", "地球飞入"}}, "tools",
             handle_fly_globe) &&
         contribute_processing_aliases(
             host,
             {{"model3d.add_pointcloud", "Add point cloud"},
              {"world3d.add_pointcloud", "加载点云"}},
             process_add_pointcloud) &&
         contribute_processing_aliases(
             host, {{"world3d.pdal_read", "PDAL read LAS/LAZ"}},
             process_pdal_read) &&
         contribute_processing_aliases(
             host, {{"world3d.pdal_pipeline", "PDAL pipeline JSON"}},
             process_pdal_pipeline) &&
         contribute_processing_aliases(
             host, {{"model3d.add_sphere", "Add sphere"}}, process_add_sphere) &&
         contribute_processing_aliases(
             host, {{"model3d.add_water", "Add water surface"}},
             process_add_water) &&
         contribute_processing_aliases(
             host,
             {{"model3d.add_terrain_heightmap", "Add terrain heightmap"}},
             process_add_terrain_heightmap) &&
         contribute_processing_aliases(
             host, {{"model3d.add_terrain_trimesh", "Add terrain trimesh"}},
             process_add_terrain_trimesh) &&
         contribute_processing_aliases(
             host, {{"model3d.create_trimesh", "Create trimesh"}},
             process_create_trimesh) &&
         contribute_processing_aliases(
             host, {{"model3d.layer_points_to_3d", "2D points to 3D"}},
             process_layer_points_to_3d) &&
         contribute_processing_aliases(
             host, {{"model3d.layer_lines_to_3d", "2D lines to 3D"}},
             process_layer_lines_to_3d) &&
         contribute_processing_aliases(
             host, {{"model3d.layer_polygons_to_3d", "2D polygons to 3D"}},
             process_layer_polygons_to_3d) &&
         contribute_processing_aliases(
             host, {{"world3d.open_earth", "打开真三维地球"}},
             process_open_earth) &&
         contribute_processing_aliases(host, {{"world3d.fly_to", "飞行到"}},
                                       process_fly_to) &&
         contribute_processing_aliases(
             host, {{"world3d.attach_city_tileset", "挂载城市瓦片"}},
             process_attach_city_tileset) &&
         contribute_processing_aliases(
             host, {{"world3d.load_global_dem", "加载全球DEM"}},
             process_load_global_dem) &&
         contribute_processing_aliases(
             host, {{"world3d.set_satellite_cloud", "卫星云图"}},
             process_set_satellite_cloud) &&
         contribute_processing_aliases(
             host, {{"world3d.set_atmosphere", "大气层开关"}},
             process_set_atmosphere) &&
         contribute_processing_aliases(
             host, {{"world3d.apply_look", "地球外观预设"}},
             process_apply_look) &&
         contribute_processing_aliases(
             host, {{"world3d.fly_globe", "地球飞入"}}, process_fly_globe);
}

}  // namespace detail
}  // namespace plugin
