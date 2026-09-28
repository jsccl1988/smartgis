// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/model3d/commands.h"

#include <string>
#include <string_view>

#include "content/public/plugin_host.h"
#include "plugin/runtime/host/operation_result.h"
#include "tool/command/command.h"
#include "ui/views/dialogs/file_picker.h"
#include "ui/views/dialogs/message_box.h"

#include <rapidjson/document.h>

namespace plugin {
namespace {

constexpr const char* kPluginId = "smartgis.model3d";

Model3dSceneWriter g_scene_writer;

void set_model3d_scene_writer_store(Model3dSceneWriter writer) {
  g_scene_writer = std::move(writer);
}

// UI command path: message box + structured operation_result.
bool fail_no_scene(const char* command) {
  set_operation_result(std::string("{\"error\":\"no_scene_device\",\"command\":\"") +
                       command + "\"}");
  ui::views::show_message_box(
      ui::views::MessageBoxKind::kError,
      "No scene render device is attached to the host.");
  return false;
}

// Processing path: structured result only (no Views).
bool fail_no_scene_processing(const char* command) {
  set_operation_result(std::string("{\"error\":\"no_scene_device\",\"command\":\"") +
                       command + "\"}");
  return false;
}

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

bool handle_add_pointcloud(const tool::CommandArgs&) {
  constexpr wchar_t kFilter[] =
      L"Data Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
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
  if (!parse_args(args_json, &args)) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"model3d.add_pointcloud\"}");
    return false;
  }
  std::string path;
  if (!json_get_string(args, "path", &path) || path.empty()) {
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

bool contribute(content::PluginHost* host, std::string_view command_id,
                std::string_view title, std::string_view menu_id,
                tool::CommandHandler handler) {
  if (!host || command_id.empty() || !handler) {
    return false;
  }
  return host->contribute_command(kPluginId, command_id, title, menu_id,
                                  std::move(handler));
}

}  // namespace

void set_model3d_scene_writer(Model3dSceneWriter writer) {
  set_model3d_scene_writer_store(std::move(writer));
}

bool register_model3d(content::PluginHost* host) {
  if (!host) {
    return false;
  }

  bool ok = true;
  ok = contribute(host, "model3d.add_pointcloud", "Add point cloud", "tools",
                  handle_add_pointcloud) &&
       ok;
  ok = contribute(host, "model3d.add_sphere", "Add sphere", "tools",
                  handle_add_sphere) &&
       ok;
  ok = contribute(host, "model3d.add_water", "Add water surface", "tools",
                  handle_add_water) &&
       ok;
  ok = contribute(host, "model3d.add_terrain_grid", "Add terrain GRID",
                  "tools", handle_add_terrain_grid) &&
       ok;
  ok = contribute(host, "model3d.add_terrain_tin", "Add terrain TIN", "tools",
                  handle_add_terrain_tin) &&
       ok;
  ok = contribute(host, "model3d.create_tin", "Create TIN", "tools",
                  handle_create_tin) &&
       ok;
  ok = contribute(host, "model3d.layer_points_to_3d", "2D points to 3D",
                  "tools", handle_layer_points_to_3d) &&
       ok;
  ok = contribute(host, "model3d.layer_lines_to_3d", "2D lines to 3D",
                  "tools", handle_layer_lines_to_3d) &&
       ok;
  ok = contribute(host, "model3d.layer_polygons_to_3d", "2D polygons to 3D",
                  "tools", handle_layer_polygons_to_3d) &&
       ok;

  ok = host->contribute_processing(
           kPluginId, {"model3d.add_pointcloud", "Add point cloud"},
           process_add_pointcloud) &&
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

  return ok;
}

}  // namespace plugin
