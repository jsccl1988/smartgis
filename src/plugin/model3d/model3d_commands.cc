// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/model3d/model3d_commands.h"

#include <string>
#include <string_view>

#include "content/public/plugin_host.h"
#include "tool/command.h"
#include "ui/views/dialogs/file_picker.h"
#include "ui/views/dialogs/message_box.h"

namespace plugin {
namespace {

constexpr const char* kPluginId = "smartgis.model3d";

namespace detail {

// Legacy scene types (SmtSceneMgr, render device) are not linked into model3d_views.
// Hooks return null / false so handlers stay safe when no 3D scene is attached.
void* scene_render_device() { return nullptr; }

bool add_pointcloud_to_scene(const std::string& path, void* device) {
  (void)path;
  (void)device;
  return false;
}

bool add_sphere_to_scene(void* device) {
  (void)device;
  return false;
}

bool add_water_to_scene(void* device) {
  (void)device;
  return false;
}

bool add_terrain_grid_to_scene(void* device) {
  (void)device;
  return false;
}

bool add_terrain_tin_to_scene(void* device) {
  (void)device;
  return false;
}

bool create_tin_from_active_layer() { return false; }

bool layer_points_to_3d(void* device) {
  (void)device;
  return false;
}

bool layer_lines_to_3d(void* device) {
  (void)device;
  return false;
}

bool layer_polygons_to_3d(void* device) {
  (void)device;
  return false;
}

}  // namespace detail

bool handle_add_pointcloud(const tool::CommandArgs&) {
  constexpr wchar_t kFilter[] =
      L"Data Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
  const ui::views::FilePickerResult pick =
      ui::views::pick_open_file(kFilter);
  if (!pick.accepted || pick.path.empty()) {
    return false;
  }

  void* device = detail::scene_render_device();
  if (!device) {
    // No 3D scene / render device attached on the Views path yet.
    return false;
  }
  return detail::add_pointcloud_to_scene(pick.path, device);
}

bool handle_add_sphere(const tool::CommandArgs&) {
  void* device = detail::scene_render_device();
  if (!device) {
    return false;
  }
  return detail::add_sphere_to_scene(device);
}

bool handle_add_water(const tool::CommandArgs&) {
  void* device = detail::scene_render_device();
  if (!device) {
    return false;
  }
  return detail::add_water_to_scene(device);
}

bool handle_add_terrain_grid(const tool::CommandArgs&) {
  void* device = detail::scene_render_device();
  if (!device) {
    return false;
  }
  return detail::add_terrain_grid_to_scene(device);
}

bool handle_add_terrain_tin(const tool::CommandArgs&) {
  void* device = detail::scene_render_device();
  if (!device) {
    return false;
  }
  return detail::add_terrain_tin_to_scene(device);
}

bool handle_create_tin(const tool::CommandArgs&) {
  if (detail::create_tin_from_active_layer()) {
    ui::views::show_message_box(ui::views::MessageBoxKind::kInfo,
                                "TIN created successfully.");
    return true;
  }
  return false;
}

bool handle_layer_points_to_3d(const tool::CommandArgs&) {
  void* device = detail::scene_render_device();
  if (!device) {
    return false;
  }
  if (detail::layer_points_to_3d(device)) {
    return true;
  }
  ui::views::show_message_box(ui::views::MessageBoxKind::kInfo,
                              "Please activate a point layer.");
  return false;
}

bool handle_layer_lines_to_3d(const tool::CommandArgs&) {
  void* device = detail::scene_render_device();
  if (!device) {
    return false;
  }
  if (detail::layer_lines_to_3d(device)) {
    return true;
  }
  ui::views::show_message_box(ui::views::MessageBoxKind::kInfo,
                              "Please activate a line layer.");
  return false;
}

bool handle_layer_polygons_to_3d(const tool::CommandArgs&) {
  void* device = detail::scene_render_device();
  if (!device) {
    return false;
  }
  if (detail::layer_polygons_to_3d(device)) {
    return true;
  }
  ui::views::show_message_box(ui::views::MessageBoxKind::kInfo,
                              "Please activate a polygon layer.");
  return false;
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
  return ok;
}

}  // namespace plugin
