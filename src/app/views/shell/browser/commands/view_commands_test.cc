// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/browser/commands/view_commands.h"

#include <cstdio>
#include <string>
#include <string_view>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

}  // namespace

int main() {
  const std::span<const app::ViewCommandId> rows = app::navigation_commands();
  const char* kLabels[] = {
      "Pan",
      "Zoom in",
      "Zoom out",
      "Zoom full",
      "Zoom to layer",
      "Zoom to selection",
      "Previous extent",
      "Next extent",
      "Identify",
      "Add bookmark",
  };
  const char* kIds[] = {
      "view.pan",
      "view.zoom_in",
      "view.zoom_out",
      "view.full",
      "view.zoom_layer",
      "view.zoom_selection",
      "view.extent_prev",
      "view.extent_next",
      "view.identify",
      "view.bookmark_add",
  };
  expect(rows.size() == 10, "navigation row count");
  if (rows.size() == 10) {
    for (size_t i = 0; i < rows.size(); ++i) {
      expect(std::string_view(rows[i].label) == kLabels[i], "nav label");
      expect(std::string_view(rows[i].id) == kIds[i], "nav id");
      expect(std::string_view(rows[i].id).find("view.backend.") ==
                 std::string_view::npos,
             "nav table has no backend id");
    }
  }

  const std::vector<std::string> ids =
      app::navigation_command_ids({"North", "South"});
  expect(ids.size() == 12, "bookmark rows appended");
  if (ids.size() == 12) {
    for (size_t i = 0; i < 10; ++i) {
      expect(ids[i] == kIds[i], "id order");
    }
    expect(ids[10] == "view.bookmark_go", "first bookmark id");
    expect(ids[11] == "view.bookmark_go", "second bookmark id");
  }
  for (const std::string& id : ids) {
    expect(id.find("view.backend.") == std::string::npos, "no backend id");
  }

  const std::vector<ui::views::MenuItem> items =
      app::navigation_menu_items({"North", "South"}, {});
  expect(items.size() == 12, "menu item count");
  if (items.size() == 12) {
    expect(items[0].label == "Pan", "menu starts at Pan");
    expect(items[9].label == "Add bookmark", "add bookmark before rows");
    expect(items[10].label == "North", "bookmark label");
    expect(items[11].label == "South", "second bookmark label");
    expect(!items[0].separator, "nav rows are not separators");
  }

  const app::ShellMenus menus = app::build_shell_menus({}, {});
  expect(menus.file.size() == 4, "file menu count");
  expect(menus.edit.size() == 3, "edit menu count");
  expect(menus.layer.size() == 4, "layer menu count");
  expect(menus.view.size() == 14, "view menu is nav plus tail");
  if (menus.file.size() == 4 && menus.view.size() == 14 &&
      menus.layer.size() == 4) {
    expect(menus.file[0].label == "Open", "file starts at Open");
    expect(menus.file[3].label == "Exit", "file ends at Exit");
    expect(menus.view[0].label == "Pan", "view starts at Pan");
    expect(menus.view[10].separator, "view separator after nav");
    expect(menus.view[11].label == "Refresh", "refresh after separator");
    expect(menus.view[12].label == "RHI", "rhi row");
    expect(menus.view[13].label == "MapLibre", "maplibre row");
    expect(menus.layer[3].label == "Zoom to layer", "layer zoom row");
  }

  if (g_fails) {
    std::fprintf(stderr, "%d view_commands_test fail(s)\n", g_fails);
    return 1;
  }
  std::printf("view_commands_test ok\n");
  return 0;
}
