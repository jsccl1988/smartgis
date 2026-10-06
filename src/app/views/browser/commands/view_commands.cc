// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/browser/commands/view_commands.h"

#include <iterator>

namespace app {
namespace {

constexpr ViewCommandId kNavigationCommands[] = {
    {"Pan", "view.pan"},
    {"Zoom in", "view.zoom_in"},
    {"Zoom out", "view.zoom_out"},
    {"Zoom full", "view.full"},
    {"Zoom to layer", "view.zoom_layer"},
    {"Zoom to selection", "view.zoom_selection"},
    {"Previous extent", "view.extent_prev"},
    {"Next extent", "view.extent_next"},
    {"Identify", "view.identify"},
    {"Add bookmark", "view.bookmark_add"},
};

}  // namespace

std::span<const ViewCommandId> navigation_commands() {
  return kNavigationCommands;
}

std::vector<std::string> navigation_command_ids(
    const std::vector<std::string>& bookmark_labels) {
  std::vector<std::string> ids;
  ids.reserve(std::size(kNavigationCommands) + bookmark_labels.size());
  for (const ViewCommandId& row : kNavigationCommands) {
    ids.emplace_back(row.id);
  }
  for (size_t i = 0; i < bookmark_labels.size(); ++i) {
    ids.emplace_back("view.bookmark_go");
  }
  return ids;
}

std::vector<ui::views::MenuItem> navigation_menu_items(
    const std::vector<std::string>& bookmark_labels,
    const std::function<void(std::string_view id, int bookmark_index)>&
        invoke) {
  std::vector<ui::views::MenuItem> items;
  items.reserve(std::size(kNavigationCommands) + bookmark_labels.size());
  for (const ViewCommandId& row : kNavigationCommands) {
    ui::views::MenuItem item;
    item.label = row.label;
    item.enabled = true;
    const std::string id = row.id;
    item.invoke = [invoke, id]() {
      if (invoke) {
        invoke(id, -1);
      }
    };
    items.push_back(std::move(item));
  }
  for (size_t i = 0; i < bookmark_labels.size(); ++i) {
    ui::views::MenuItem item;
    item.label = bookmark_labels[i];
    item.enabled = true;
    const int index = static_cast<int>(i);
    item.invoke = [invoke, index]() {
      if (invoke) {
        invoke("view.bookmark_go", index);
      }
    };
    items.push_back(std::move(item));
  }
  return items;
}

namespace {

ui::views::MenuItem command_item(
    std::string label, std::string id,
    const std::function<void(std::string_view id, int bookmark_index)>&
        invoke) {
  ui::views::MenuItem item;
  item.label = std::move(label);
  item.enabled = true;
  item.invoke = [invoke, id = std::move(id)]() {
    if (invoke) {
      invoke(id, -1);
    }
  };
  return item;
}

struct ShellRow {
  const char* menu;
  const char* label;
  const char* id;
};

constexpr ShellRow kFileEditLayer[] = {
    {"File", "Open", "shell.open"},
    {"File", "Save", "shell.save"},
    {"File", "Export", "shell.export"},
    {"File", "Exit", "shell.exit"},
    {"Edit", "Undo", "edit.undo"},
    {"Edit", "Redo", "edit.redo"},
    {"Edit", "Clear selection", "selection.clear"},
    {"Layer", "Create layer", "catalog.layer.create"},
    {"Layer", "Add basemap", "catalog.layer.add_basemap"},
    {"Layer", "Remove layer", "catalog.layer.remove"},
    {"Layer", "Zoom to layer", "view.zoom_layer"},
};

constexpr ShellRow kViewTail[] = {
    {"View", "Refresh", "view.refresh"},
    {"View", "Theme: Dark", "view.theme.dark"},
    {"View", "Theme: Light", "view.theme.light"},
    {"View", "Preferences…", "view.preferences"},
    {"View", "Toggle Diagnostic Tools", "view.debug_console"},
    {"View", "Engine: FlyCube/DX12", "view.engine.flycube"},
    {"View", "Engine: Stereo/GL", "view.engine.stereo_gl"},
    {"View", "Engine: GDI", "view.engine.gdi"},
    {"View", "RHI", "view.backend.rhi"},
    {"View", "MapLibre", "view.backend.maplibre"},
};

void append_menu(std::string_view menu, const ShellRow& row,
                 ShellMenus* out,
                 const std::function<void(std::string_view, int)>& invoke) {
  std::vector<ui::views::MenuItem>* dest = nullptr;
  if (menu == "File") {
    dest = &out->file;
  } else if (menu == "Edit") {
    dest = &out->edit;
  } else if (menu == "Layer") {
    dest = &out->layer;
  }
  if (!dest) {
    return;
  }
  dest->push_back(command_item(row.label, row.id, invoke));
}

}  // namespace

ShellMenus build_shell_menus(
    const std::vector<std::string>& bookmark_labels,
    const std::function<void(std::string_view id, int bookmark_index)>&
        invoke) {
  ShellMenus menus;
  for (const ShellRow& row : kFileEditLayer) {
    append_menu(row.menu, row, &menus, invoke);
  }
  menus.view = navigation_menu_items(bookmark_labels, invoke);
  ui::views::MenuItem separator;
  separator.separator = true;
  menus.view.push_back(separator);
  for (const ShellRow& row : kViewTail) {
    menus.view.push_back(command_item(row.label, row.id, invoke));
  }
  return menus;
}

}  // namespace app
