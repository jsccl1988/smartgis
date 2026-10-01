// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/commands.h"

#include <functional>
#include <string>
#include <string_view>
#include <utility>

#include "content/public/plugin_host.h"
#include "gis/vista/world/pointcloud/ingest/io/pdal_io.h"
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

World3dSceneWriter g_scene_writer;

void set_world3d_scene_writer_store(World3dSceneWriter writer) {
  g_scene_writer = std::move(writer);
}

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
  if (!g_scene_writer.add_pointcloud) {
    return fail_no_scene("model3d.add_pointcloud");
  }
  return g_scene_writer.add_pointcloud(pick.path);
}

bool handle_add_sphere(const tool::CommandArgs&) {
  if (!g_scene_writer.add_sphere) {
    return fail_no_scene("model3d.add_sphere");
  }
  return g_scene_writer.add_sphere();
}

bool handle_add_water(const tool::CommandArgs&) {
  if (!g_scene_writer.add_water) {
    return fail_no_scene("model3d.add_water");
  }
  return g_scene_writer.add_water();
}

bool handle_open_earth(const tool::CommandArgs&) {
  if (!g_scene_writer.open_earth) {
    return fail_no_scene("world3d.open_earth");
  }
  return g_scene_writer.open_earth();
}

bool handle_fly_to(const tool::CommandArgs& args) {
  if (!g_scene_writer.fly_to) {
    return fail_no_scene("world3d.fly_to");
  }
  rapidjson::Document doc;
  if (!parse_scene_args(args.payload, &doc)) {
    set_operation_result("{\"error\":\"bad_args\",\"command\":\"world3d.fly_to\"}");
    return false;
  }
  double lon = 0.0;
  double lat = 0.0;
  if (!scene_json_get_double(doc, "lon", &lon) ||
      !scene_json_get_double(doc, "lat", &lat)) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"command\":\"world3d.fly_to\","
        "\"need\":\"lon,lat\"}");
    return false;
  }
  double distance = 1.6;
  double span_deg = 4.0;
  (void)scene_json_get_double(doc, "distance", &distance);
  (void)scene_json_get_double(doc, "span_deg", &span_deg);
  return g_scene_writer.fly_to(lon, lat, static_cast<float>(distance),
                               span_deg);
}

bool handle_attach_city_tileset(const tool::CommandArgs& args) {
  if (!g_scene_writer.attach_tileset) {
    return fail_no_scene("world3d.attach_city_tileset");
  }
  rapidjson::Document doc;
  if (!parse_scene_args(args.payload, &doc)) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"command\":\"world3d.attach_city_tileset\"}");
    return false;
  }
  std::string path;
  (void)scene_json_get_string(doc, "path", &path);
  return g_scene_writer.attach_tileset(path);
}

bool handle_add_terrain_grid(const tool::CommandArgs&) {
  if (!g_scene_writer.add_terrain_grid) {
    return fail_no_scene("model3d.add_terrain_grid");
  }
  return g_scene_writer.add_terrain_grid();
}

bool handle_add_terrain_tin(const tool::CommandArgs&) {
  if (!g_scene_writer.add_terrain_tin) {
    return fail_no_scene("model3d.add_terrain_tin");
  }
  return g_scene_writer.add_terrain_tin();
}

bool handle_create_tin(const tool::CommandArgs&) {
  if (!g_scene_writer.create_tin_from_active_layer) {
    return fail_no_scene("model3d.create_tin");
  }
  if (g_scene_writer.create_tin_from_active_layer()) {
    ui::views::show_message_box(ui::views::MessageBoxKind::kInfo,
                                "TIN created successfully.");
    return true;
  }
  return fail_no_scene("model3d.create_tin");
}

bool handle_layer_points_to_3d(const tool::CommandArgs&) {
  if (!g_scene_writer.layer_points_to_3d) {
    return fail_no_scene("model3d.layer_points_to_3d");
  }
  if (g_scene_writer.layer_points_to_3d()) {
    return true;
  }
  ui::views::show_message_box(ui::views::MessageBoxKind::kInfo,
                              "Please activate a point layer.");
  return false;
}

bool handle_layer_lines_to_3d(const tool::CommandArgs&) {
  if (!g_scene_writer.layer_lines_to_3d) {
    return fail_no_scene("model3d.layer_lines_to_3d");
  }
  if (g_scene_writer.layer_lines_to_3d()) {
    return true;
  }
  ui::views::show_message_box(ui::views::MessageBoxKind::kInfo,
                              "Please activate a line layer.");
  return false;
}

bool handle_layer_polygons_to_3d(const tool::CommandArgs&) {
  if (!g_scene_writer.layer_polygons_to_3d) {
    return fail_no_scene("model3d.layer_polygons_to_3d");
  }
  if (g_scene_writer.layer_polygons_to_3d()) {
    return true;
  }
  ui::views::show_message_box(ui::views::MessageBoxKind::kInfo,
                              "Please activate a polygon layer.");
  return false;
}

bool process_add_pointcloud(content::PluginHost*, std::string_view args_json) {
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
  if (!g_scene_writer.add_pointcloud) {
    return fail_no_scene_processing("model3d.add_pointcloud");
  }
  if (!g_scene_writer.add_pointcloud(path)) {
    set_operation_result(
        "{\"error\":\"add_failed\",\"op\":\"model3d.add_pointcloud\"}");
    return false;
  }
  set_operation_result("{\"ok\":true,\"op\":\"model3d.add_pointcloud\"}");
  return true;
}

bool attach_pointcloud_result(const gis::PointCloud& cloud, const char* op) {
  if (g_scene_writer.add_pointcloud_xyz) {
    const uint8_t* rgba = cloud.has_color() ? cloud.rgba.data() : nullptr;
    if (!g_scene_writer.add_pointcloud_xyz(
            cloud.xyz.data(), static_cast<int>(cloud.point_count()), rgba)) {
      set_operation_result(std::string("{\"error\":\"add_failed\",\"op\":\"") +
                           op + "\"}");
      return false;
    }
  } else if (g_scene_writer.add_pointcloud && !cloud.source_path.empty()) {
    if (!g_scene_writer.add_pointcloud(cloud.source_path)) {
      set_operation_result(std::string("{\"error\":\"add_failed\",\"op\":\"") +
                           op + "\"}");
      return false;
    }
  } else if (!g_scene_writer.add_pointcloud_xyz && !g_scene_writer.add_pointcloud) {
    return fail_no_scene_processing(op);
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
  w.Bool(gis::pdal_is_available());
  w.EndObject();
  set_operation_result(buf.GetString());
  return true;
}

bool process_pdal_read(content::PluginHost*, std::string_view args_json) {
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
  gis::PdalReadOptions opts;
  if (args.HasMember("z_min") && args["z_min"].IsNumber() &&
      args.HasMember("z_max") && args["z_max"].IsNumber()) {
    opts.has_z_range = true;
    opts.z_min = args["z_min"].GetDouble();
    opts.z_max = args["z_max"].GetDouble();
  }
  if (args.HasMember("max_points") && args["max_points"].IsUint64()) {
    opts.max_points = static_cast<size_t>(args["max_points"].GetUint64());
  }
  gis::PointCloud cloud;
  if (!gis::run_pdal_read(path.c_str(), opts, &cloud)) {
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
    w.Bool(gis::pdal_is_available());
    w.EndObject();
    set_operation_result(buf.GetString());
    return true;
  }
  return attach_pointcloud_result(cloud, "world3d.pdal_read");
}

bool process_pdal_pipeline(content::PluginHost*, std::string_view args_json) {
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
  gis::PointCloud cloud;
  if (!gis::run_pdal_pipeline_json(pipeline_json, &cloud)) {
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
    w.Bool(gis::pdal_is_available());
    w.EndObject();
    set_operation_result(buf.GetString());
    return true;
  }
  return attach_pointcloud_result(cloud, "world3d.pdal_pipeline");
}

bool process_noop_op(content::PluginHost*,
                     std::string_view /*args_json*/,
                     const char* op,
                     const std::function<bool()>& fn) {
  if (!fn) {
    return fail_no_scene_processing(op);
  }
  if (!fn()) {
    set_operation_result(std::string("{\"error\":\"add_failed\",\"op\":\"") + op +
                         "\"}");
    return false;
  }
  set_operation_result(std::string("{\"ok\":true,\"op\":\"") + op + "\"}");
  return true;
}

bool process_add_sphere(content::PluginHost* host, std::string_view args_json) {
  return process_noop_op(host, args_json, "model3d.add_sphere",
                         g_scene_writer.add_sphere);
}

bool process_add_water(content::PluginHost* host, std::string_view args_json) {
  return process_noop_op(host, args_json, "model3d.add_water",
                         g_scene_writer.add_water);
}

bool process_add_terrain_grid(content::PluginHost* host,
                              std::string_view args_json) {
  return process_noop_op(host, args_json, "model3d.add_terrain_grid",
                         g_scene_writer.add_terrain_grid);
}

bool process_add_terrain_tin(content::PluginHost* host,
                             std::string_view args_json) {
  return process_noop_op(host, args_json, "model3d.add_terrain_tin",
                         g_scene_writer.add_terrain_tin);
}

bool process_create_tin(content::PluginHost* host, std::string_view args_json) {
  return process_noop_op(host, args_json, "model3d.create_tin",
                         g_scene_writer.create_tin_from_active_layer);
}

bool process_layer_points_to_3d(content::PluginHost* host,
                                std::string_view args_json) {
  return process_noop_op(host, args_json, "model3d.layer_points_to_3d",
                         g_scene_writer.layer_points_to_3d);
}

bool process_layer_lines_to_3d(content::PluginHost* host,
                               std::string_view args_json) {
  return process_noop_op(host, args_json, "model3d.layer_lines_to_3d",
                         g_scene_writer.layer_lines_to_3d);
}

bool process_layer_polygons_to_3d(content::PluginHost* host,
                                  std::string_view args_json) {
  return process_noop_op(host, args_json, "model3d.layer_polygons_to_3d",
                         g_scene_writer.layer_polygons_to_3d);
}

bool process_open_earth(content::PluginHost*, std::string_view /*args_json*/) {
  if (!g_scene_writer.open_earth) {
    return fail_no_scene_processing("world3d.open_earth");
  }
  if (!g_scene_writer.open_earth()) {
    set_operation_result("{\"error\":\"open_failed\",\"op\":\"world3d.open_earth\"}");
    return false;
  }
  set_operation_result("{\"ok\":true,\"op\":\"world3d.open_earth\"}");
  return true;
}

bool process_fly_to(content::PluginHost*, std::string_view args_json) {
  if (!g_scene_writer.fly_to) {
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
  if (!g_scene_writer.fly_to(lon, lat, static_cast<float>(distance),
                             span_deg)) {
    set_operation_result("{\"error\":\"fly_failed\",\"op\":\"world3d.fly_to\"}");
    return false;
  }
  set_operation_result("{\"ok\":true,\"op\":\"world3d.fly_to\"}");
  return true;
}

bool process_attach_city_tileset(content::PluginHost*,
                                 std::string_view args_json) {
  if (!g_scene_writer.attach_tileset) {
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
  if (!g_scene_writer.attach_tileset(path)) {
    set_operation_result(
        "{\"error\":\"attach_failed\",\"op\":\"world3d.attach_city_tileset\"}");
    return false;
  }
  set_operation_result("{\"ok\":true,\"op\":\"world3d.attach_city_tileset\"}");
  return true;
}

bool contribute_scene_cmd(content::PluginHost* host,
                          std::string_view command_id,
                          std::string_view title,
                          tool::CommandHandler handler) {
  if (!host || command_id.empty() || !handler) {
    return false;
  }
  return host->contribute_command(kPluginId, command_id, title, "tools",
                                  std::move(handler));
}

}  // namespace

void set_world3d_scene_writer(World3dSceneWriter writer) {
  set_world3d_scene_writer_store(std::move(writer));
}

bool register_world3d_scene_ops(content::PluginHost* host) {
  if (!host) {
    return false;
  }

  // Keep model3d.* catalog ids for leftover AM / menus; owned by world3d.
  bool ok = true;
  ok = contribute_scene_cmd(host, "model3d.add_pointcloud", "Add point cloud",
                            handle_add_pointcloud) &&
       ok;
  ok = contribute_scene_cmd(host, "world3d.add_pointcloud", "加载点云",
                            handle_add_pointcloud) &&
       ok;
  ok = contribute_scene_cmd(host, "model3d.add_sphere", "Add sphere",
                            handle_add_sphere) &&
       ok;
  ok = contribute_scene_cmd(host, "model3d.add_water", "Add water surface",
                            handle_add_water) &&
       ok;
  ok = contribute_scene_cmd(host, "model3d.add_terrain_grid", "Add terrain GRID",
                            handle_add_terrain_grid) &&
       ok;
  ok = contribute_scene_cmd(host, "model3d.add_terrain_tin", "Add terrain TIN",
                            handle_add_terrain_tin) &&
       ok;
  ok = contribute_scene_cmd(host, "model3d.create_tin", "Create TIN",
                            handle_create_tin) &&
       ok;
  ok = contribute_scene_cmd(host, "model3d.layer_points_to_3d", "2D points to 3D",
                            handle_layer_points_to_3d) &&
       ok;
  ok = contribute_scene_cmd(host, "model3d.layer_lines_to_3d", "2D lines to 3D",
                            handle_layer_lines_to_3d) &&
       ok;
  ok = contribute_scene_cmd(host, "model3d.layer_polygons_to_3d",
                            "2D polygons to 3D", handle_layer_polygons_to_3d) &&
       ok;
  ok = contribute_scene_cmd(host, "world3d.open_earth", "打开真三维地球",
                            handle_open_earth) &&
       ok;
  ok = contribute_scene_cmd(host, "world3d.fly_to", "飞行到", handle_fly_to) &&
       ok;
  ok = contribute_scene_cmd(host, "world3d.attach_city_tileset", "挂载城市瓦片",
                            handle_attach_city_tileset) &&
       ok;

  ok = host->contribute_processing(
           kPluginId, {"model3d.add_pointcloud", "Add point cloud"},
           process_add_pointcloud) &&
       ok;
  ok = host->contribute_processing(
           kPluginId, {"world3d.add_pointcloud", "加载点云"},
           process_add_pointcloud) &&
       ok;
  ok = host->contribute_processing(
           kPluginId, {"world3d.pdal_read", "PDAL read LAS/LAZ"},
           process_pdal_read) &&
       ok;
  ok = host->contribute_processing(
           kPluginId, {"world3d.pdal_pipeline", "PDAL pipeline JSON"},
           process_pdal_pipeline) &&
       ok;
  ok = host->contribute_processing(kPluginId, {"model3d.add_sphere", "Add sphere"},
                                   process_add_sphere) &&
       ok;
  ok = host->contribute_processing(
           kPluginId, {"model3d.add_water", "Add water surface"},
           process_add_water) &&
       ok;
  ok = host->contribute_processing(
           kPluginId, {"model3d.add_terrain_grid", "Add terrain GRID"},
           process_add_terrain_grid) &&
       ok;
  ok = host->contribute_processing(
           kPluginId, {"model3d.add_terrain_tin", "Add terrain TIN"},
           process_add_terrain_tin) &&
       ok;
  ok = host->contribute_processing(kPluginId, {"model3d.create_tin", "Create TIN"},
                                   process_create_tin) &&
       ok;
  ok = host->contribute_processing(
           kPluginId, {"model3d.layer_points_to_3d", "2D points to 3D"},
           process_layer_points_to_3d) &&
       ok;
  ok = host->contribute_processing(
           kPluginId, {"model3d.layer_lines_to_3d", "2D lines to 3D"},
           process_layer_lines_to_3d) &&
       ok;
  ok = host->contribute_processing(
           kPluginId, {"model3d.layer_polygons_to_3d", "2D polygons to 3D"},
           process_layer_polygons_to_3d) &&
       ok;
  ok = host->contribute_processing(
           kPluginId, {"world3d.open_earth", "打开真三维地球"},
           process_open_earth) &&
       ok;
  ok = host->contribute_processing(kPluginId, {"world3d.fly_to", "飞行到"},
                                   process_fly_to) &&
       ok;
  ok = host->contribute_processing(
           kPluginId, {"world3d.attach_city_tileset", "挂载城市瓦片"},
           process_attach_city_tileset) &&
       ok;

  return ok;
}

}  // namespace plugin
