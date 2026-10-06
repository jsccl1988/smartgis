// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scene/model/register.h"

#include "content/public/gis_document.h"
#include "content/public/plugin_host.h"
#include "plugin/product/world3d/scene/detail/contribute.h"
#include "plugin/product/world3d/scene/detail/host.h"
#include "plugin/product/world3d/scene/present/standin.h"
#include "plugin/product/world3d/scene/present/style.h"
#include "plugin/runtime/host/processing/operation_result.h"
#include "tool/command/command.h"
#include "ui/views/dialogs/message_box.h"

#include <string>
#include <string_view>

namespace plugin {
namespace {

content::PluginHost* g_host = nullptr;

bool present_standin(content::PluginHost* host, const char* name, double lon,
                     double lat, double half_deg, const char* op) {
  if (!host || !host->gis_document()) {
    return detail::fail_no_scene_processing(op);
  }
  if (!present_world3d_standin_mesh(host->gis_document(),
                                    detail::world3d_scene_sink(host), name, lon,
                                    lat, half_deg)) {
    set_operation_result(std::string("{\"error\":\"add_failed\",\"op\":\"") +
                         op + "\"}");
    return false;
  }
  (void)apply_world3d_mesh_style(host->gis_document());
  (void)host->present_dataset(detail::kWorld3dPluginId, "", 1);
  set_operation_result(std::string("{\"ok\":true,\"op\":\"") + op + "\"}");
  return true;
}

bool present_active_layer(content::PluginHost* host, const char* op) {
  if (!host || !host->gis_document()) {
    return detail::fail_no_scene_processing(op);
  }
  if (host->gis_document()->feature_count() == 0) {
    set_operation_result(std::string("{\"error\":\"add_failed\",\"op\":\"") +
                         op + "\"}");
    return false;
  }
  (void)host->present_dataset(detail::kWorld3dPluginId, "", 1);
  set_operation_result(std::string("{\"ok\":true,\"op\":\"") + op + "\"}");
  return true;
}

bool process_add_sphere(content::PluginHost* host, std::string_view) {
  return present_standin(host, "Sphere", 105.0, 35.0, 0.4,
                         "model3d.add_sphere");
}

bool process_add_water(content::PluginHost* host, std::string_view) {
  return present_standin(host, "Water", 110.0, 30.0, 0.6, "model3d.add_water");
}

bool process_add_terrain_heightmap(content::PluginHost* host, std::string_view) {
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

bool handle_add_sphere(const tool::CommandArgs&) {
  if (!g_host || !g_host->gis_document()) {
    return detail::fail_no_scene("model3d.add_sphere");
  }
  return process_add_sphere(g_host, {});
}

bool handle_add_water(const tool::CommandArgs&) {
  if (!g_host || !g_host->gis_document()) {
    return detail::fail_no_scene("model3d.add_water");
  }
  return process_add_water(g_host, {});
}

bool handle_add_terrain_heightmap(const tool::CommandArgs&) {
  if (!g_host || !g_host->gis_document()) {
    return detail::fail_no_scene("model3d.add_terrain_heightmap");
  }
  return process_add_terrain_heightmap(g_host, {});
}

bool handle_add_terrain_trimesh(const tool::CommandArgs&) {
  if (!g_host || !g_host->gis_document()) {
    return detail::fail_no_scene("model3d.add_terrain_trimesh");
  }
  return process_add_terrain_trimesh(g_host, {});
}

bool handle_create_trimesh(const tool::CommandArgs&) {
  if (!g_host || !g_host->gis_document()) {
    return detail::fail_no_scene("model3d.create_trimesh");
  }
  if (process_create_trimesh(g_host, {})) {
    ui::views::show_message_box(ui::views::MessageBoxKind::kInfo,
                                "Trimesh created successfully.");
    return true;
  }
  return detail::fail_no_scene("model3d.create_trimesh");
}

bool handle_layer_points_to_3d(const tool::CommandArgs&) {
  if (!g_host || !g_host->gis_document()) {
    return detail::fail_no_scene("model3d.layer_points_to_3d");
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
    return detail::fail_no_scene("model3d.layer_lines_to_3d");
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
    return detail::fail_no_scene("model3d.layer_polygons_to_3d");
  }
  if (process_layer_polygons_to_3d(g_host, {})) {
    return true;
  }
  ui::views::show_message_box(ui::views::MessageBoxKind::kInfo,
                              "Please activate a polygon layer.");
  return false;
}

}  // namespace

namespace detail {

bool register_world3d_model(content::PluginHost* host) {
  if (!host) {
    return false;
  }
  g_host = host;
  return contribute_command_aliases(host, {{"model3d.add_sphere", "Add sphere"}},
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
             process_layer_polygons_to_3d);
}

}  // namespace detail
}  // namespace plugin
